// QChartSphericalProjection3D.h —— 3D 球坐标投影
// dim0 = r（半径），dim1 = θ（方位角，度），dim2 = φ（仰角，度）
// toWorld(r, θ°, φ°):
//   x = r·cosφ·cosθ,  y = r·cosφ·sinθ,  z = r·sinφ
// fromWorld(x, y, z):
//   r = √(x²+y²+z²)；θ = atan2(y,x) ∈ [0°,360°)；φ = asin(z/r) ∈ [-90°,90°]
//   奇点：r=0 处 θ/φ 均无定义 → (NaN, NaN, 0)
#pragma once
#include "QChartProjection3D.h"
#include <QtMath>

class QChartSphericalProjection3D : public QChartProjection3D {
public:
    QChartSphericalProjection3D() : QChartProjection3D("r", "θ", "φ") {}

    // ── Numeric → World ──
    QVector3D toWorld(qreal n0, qreal n1, qreal n2 = 0.0) const override {
        const qreal r = n0;            // dim0 = r
        const qreal thRad = qDegreesToRadians(n1); // dim1 = θ（方位角，度）
        const qreal phRad = qDegreesToRadians(n2); // dim2 = φ（仰角，度）
        const qreal cosPh = qCos(phRad);
        return QVector3D(r * cosPh * qCos(thRad),
                         r * cosPh * qSin(thRad),
                         r * qSin(phRad));
    }

    // ── World → Numeric ──
    QVector3D fromWorld(const QVector3D& w) const override {
        const qreal r = qSqrt(w.x() * w.x() + w.y() * w.y() + w.z() * w.z());
        if (qFuzzyIsNull(r)) {
            // 奇点：r=0 → θ/φ 均无定义 → NaN
            return QVector3D(qQNaN(), qQNaN(), 0.0);
        }
        qreal rad = qAtan2(w.y(), w.x());          // (-π, π]
        if (rad < 0.0) rad += 2.0 * M_PI;          // [0, 2π)
        const qreal phiDeg = qRadiansToDegrees(qAsin(qBound<qreal>(-1.0, w.z() / r, 1.0)));
        return QVector3D(r, qRadiansToDegrees(rad), phiDeg);
    }
};
