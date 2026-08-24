// QChartCylindricalProjection3D.h —— 3D 柱坐标投影
// dim0 = r（半径），dim1 = θ（方位角，度），dim2 = z（高度）
// toWorld(r, θ°, z):  x = r·cos(θ),  y = r·sin(θ),  z = z
// fromWorld(x, y, z): r = √(x²+y²)；θ = atan2(y,x) ∈ [0°,360°)；z = z
//                     奇点：r=0 处 θ 无定义 → 返回 (NaN, 0, z)（与 2D Polar 极点一致）
#pragma once
#include "QChartProjection3D.h"
#include <QtMath>

class QChartCylindricalProjection3D : public QChartProjection3D {
public:
    QChartCylindricalProjection3D() : QChartProjection3D("r", "θ", "z") {}

    // ── Numeric → World ──
    QVector3D toWorld(qreal n0, qreal n1, qreal n2 = 0.0) const override {
        const qreal r = n0;            // dim0 = r
        const qreal rad = qDegreesToRadians(n1); // dim1 = θ（度）
        return QVector3D(r * qCos(rad),
                         r * qSin(rad),
                         n2);          // dim2 = z
    }

    // ── World → Numeric ──
    QVector3D fromWorld(const QVector3D& w) const override {
        const qreal r = qSqrt(w.x() * w.x() + w.y() * w.y());
        if (qFuzzyIsNull(r)) {
            // 奇点：r=0 → θ 无定义 → NaN（z 保留）
            return QVector3D(qQNaN(), 0.0, w.z());
        }
        qreal rad = qAtan2(w.y(), w.x());          // (-π, π]
        if (rad < 0.0) rad += 2.0 * M_PI;          // [0, 2π)
        return QVector3D(r, qRadiansToDegrees(rad), w.z());
    }
};
