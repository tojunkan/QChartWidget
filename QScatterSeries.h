#ifndef QSCATTERSERIES_H
#define QSCATTERSERIES_H

#include "QAbstractSeries.h"
#include <QVector>
#include <QPointF>
#include <QPainterPath>

class QScatterSeries : public QAbstractSeries
{
    Q_OBJECT
public:
    enum MarkerShape { Circle, Rectangle, Triangle, Diamond, Star, Cross };

    explicit QScatterSeries(const QString& name = {}, QObject* parent = nullptr);

    int count() const { return m_points.size(); }
    void append(qreal x, qreal y);
    void clear();
    QPointF at(int i) const;
    void setPoints(const QVector<QPointF>& pts);
    const QVector<QPointF>& points() const { return m_points; }

    MarkerShape markerShape() const { return m_markerShape; }
    void setMarkerShape(MarkerShape s) { m_markerShape = s; }
    qreal markerSize() const { return m_markerSize; }
    void setMarkerSize(qreal s) { m_markerSize = qMax(1.0, s); }

    static QScatterSeries* randomScatter(const QString& name, int n, qreal xMean, qreal xSd, qreal yMean, qreal ySd, QObject* parent = nullptr);

    QRectF boundingRect() const override;

private:
    void drawSingleMarker(QPainter* p, const QPointF& c, qreal size,
                          MarkerShape shape, const QColor& fill, const QColor& border) const;
    QVector<QPointF> m_points;
    MarkerShape m_markerShape = Circle;
    qreal m_markerSize = 8;
};

#endif
