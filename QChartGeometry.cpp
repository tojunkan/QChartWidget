#include "QChartGeometry.h"
#include "QChartWidget.h"
#include "QChartSeries.h"
#include <QPainterPath>
#include <QDebug>

QChartGeometry::QChartGeometry(QObject* p) : QObject(p) {}
QChartGeometry::~QChartGeometry() {
    qDeleteAll(m_series);
}

void QChartGeometry::setAxisX(QChartAxis* a) {
    m_axisX = a;
    // 轴不再持有几何体，无需调用 setGeometry
}

void QChartGeometry::setAxisY(QChartAxis* a) {
    m_axisY = a;
}

QRectF QChartGeometry::plotArea() const {
    auto* cw = qobject_cast<QChartWidget*>(parent());
    return cw ? cw->plotArea() : QRectF();
}

void QChartGeometry::drawGrid(QPainter* p, QChartProjection* projection) const {
    // 基类空实现，子类重写
}

void QChartGeometry::addSeries(QChartSeries* s) {
    if (!s) return;
    s->setParent(this);
    m_series.append(s);
    emit seriesAdded(s);
}

void QChartGeometry::removeSeries(QChartSeries* s) {
    if (m_series.removeAll(s)) {
        emit seriesRemoved(s);
        delete s;
    }
}

void QChartGeometry::clearSeries() {
    qDeleteAll(m_series);
    m_series.clear();
}

void QChartGeometry::drawAllSeries(QPainter* p, QChartProjection* projection) {
    for (auto* s : m_series) {
        if (s->isVisible()) {
            p->save();
            p->setOpacity(s->opacity());
            drawSeries(p, s, projection);
            p->restore();
        }
    }
}

void QChartGeometry::drawSeries(QPainter* p, QChartSeries* s, QChartProjection* projection) {
    // 基类空实现，子类可重写
    Q_UNUSED(p);
    Q_UNUSED(s);
}

int QChartGeometry::hitTestSeries(QChartSeries* s, const QPointF& pos) const {
    // 基类空实现，子类重写
    Q_UNUSED(s);
    Q_UNUSED(pos);
    return -1;
}

QPair<QChartSeries*, int> QChartGeometry::hitTest(const QPointF& pos) const {
    for (int i = m_series.size() - 1; i >= 0; --i) {
        auto* s = m_series[i];
        if (!s->isVisible()) continue;
        int idx = hitTestSeries(s, pos);
        if (idx >= 0) return { s, idx };
    }
    return { nullptr, -1 };
}