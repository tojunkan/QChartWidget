#include "QChartAxis.h"
#include <QtMath>
#include <QDebug>
#include <cmath>

// ========== QChartAxis 基类 ==========
QChartAxis::QChartAxis(QObject* parent) : QObject(parent) {}

void QChartAxis::setTickCount(int n)
{
    if (n < 2) n = 2;
    m_tickCount = n;
    emit tickCountChanged();
}

// ========== QValueAxis ==========
QValueAxis::QValueAxis(QObject* parent) : QChartAxis(parent)
{
    m_min = 0;
    m_max = 10;
}

qreal QValueAxis::mapToPixel(qreal value, qreal axisLength) const
{
    if (qFuzzyCompare(m_max, m_min)) return 0;
    return (value - m_min) / (m_max - m_min) * axisLength;
}

qreal QValueAxis::pixelToValue(qreal pixel, qreal axisLength) const
{
    if (axisLength <= 0) return m_min;
    return m_min + pixel / axisLength * (m_max - m_min);
}

qreal QValueAxis::niceStep(qreal range) const
{
    // 把范围缩放到 1~10 量级，取 nice 步长
    qreal exponent = std::floor(std::log10(range));
    qreal fraction = range / std::pow(10.0, exponent);
    qreal nice;
    if (fraction <= 1.5)       nice = 1.0;
    else if (fraction <= 3.5)  nice = 2.0;
    else if (fraction <= 7.5)  nice = 5.0;
    else                       nice = 10.0;
    return nice * std::pow(10.0, exponent);
}

void QValueAxis::setTickInterval(qreal v)
{
    m_tickInterval = (v > 0) ? v : 0;
}

QVector<qreal> QValueAxis::tickValues() const
{
    QVector<qreal> ticks;
    qreal range = m_max - m_min;
    if (range <= 0) return ticks;

    qreal step = (m_tickInterval > 0) ? m_tickInterval
                                      : niceStep(range / (m_tickCount - 1));

    // 从 tickAnchor（=min 向下取整到 step）开始
    qreal start = std::floor(m_min / step) * step;
    if (start < m_min) start += step;

    for (qreal v = start; v <= m_max + step * 0.001; v += step)
        ticks.append(v);

    qDebug() << "[QValueAxis] ticks: min=" << m_min << "max=" << m_max
             << "step=" << step << "count=" << ticks.size();
    return ticks;
}

QStringList QValueAxis::tickLabels() const
{
    QStringList labels;
    QVector<qreal> ticks = tickValues();
    for (qreal v : ticks) {
        QString label;
        if (!m_labelFormat.isEmpty()) {
            label = QString::asprintf(qPrintable(m_labelFormat), v);
        } else {
            int decimals = m_labelDecimals;
            if (decimals < 0) {
                // 自动精度
                qreal step = ticks.size() > 1 ? ticks[1] - ticks[0] : 1;
                decimals = (step >= 1) ? 0 : qMax(0, (int)std::ceil(-std::log10(step)));
            }
            label = QString::number(v, 'f', decimals);
        }
        labels.append(label);
    }
    return labels;
}

// ========== QBarCategoryAxis ==========
QBarCategoryAxis::QBarCategoryAxis(QObject* parent) : QChartAxis(parent)
{
    m_min = 0;
    m_max = 1;  // 等间距后范围是 0~catCount
}

qreal QBarCategoryAxis::mapToPixel(qreal value, qreal axisLength) const
{
    int n = m_categories.size();
    if (n == 0) return 0;
    // value = category index (0-based)，居中放在槽内
    qreal step = axisLength / n;
    return (value + 0.5) * step;
}

qreal QBarCategoryAxis::pixelToValue(qreal pixel, qreal axisLength) const
{
    int n = m_categories.size();
    if (n == 0 || axisLength <= 0) return 0;
    qreal step = axisLength / n;
    return std::floor(pixel / step);
}

void QBarCategoryAxis::setCategories(const QStringList& cats)
{
    m_categories = cats;
    m_max = qMax(1, cats.size());
    emit categoriesChanged();
}

void QBarCategoryAxis::append(const QString& cat)
{
    m_categories.append(cat);
    m_max = m_categories.size();
    emit categoriesChanged();
}

void QBarCategoryAxis::insert(int index, const QString& cat)
{
    m_categories.insert(index, cat);
    m_max = m_categories.size();
    emit categoriesChanged();
}

void QBarCategoryAxis::remove(const QString& cat)
{
    m_categories.removeAll(cat);
    m_max = qMax(1, m_categories.size());
    emit categoriesChanged();
}

