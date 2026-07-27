#pragma once
// QPolarProjection.h
#pragma once
#include "QChartProjection.h"
#include <QtMath>

class QPolarProjection : public QChartProjection {
public:
    CoordinateSystem type() const override { return CoordinateSystem::Polar; }

    QPointF mapToPixel(qreal normAngle, qreal normRadius, const QRectF& plotArea) const override {
        QPointF center = plotArea.center();
        qreal radius = normRadius * qMin(plotArea.width(), plotArea.height()) / 2.0;
        qreal rad = normAngle * 2.0 * M_PI;
        return QPointF(
            center.x() + radius * qCos(rad),
            center.y() - radius * qSin(rad)
        );
    }

    QPointF mapToNormalized(const QPointF& pixel, const QRectF& plotArea) const override {
        QPointF center = plotArea.center();
        qreal dx = pixel.x() - center.x();
        qreal dy = center.y() - pixel.y();
        qreal maxRadius = qMin(plotArea.width(), plotArea.height()) / 2.0;
        qreal radius = qSqrt(dx * dx + dy * dy);
        qreal normRadius = qBound(0.0, radius / maxRadius, 1.0);
        qreal angle = qAtan2(dy, dx);
        if (angle < 0) angle += 2.0 * M_PI;
        qreal normAngle = angle / (2.0 * M_PI);
        return QPointF(normAngle, normRadius);
    }
};