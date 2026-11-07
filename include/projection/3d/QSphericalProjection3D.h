// QSphericalProjection3D.h —— 3D 球坐标投影
// ★ 维度约定（与其它投影统一）：**角度维在前、径向维在最后**（"面外方向"永远是最后一维）。
//   dim0 = θ（方位角，度），dim1 = φ（仰角，度），dim2 = r（半径）
//   2D 极坐标是 (θ, r)，本类就是它的三维版（多一个 φ）。
// toWorld(θ°, φ°, r):
//   x = r·cosφ·cosθ,  y = r·cosφ·sinθ,  z = r·sinφ
// fromWorld(x, y, z):
//   θ = atan2(y,x) ∈ [0°,360°)；φ = asin(z/r) ∈ [-90°,90°]；r = √(x²+y²+z²)
//   奇点：r=0 处 θ/φ 均无定义 → (NaN, NaN, 0)
#pragma once
#include "QChartProjection3D.h"
#include <QtMath>

class QSphericalProjection3D : public QChartProjection3D {
public:
    QSphericalProjection3D() : QChartProjection3D("θ", "φ", "r") {}

    CoordinateSystem type() const override { return CoordinateSystem::Spherical; }

    // ── Numeric → World ──
    QVector3D toCartesian(qreal n0, qreal n1, qreal n2 = 0.0) const override {
        const qreal thRad = qDegreesToRadians(n0);   // dim0 = θ（方位角，度）
        const qreal phRad = qDegreesToRadians(n1);   // dim1 = φ（仰角，度）
        const qreal r = n2;                          // dim2 = r
        const qreal cosPh = qCos(phRad);
        return QVector3D(r * cosPh * qCos(thRad),
                         r * cosPh * qSin(thRad),
                         r * qSin(phRad));
    }

    // ── World → Numeric ──
    QVector3D fromCartesian(const QVector3D& cart) const override {
        const qreal r = qSqrt(cart.x() * cart.x() + cart.y() * cart.y() + cart.z() * cart.z());
        if (qFuzzyIsNull(r)) {
            // 奇点：r=0 → θ/φ 均无定义（r 本身仍为 0）
            return QVector3D(qQNaN(), qQNaN(), 0.0);
        }
        qreal rad = qAtan2(cart.y(), cart.x());    // (-π, π]
        if (rad < 0.0) rad += 2.0 * M_PI;          // [0, 2π)
        const qreal phiDeg = qRadiansToDegrees(qAsin(qBound<qreal>(-1.0, cart.z() / r, 1.0)));
        return QVector3D(qRadiansToDegrees(rad), phiDeg, r);
    }

    // ── 反算：World 盒 → Numeric 盒（闭式，不用采样）──
    //    rMin = 盒到原点的距离（含原点 → 0）；rMax = 最远角点；
    //    θ：盒含 z 轴或跨正 X 轴（0/360 缝）→ [0,360)，否则取四角点角度区间；
    //    φ = asin(z / √(dxy² + z²))，在 zmin / zmax 处取极值（含轴时自然得到 ±90）。
    QCube computeDataBounds(const QCube& w) const override {
        if (!w.isValid()) return w;
        const qreal dx = (w.min.x() <= 0.0 && w.max.x() >= 0.0) ? 0.0
                                                               : qMin(qAbs(w.min.x()), qAbs(w.max.x()));
        const qreal dy = (w.min.y() <= 0.0 && w.max.y() >= 0.0) ? 0.0
                                                               : qMin(qAbs(w.min.y()), qAbs(w.max.y()));
        const qreal dz = (w.min.z() <= 0.0 && w.max.z() >= 0.0) ? 0.0
                                                               : qMin(qAbs(w.min.z()), qAbs(w.max.z()));
        const qreal rx = qMax(qAbs(w.min.x()), qAbs(w.max.x()));
        const qreal ry = qMax(qAbs(w.min.y()), qAbs(w.max.y()));
        const qreal rz = qMax(qAbs(w.min.z()), qAbs(w.max.z()));
        const qreal dxy2 = dx * dx + dy * dy;
        const qreal rMin = qSqrt(dxy2 + dz * dz);
        const qreal rMax = qSqrt(rx * rx + ry * ry + rz * rz);

        const bool containsZAxis = (dx == 0.0 && dy == 0.0);
        const bool crossesPositiveX = (w.min.x() > 0.0 && w.min.y() <= 0.0 && w.max.y() >= 0.0);
        qreal thMin = 0.0, thMax = 360.0;
        if (!containsZAxis && !crossesPositiveX) {
            const qreal corners[4][2] = { {w.min.x(), w.min.y()}, {w.max.x(), w.min.y()},
                                          {w.max.x(), w.max.y()}, {w.min.x(), w.max.y()} };
            thMin = 360.0; thMax = 0.0;
            for (const auto& c : corners) {
                qreal a = qRadiansToDegrees(qAtan2(c[1], c[0]));
                if (a < 0.0) a += 360.0;
                thMin = qMin(thMin, a);
                thMax = qMax(thMax, a);
            }
        }

        auto phiOf = [dxy2](qreal z) -> qreal {
            const qreal den2 = dxy2 + z * z;
            if (den2 <= 0.0) return 0.0;   // dxy=0 且 z=0：约定取 0
            return qRadiansToDegrees(qAsin(qBound<qreal>(-1.0, z / qSqrt(den2), 1.0)));
        };

        return QCube(QVector3D(thMin, phiOf(w.min.z()), rMin),
                     QVector3D(thMax, phiOf(w.max.z()), rMax));
    }

    // 默认域：整圈方位 + 整圈仰角 + 半径 0..10
    std::pair<QVector3D, QVector3D> defaultDataBounds() const override {
        return { QVector3D(0.0, -90.0, 0.0), QVector3D(360.0, 90.0, 10.0) };
    }

    QString glslToCartesian() const override {
        // num.x=θ, num.y=φ, num.z=r
        return R"(
            vec3(num.z * cos(radians(num.y)) * cos(radians(num.x)),
                num.z * cos(radians(num.y)) * sin(radians(num.x)),
                num.z * sin(radians(num.y)))
        )";
    }
    QString glslFromCartesian() const override {
        // 奇点 r=0 → θ=NaN, φ=NaN, r=0
        return R"(
            (length(cart) < 1e-8) ? 
            vec3(0.0/0.0, 0.0/0.0, 0.0) : 
            vec3(degrees(atan(cart.y, cart.x) + 6.28318530718 * float(atan(cart.y, cart.x) < 0.0)),
                degrees(asin(clamp(cart.z / length(cart), -1.0, 1.0))),
                length(cart))
        )";
    }
};
