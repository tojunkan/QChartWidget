#ifndef QPIEWIDGET_H
#define QPIEWIDGET_H
#include "QChartWidget.h"
#include "QChartGeometry.h"
#include "QPieSeries.h"

class QPieWidget : public QChartWidget {
    Q_OBJECT
public:
    explicit QPieWidget(QWidget* p=nullptr);
    QPieSeries* series() { return m_series; }
    void appendSlice(const QString& label, qreal value, const QColor& c=QColor());
    void setHoleSize(qreal s);
    void setStartAngle(qreal deg);
    void setSliceExploded(int i, bool on);
private:
    QPolarGeometry* m_geom;
    QPieSeries* m_series;
};
#endif
