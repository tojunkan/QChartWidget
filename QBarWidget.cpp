#include "QBarWidget.h"

QBarWidget::QBarWidget(QWidget* p) : QChartWidget(p) {
    m_geom = new QCartesianGeometry; addGeometry(m_geom);
    auto* axX = new QBarCategoryAxis; auto* axY = new QValueAxis;
    addAxis(axX); addAxis(axY);
    m_geom->setAxisX(axX); m_geom->setAxisY(axY);
    m_series = new QBarSeries("bar"); m_geom->addSeries(m_series);
}
void QBarWidget::setCategories(const QStringList& cats) {
    m_series->setCategories(cats);
    qobject_cast<QBarCategoryAxis*>(m_geom->axisX())->setCategories(cats);
}
QBarSet* QBarWidget::addBarSet(const QString& l, const QVector<qreal>& vals) {
    auto* s = new QBarSet(l); s->setValues(vals); m_series->addBarSet(s); return s;
}
