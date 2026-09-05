// QCylindricalProjection3D.h —— 3D 柱坐标投影
// dim0 = r（半径），dim1 = θ（方位角，度），dim2 = z（高度）
// toWorld(r, θ°, z):  x = r·cos(θ),  y = r·sin(θ),  z = z
// fromWorld(x, y, z): r = √(x²+y²)；θ = atan2(y,x) ∈ [0°,360°)；z = z
//                     奇点：r=0 处 θ 无定义 → 返回 (NaN, 0, z)（与 2D Polar 极点一致）
#pragma once
#include "QChartProjection3D.h"
#include <QtMath>

class QCylindricalProjection3D : public QChartProjection3D {
    Q_OBJECT
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
