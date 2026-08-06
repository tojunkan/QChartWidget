#ifndef QLINEWIDGET_H
#define QLINEWIDGET_H
#include "QChartWidget.h"
#include "QChartLayer.h"
#include "QChartAxis.h"
#include "QLineSeries.h"

class QLineWidget : public QChartWidget {
    Q_OBJECT
public:
    explicit QLineWidget(QWidget* p=nullptr);
    QLineSeries* series() { return m_series; }
    void setSmooth(bool s) { m_series->setSmooth(s); }
    void setPointsVisible(bool v) { m_series->setPointsVisible(v); }
    QChartAxis* axisX() { return m_geom->axisX(); }
    QChartAxis* axisY() { return m_geom->axisY(); }
private:
    QCartesianLayer* m_geom;
    QValueAxis *m_axX, *m_axY;
    QLineSeries* m_series;
};
#endif
