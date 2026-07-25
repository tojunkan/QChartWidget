#include "QChartSeries.h"

QChartSeries::QChartSeries(const QString& n, QObject* p)
    : QObject(p), m_name(n) {
}

void QChartSeries::setName(const QString& n) {
    if (m_name == n) return;
    m_name = n;
    emit nameChanged(n);
}

void QChartSeries::setVisible(bool v) {
    if (m_visible == v) return;
    m_visible = v;
    emit visibleChanged();
}

void QChartSeries::setOpacity(qreal o) {
    o = qBound(0.0, o, 1.0);
    if (qFuzzyCompare(m_opacity, o)) return;
    m_opacity = o;
    emit opacityChanged();
}