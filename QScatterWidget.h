#ifndef QSCATTERWIDGET_H
#define QSCATTERWIDGET_H
#include "QChartWidget.h"
#include "QChartGeometry.h"
#include "QChartAxis.h"
#include "QScatterSeries.h"

class QScatterWidget : public QChartWidget {
    Q_OBJECT
public:
    explicit QScatterWidget(QWidget* p=nullptr);
    QScatterSeries* addSeries(const QString& name);
    QChartAxis* axisX() { return m_geom->axisX(); }
    QChartAxis* axisY() { return m_geom->axisY(); }
private:
    QCartesianGeometry* m_geom;
    QValueAxis *m_axX, *m_axY;
};
#endif