void QBarCategoryAxis::clear()
{
    m_categories.clear();
    m_max = 1;
    emit categoriesChanged();
}

QVector<qreal> QBarCategoryAxis::tickValues() const
{
    QVector<qreal> ticks;
    for (int i = 0; i < m_categories.size(); ++i)
        ticks.append(i);
    return ticks;
}

QStringList QBarCategoryAxis::tickLabels() const
{
    return m_categories;
}

// ========== QLogAxis ==========
QLogAxis::QLogAxis(QObject* parent) : QChartAxis(parent)
{
    m_min = 1;  // log(0) 无意义
    m_max = 1000;
}

void QLogAxis::setBase(qreal b)
{
    if (b <= 1) return;
    m_base = b;
}

qreal QLogAxis::mapToPixel(qreal value, qreal axisLength) const
{
    if (value <= 0 || m_min <= 0) return 0;
    qreal logMin = std::log(m_min) / std::log(m_base);
    qreal logMax = std::log(m_max) / std::log(m_base);
    qreal logVal = std::log(value) / std::log(m_base);
    if (qFuzzyCompare(logMax, logMin)) return 0;
    return (logVal - logMin) / (logMax - logMin) * axisLength;
}

qreal QLogAxis::pixelToValue(qreal pixel, qreal axisLength) const
{
    if (axisLength <= 0 || m_min <= 0) return m_min;
    qreal logMin = std::log(m_min) / std::log(m_base);
    qreal logMax = std::log(m_max) / std::log(m_base);
    qreal logVal = logMin + pixel / axisLength * (logMax - logMin);
    return std::pow(m_base, logVal);
}

QVector<qreal> QLogAxis::tickValues() const
{
    QVector<qreal> ticks;
    if (m_min <= 0) return ticks;

    // 对数轴上取 m_base 的整数幂
    qreal logMin = std::floor(std::log(m_min) / std::log(m_base));
    qreal logMax = std::ceil(std::log(m_max) / std::log(m_base));
    for (qreal e = logMin; e <= logMax; e += 1.0) {
        qreal v = std::pow(m_base, e);
        if (v >= m_min && v <= m_max)
            ticks.append(v);
    }
    qDebug() << "[QLogAxis] ticks: base=" << m_base << "values=" << ticks;
    return ticks;
}

QStringList QLogAxis::tickLabels() const
{
    QStringList labels;
    for (qreal v : tickValues()) {
        // 大数用科学计数，小数用正常
        if (v >= 1e6 || v <= 1e-3)
            labels.append(QString::number(v, 'e', 1));
        else
            labels.append(QString::number(v, 'g', 4));
    }
    return labels;
}

// ========== QDateTimeAxis ==========
QDateTimeAxis::QDateTimeAxis(QObject* parent) : QChartAxis(parent)
{
    m_dtMin = QDateTime::currentDateTime();
    m_dtMax = m_dtMin.addSecs(86400);  // +1 天
    m_min = toEpoch(m_dtMin);
    m_max = toEpoch(m_dtMax);
}

void QDateTimeAxis::setRange(QDateTime min, QDateTime max)
{
    m_dtMin = min;
    m_dtMax = max;
    m_min = toEpoch(min);
    m_max = toEpoch(max);
    emit rangeChanged(m_min, m_max);
}

qreal QDateTimeAxis::mapToPixel(qreal value, qreal axisLength) const
{
    if (qFuzzyCompare(m_max, m_min)) return 0;
    return (value - m_min) / (m_max - m_min) * axisLength;
}

qreal QDateTimeAxis::pixelToValue(qreal pixel, qreal axisLength) const
{
    if (axisLength <= 0) return m_min;
    return m_min + pixel / axisLength * (m_max - m_min);
}

QVector<qreal> QDateTimeAxis::tickValues() const
{
    // 使用 QValueAxis 的线性逻辑（epoch 秒）
    QValueAxis helper;
    helper.setMin(m_min);
    helper.setMax(m_max);
    helper.setTickCount(m_tickCount);
    return helper.tickValues();
}

QStringList QDateTimeAxis::tickLabels() const
{
    QStringList labels;
    for (qreal v : tickValues()) {
        qint64 secs = qint64(v);
        QDateTime dt = QDateTime::fromSecsSinceEpoch(secs);
        labels.append(dt.toString(m_format));
    }
    qDebug() << "[QDateTimeAxis] ticks:" << labels;
    return labels;
}
