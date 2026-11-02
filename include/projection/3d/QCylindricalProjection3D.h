// QCylindricalProjection3D.h —— 3D 柱坐标投影
// dim0 = r（半径），dim1 = θ（方位角，度），dim2 = z（高度）
// toWorld(r, θ°, z):  x = r·cos(θ),  y = r·sin(θ),  z = z
// fromWorld(x, y, z): r = √(x²+y²)；θ = atan2(y,x) ∈ [0°,360°)；z = z
//                     奇点：r=0 处 θ 无定义 → 返回 (NaN, 0, z)（与 2D Polar 极点一致）
#pragma once
#include "QChartProjection3D.h"
#include <QtMath>

class QCylindricalProjection3D : public QChartProjection3D {
public:
    QCylindricalProjection3D() : QChartProjection3D("r", "θ", "z") {}

    CoordinateSystem type() const override { return CoordinateSystem::Cylindrical; }

    // ── Numeric → World ──
    QVector3D toCartesian(qreal n0, qreal n1, qreal n2 = 0.0) const override {
        const qreal r = n0;            // dim0 = r
        const qreal rad = qDegreesToRadians(n1); // dim1 = θ（度）
        return QVector3D(r * qCos(rad),
                         r * qSin(rad),
                         n2);          // dim2 = z
    }

    // ── World → Numeric ──
    QVector3D fromCartesian(const QVector3D& cart) const override {
        const qreal r = qSqrt(cart.x() * cart.x() + cart.y() * cart.y());
        if (qFuzzyIsNull(r)) {
            // 奇点：r=0 → θ 无定义 → NaN（z 保留）
            return QVector3D(qQNaN(), 0.0, cart.z());
        }
        qreal rad = qAtan2(cart.y(), cart.x());          // (-π, π]
        if (rad < 0.0) rad += 2.0 * M_PI;          // [0, 2π)
        return QVector3D(r, qRadiansToDegrees(rad), cart.z());
    }

    // ── 反算：World 盒 → Numeric 盒（闭式，不用采样）──
    //    rMin = 盒到轴的距离（含轴 → 0）；rMax = 最远角点；z 原样；
    //    θ：盒含轴或跨正 X 轴（0/360 缝）→ [0,360)，否则取四角点角度区间。
    QCube computeDataBounds(const QCube& w) const override {
        if (!w.isValid()) return w;
        const qreal dx = (w.min.x() <= 0.0 && w.max.x() >= 0.0) ? 0.0
                                                               : qMin(qAbs(w.min.x()), qAbs(w.max.x()));
        const qreal dy = (w.min.y() <= 0.0 && w.max.y() >= 0.0) ? 0.0
                                                               : qMin(qAbs(w.min.y()), qAbs(w.max.y()));
        const qreal rx = qMax(qAbs(w.min.x()), qAbs(w.max.x()));
        const qreal ry = qMax(qAbs(w.min.y()), qAbs(w.max.y()));
        const qreal rMin = qSqrt(dx * dx + dy * dy);
        const qreal rMax = qSqrt(rx * rx + ry * ry);

        const bool containsAxis = (dx == 0.0 && dy == 0.0);
        const bool crossesPositiveX = (w.min.x() > 0.0 && w.min.y() <= 0.0 && w.max.y() >= 0.0);
        qreal thMin = 0.0, thMax = 360.0;
        if (!containsAxis && !crossesPositiveX) {
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
        return QCube(QVector3D(rMin, thMin, w.min.z()),
                     QVector3D(rMax, thMax, w.max.z()));
    }

    // QCylindricalProjection3D.h 中添加
    QString glslToCartesian() const override {
        // num.x=r, num.y=θ, num.z=z
        return "vec3(num.x * cos(radians(num.y)), num.x * sin(radians(num.y)), num.z)";
    }
    QString glslFromCartesian() const override {
        // 奇点 r=0 -> θ=NaN, z保留
        return R"(
            (length(cart.xy) < 1e-8) ? 
            vec3(0.0/0.0, 0.0, cart.z) : 
            vec3(length(cart.xy), 
                degrees(atan(cart.y, cart.x) + 6.28318530718 * float(atan(cart.y, cart.x) < 0.0)),
                cart.z)
        )";
    }
};
