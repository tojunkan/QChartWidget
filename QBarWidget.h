#ifndef QBARWIDGET_H
#define QBARWIDGET_H
#include "QChartWidget.h"
#include "QChartAxis.h"
#include "QChartGeometry.h"
#include "QBarSeries.h"

class QBarWidget : public QChartWidget {
    Q_OBJECT
public:
    explicit QBarWidget(QWidget* p=nullptr);
    QBarSeries* barSeries() { return m_series; }
    void setCategories(const QStringList& cats);
    QBarSet* addBarSet(const QString& label, const QVector<qreal>& vals={});
    void setBarsType(QBarSeries::BarsType t) { m_series->setBarsType(t); }
    void setBarLabelsVisible(bool v) { m_series->setBarLabelsVisible(v); }
    void setOrientation(QBarSeries::Orientation o) { m_series->setOrientation(o); }
    QChartAxis* axisX() { return m_geom->axisX(); }
    QChartAxis* axisY() { return m_geom->axisY(); }
protected:
    QCartesianGeometry* m_geom;
    QBarSeries* m_series;
};
#endif
