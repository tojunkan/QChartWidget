// QChartSeries.cpp —— 系列基类实现
#include "QChartSeries.h"
#include "QChartDebug.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logSeries, "chart.series")
Q_LOGGING_CATEGORY(logSeriesVerbose, "chart.series.verbose")

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

// ===== 命中检测默认实现 =====
// 基类无法知道数据布局，默认返回 -1。子类（Scatter 等）重写。
int QChartSeries::hitTest(const QPointF& pixel,
                          std::function<QPointF(QVariant,QVariant)> toPixel,
                          const DrawContext* ctx) const {
    Q_UNUSED(pixel);
    Q_UNUSED(toPixel);
    Q_UNUSED(ctx);
    return -1;
}
