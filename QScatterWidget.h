#ifndef QSCATTERWIDGET_H
#define QSCATTERWIDGET_H

#include <QWidget>
#include <QList>
#include <QPair>
#include "QChartAxis.h"
#include "QXYSeries.h"

class QScatterWidget : public QWidget
{
    Q_OBJECT
public:
    enum MarkerShape { Circle, Rectangle, Triangle, Diamond, Star, Cross };

    explicit QScatterWidget(QWidget* parent = nullptr);
    ~QScatterWidget() override;

    // 数据
    void addSeries(QXYSeries* series);
    void removeSeries(QXYSeries* series);
    void clear();
    QList<QXYSeries*> seriesList() const { return m_series; }

    // 轴
    void setAxisX(QChartAxis* axis);
    QChartAxis* axisX() const { return m_axisX; }
    void setAxisY(QChartAxis* axis);
    QChartAxis* axisY() const { return m_axisY; }

    // 动画
    void setValuesMultiplier(qreal v);
    qreal valuesMultiplier() const { return m_valuesMultiplier; }

    // 样式
    void setMarkerShape(MarkerShape s) { m_markerShape = s; update(); }
    MarkerShape markerShape() const { return m_markerShape; }
    void setMarkerSize(qreal px) { m_markerSize = qMax(1.0, px); update(); }
    qreal markerSize() const { return m_markerSize; }

    // 交互
    QPair<int,int> pointAtPos(const QPointF& pos) const;

signals:
    void pointClicked(int seriesIdx, int pointIdx);
    void pointHovered(int seriesIdx, int pointIdx, bool entered);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    void drawAxes(QPainter* p, const QRectF& area);
    void drawMarkers(QPainter* p, const QRectF& area);
    void drawSingleMarker(QPainter* p, const QPointF& center, qreal size,
                          MarkerShape shape, const QColor& fill, const QColor& border);
    QRectF plotArea() const;
    QColor borderColorFor(const QColor& fill) const;

    QList<QXYSeries*> m_series;
    QValueAxis m_defaultAxisX, m_defaultAxisY;
    QChartAxis* m_axisX = &m_defaultAxisX;
    QChartAxis* m_axisY = &m_defaultAxisY;
    qreal m_valuesMultiplier = 1.0;
    MarkerShape m_markerShape = Circle;
    qreal m_markerSize = 8;
    int m_hoverSer = -1, m_hoverPt = -1;
    int m_pressSer = -1, m_pressPt = -1;
    static constexpr int kMarginL = 50, kMarginR = 20, kMarginT = 20, kMarginB = 40;
};

#endif
