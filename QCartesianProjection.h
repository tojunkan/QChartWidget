#pragma once
// QCartesianProjection.h
#pragma once
#include "QChartProjection.h"

class QCartesianProjection : public QChartProjection {
public:
    CoordinateSystem type() const override { return CoordinateSystem::Cartesian; }

    QPointF mapToPixel(qreal normX, qreal normY, const QRectF& plotArea) const override {
        return QPointF(
            plotArea.left() + normX * plotArea.width(),
            plotArea.top() + (1.0 - normY) * plotArea.height()
        );
    }

    QPointF mapToNormalized(const QPointF& pixel, const QRectF& plotArea) const override {
        return QPointF(
            (pixel.x() - plotArea.left()) / plotArea.width(),
            (plotArea.bottom() - pixel.y()) / plotArea.height()
        );
    }
};