#ifndef QLINEWIDGET_H
#define QLINEWIDGET_H

#include <QWidget>
#include <QList>
#include <QPair>
#include "QChartAxis.h"
#include "QXYSeries.h"

class QLineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QLineWidget(QWidget* parent = nullptr);
    ~QLineWidget() override;

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
    void setLineWidth(qreal w) { m_lineWidth = qMax(0.5, w); update(); }
    qreal lineWidth() const { return m_lineWidth; }
    void setPointsVisible(bool v) { m_pointsVisible = v; update(); }
    bool pointsVisible() const { return m_pointsVisible; }
    void setPointMarkerSize(qreal px) { m_pointMarkerSize = qMax(1.0, px); update(); }
    void setSmooth(bool s) { m_smooth = s; update(); }
    bool smooth() const { return m_smooth; }

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
    void drawLines(QPainter* p, const QRectF& area);
    void drawPoints(QPainter* p, const QRectF& area);
    QRectF plotArea() const;
    // Catmull-Rom → Bezier for smooth curves
    QPainterPath smoothPath(const QVector<QPointF>& pts) const;

    QList<QXYSeries*> m_series;
    QValueAxis m_defaultAxisX, m_defaultAxisY;
    QChartAxis* m_axisX = &m_defaultAxisX;
    QChartAxis* m_axisY = &m_defaultAxisY;
    qreal m_valuesMultiplier = 1.0;
    qreal m_lineWidth = 2;
    bool m_pointsVisible = false;
    qreal m_pointMarkerSize = 6;
    bool m_smooth = false;
    int m_hoverSer = -1, m_hoverPt = -1;
    int m_pressSer = -1, m_pressPt = -1;
    static constexpr int kMarginL = 50, kMarginR = 20, kMarginT = 20, kMarginB = 40;
};

#endif
