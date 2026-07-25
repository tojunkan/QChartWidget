#include "QLineWidget.h"

QLineWidget::QLineWidget(QWidget* p) : QChartWidget(p) {
    m_geom = new QCartesianGeometry; addGeometry(m_geom);
    m_axX = new QValueAxis; m_axY = new QValueAxis;
    addAxis(m_axX); addAxis(m_axY);
    m_geom->setAxisX(m_axX); m_geom->setAxisY(m_axY);
    m_series = new QLineSeries("line"); m_geom->addSeries(m_series);
}
