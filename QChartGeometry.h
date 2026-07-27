#ifndef QCHARTGEOMETRY_H
#define QCHARTGEOMETRY_H
#include <QObject>
#include <QList>
#include <QRectF>
#include <QPointF>
#include <QPainter>
#include <QColor>
#include "QChartAxis.h"

class QChartSeries;
class QChartWidget;

class QChartGeometry : public QObject
{
    Q_OBJECT
        Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible NOTIFY gridChanged)
        Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY gridChanged)
public:
    explicit QChartGeometry(QObject* parent=nullptr);
    ~QChartGeometry() override;

    virtual CoordinateSystem coordinateSystem() const = 0;

    QChartAxis* axisX() const { return m_axisX; }
    QChartAxis* axisY() const { return m_axisY; }
    void setAxisX(QChartAxis* a);
    void setAxisY(QChartAxis* a);

    // plotArea 从 ChartWidget 读（layout 阶段设置）
    QRectF plotArea() const;

    // grid 归 Geometry 管
    bool isGridVisible() const { return m_gridVisible; }
    void setGridVisible(bool v) { 
        if (m_gridVisible == v)return;
        m_gridVisible=v; 
        emit gridChanged();
    }
    QColor gridColor() const { return m_gridColor; }
    void setGridColor(const QColor& c) { 
        if (m_gridColor == c)return;
        m_gridColor=c; 
        emit gridChanged();
    }
    virtual void drawGrid(QPainter* p, QChartProjection* projection) const;

    virtual QPointF mapToPixel(qreal x, qreal y) const = 0;
    virtual QPointF mapFromPixel(const QPointF& p) const = 0;

    // Series
    void addSeries(QChartSeries* s);
    void removeSeries(QChartSeries* s);
    QList<QChartSeries*> seriesList() const { return m_series; }
    void clearSeries();
    void drawAllSeries(QPainter* p, QChartProjection* projection);
    virtual void drawSeries(QPainter* p, QChartSeries* s, QChartProjection* projection);
    virtual int hitTestSeries(QChartSeries* s, const QPointF& pixelPos) const;
    QPair<QChartSeries*,int> hitTest(const QPointF& pixelPos) const;

signals:
    void seriesAdded(QChartSeries*);
    void seriesRemoved(QChartSeries*);
    void gridChanged();

protected:
    QChartAxis *m_axisX=nullptr, *m_axisY=nullptr;
    QList<QChartSeries*> m_series;
    bool m_gridVisible=true;
    QColor m_gridColor=QColor(220,220,220);
};

#endif //!QCHARTGEOMETRY_H
