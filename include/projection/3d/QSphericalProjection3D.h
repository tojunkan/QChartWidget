// QSphericalProjection3D.h —— 3D 球坐标投影
// dim0 = r（半径），dim1 = θ（方位角，度），dim2 = φ（仰角，度）
// toWorld(r, θ°, φ°):
//   x = r·cosφ·cosθ,  y = r·cosφ·sinθ,  z = r·sinφ
// fromWorld(x, y, z):
//   r = √(x²+y²+z²)；θ = atan2(y,x) ∈ [0°,360°)；φ = asin(z/r) ∈ [-90°,90°]
//   奇点：r=0 处 θ/φ 均无定义 → (NaN, NaN, 0)
#pragma once
#include "QChartProjection3D.h"
#include <QtMath>

class QSphericalProjection3D : public QChartProjection3D {
    Q_OBJECT
public:
    QSphericalProjection3D() : QChartProjection3D("r", "θ", "φ") {}

    CoordinateSystem type() const override { return CoordinateSystem::Spherical; }
    
    // ── Numeric → World ──
    QVector3D toCartesian(qreal n0, qreal n1, qreal n2 = 0.0) const override {
        const qreal r = n0;            // dim0 = r
        const qreal thRad = qDegreesToRadians(n1); // dim1 = θ（方位角，度）
        const qreal phRad = qDegreesToRadians(n2); // dim2 = φ（仰角，度）
        const qreal cosPh = qCos(phRad);
        return QVector3D(r * cosPh * qCos(thRad),
                         r * cosPh * qSin(thRad),
                         r * qSin(phRad));
    }

    // ── World → Numeric ──
    QVector3D fromCartesian(const QVector3D& cart) const override {
        const qreal r = qSqrt(cart.x() * cart.x() + cart.y() * cart.y() + cart.z() * cart.z());
        if (qFuzzyIsNull(r)) {
            // 奇点：r=0 → θ/φ 均无定义 → NaN
            return QVector3D(qQNaN(), qQNaN(), 0.0);
        }
        qreal rad = qAtan2(cart.y(), cart.x());          // (-π, π]
        if (rad < 0.0) rad += 2.0 * M_PI;          // [0, 2π)
        const qreal phiDeg = qRadiansToDegrees(qAsin(qBound<qreal>(-1.0, cart.z() / r, 1.0)));
        return QVector3D(r, qRadiansToDegrees(rad), phiDeg);
    }

    // QSphericalProjection3D.h 中添加
    QString glslToCartesian() const override {
        // num.x=r, num.y=θ, num.z=φ
        return R"(
            vec3(num.x * cos(radians(num.z)) * cos(radians(num.y)),
                num.x * cos(radians(num.z)) * sin(radians(num.y)),
                num.x * sin(radians(num.z)))
        )";
    }
    QString glslFromCartesian() const override {
        // 奇点 r=0 -> θ=NaN, φ=NaN
        return R"(
            (length(cart) < 1e-8) ? 
            vec3(0.0/0.0, 0.0/0.0, 0.0) : 
            vec3(length(cart), 
                degrees(atan(cart.y, cart.x) + 6.28318530718 * float(atan(cart.y, cart.x) < 0.0)),
                degrees(asin(clamp(cart.z / length(cart), -1.0, 1.0))))
        )";
    }
};
