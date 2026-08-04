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
