#include "QXYSeries.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QtMath>
#include <cmath>

static const QList<QColor> defaultXYColors() {
    return {
        QColor("#2196F3"), QColor("#F44336"), QColor("#4CAF50"),
        QColor("#FF9800"), QColor("#9C27B0"), QColor("#00BCD4"),
        QColor("#FF5722"), QColor("#3F51B5"), QColor("#8BC34A"),
        QColor("#E91E63")
    };
}

QXYSeries::QXYSeries(const QString& name, QObject* parent)
    : QObject(parent), m_name(name)
{
    // 用 name 哈希取默认色
    int idx = qHash(name) % defaultXYColors().size();
    m_color = defaultXYColors().at(idx);
}

void QXYSeries::setName(const QString& n) {
    if (m_name != n) { m_name = n; emit nameChanged(n); }
}

void QXYSeries::append(qreal x, qreal y) {
    m_points.append(QXYPoint(x, y));
    emit pointAdded(m_points.size() - 1);
    emit countChanged();
    emit pointsChanged();
}

void QXYSeries::append(const QXYPoint& p) {
    append(p.x, p.y);
}

void QXYSeries::insert(int index, qreal x, qreal y) {
    if (index < 0 || index > m_points.size()) return;
    m_points.insert(index, QXYPoint(x, y));
    emit pointAdded(index);
    emit countChanged();
    emit pointsChanged();
}

void QXYSeries::remove(int index) {
    if (index < 0 || index >= m_points.size()) return;
    m_points.remove(index);
    emit pointRemoved(index);
    emit countChanged();
    emit pointsChanged();
}

void QXYSeries::replace(int index, qreal x, qreal y) {
    if (index < 0 || index >= m_points.size()) return;
    m_points[index] = QXYPoint(x, y);
    emit pointReplaced(index);
    emit pointsChanged();
}

void QXYSeries::clear() {
    m_points.clear();
    emit countChanged();
    emit pointsChanged();
}

QXYPoint QXYSeries::at(int index) const {
    return (index >= 0 && index < m_points.size()) ? m_points.at(index) : QXYPoint();
}

void QXYSeries::setPoints(const QVector<QXYPoint>& pts) {
    m_points = pts;
    emit countChanged();
    emit pointsChanged();
}

void QXYSeries::setColor(const QColor& c) {
    if (m_color != c) { m_color = c; emit colorChanged(c); }
}

// 生成正弦曲线数据
QXYSeries* QXYSeries::sinusoidal(const QString& name, int n, qreal amp, qreal freq, qreal phase, QObject* parent) {
    auto* s = new QXYSeries(name, parent);
    for (int i = 0; i < n; ++i) {
        qreal x = qreal(i) / (n - 1) * 10.0;  // 0..10
        qreal y = amp * std::sin(freq * x + phase);
        s->append(x, y);
    }
    qDebug() << "[QXYSeries] sinusoidal" << name << ":" << n << "points, amp=" << amp;
    return s;
}

// 生成正态分布散点
QXYSeries* QXYSeries::randomScatter(const QString& name, int n, qreal xMean, qreal xSd, qreal yMean, qreal ySd, QObject* parent) {
    auto* s = new QXYSeries(name, parent);
    auto* rng = QRandomGenerator::global();
    for (int i = 0; i < n; ++i) {
        // Box-Muller
        qreal u1 = rng->generateDouble();
        qreal u2 = rng->generateDouble();
        qreal z1 = std::sqrt(-2 * std::log(qMax(u1, 0.0001))) * std::cos(2 * M_PI * u2);
        qreal z2 = std::sqrt(-2 * std::log(qMax(u1, 0.0001))) * std::sin(2 * M_PI * u2);
        s->append(xMean + z1 * xSd, yMean + z2 * ySd);
    }
    qDebug() << "[QXYSeries] randomScatter" << name << ":" << n << "points";
    return s;
}
