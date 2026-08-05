// QXYSeries.cpp —— 点集系列实现
#include "QXYSeries.h"
#include <QDebug>

QXYSeries::QXYSeries(const QString& name, QObject* parent)
    : QChartSeries(name, parent) {}

// ===== 数据操作 =====
void QXYSeries::append(qreal x, qreal y) {
    m_points.append(QDataPoint(QVariant::fromValue(x), QVariant::fromValue(y)));
    emit dataChanged();
}

void QXYSeries::append(const QDataPoint& pt) {
    m_points.append(pt);
    emit dataChanged();
}

void QXYSeries::insert(int index, const QDataPoint& pt) {
    if (index < 0 || index > m_points.size()) {
        qWarning() << "QXYSeries::insert: index out of range" << index;
        return;
    }
    m_points.insert(index, pt);
    emit dataChanged();
}

void QXYSeries::remove(int index) {
    if (index < 0 || index >= m_points.size()) {
        qWarning() << "QXYSeries::remove: index out of range" << index;
        return;
    }
    m_points.removeAt(index);
    emit dataChanged();
}

void QXYSeries::replace(int index, const QDataPoint& pt) {
    if (index < 0 || index >= m_points.size()) {
        qWarning() << "QXYSeries::replace: index out of range" << index;
        return;
    }
    m_points[index] = pt;
    emit dataChanged();
}

void QXYSeries::clear() {
    if (m_points.isEmpty()) return;
    m_points.clear();
    emit dataChanged();
}

void QXYSeries::setPoints(const QVector<QDataPoint>& pts) {
    m_points = pts;
    emit dataChanged();
}

// ===== 动画覆盖层 =====
// 每帧都可能被动画调用——发专用信号而非 dataChanged，
// 避免把"动画临时状态"误当成"数据改动"（后者可能触发布局重算）
void QXYSeries::setRenderOverride(const QVector<QPointF>& numericPts) {
    m_overridePoints = numericPts;
    m_hasOverride = true;
    emit renderOverrideChanged();
}

void QXYSeries::clearRenderOverride() {
    m_overridePoints.clear();
    m_hasOverride = false;
    emit renderOverrideChanged();
}
