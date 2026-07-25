#ifndef QLINESERIES_H
#define QLINESERIES_H

#include "QAbstractSeries.h"
#include <QVector>
#include <QPointF>
#include <QPainterPath>

class QLineSeries : public QAbstractSeries
{
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth)
    Q_PROPERTY(bool smooth READ smooth WRITE setSmooth)
    Q_PROPERTY(bool pointsVisible READ pointsVisible WRITE setPointsVisible)

public:
    explicit QLineSeries(const QString& name = {}, QObject* parent = nullptr);

    // 数据
    int count() const { return m_points.size(); }
    void append(qreal x, qreal y);
    void append(const QPointF& p);
    void clear();
    QPointF at(int index) const;
    void setPoints(const QVector<QPointF>& pts);
    const QVector<QPointF>& points() const { return m_points; }

    // 样式
    qreal lineWidth() const { return m_lineWidth; }
    void setLineWidth(qreal w) { m_lineWidth = qMax(0.5, w); }
    bool smooth() const { return m_smooth; }
    void setSmooth(bool s) { m_smooth = s; }
    bool pointsVisible() const { return m_pointsVisible; }
    void setPointsVisible(bool v) { m_pointsVisible = v; }
    qreal pointSize() const { return m_pointSize; }
    void setPointSize(qreal s) { m_pointSize = qMax(1.0, s); }

    // 工厂
    static QLineSeries* sinusoidal(const QString& name, int n, qreal amp, qreal freq, qreal phase, QObject* parent = nullptr);

    // 接口实现
    QRectF boundingRect() const override;

private:
    QPainterPath smoothPath(const QVector<QPointF>& screenPts) const;

    QVector<QPointF> m_points;
    qreal m_lineWidth = 2;
    bool m_smooth = false;
    bool m_pointsVisible = false;
    qreal m_pointSize = 6;
};

#endif
