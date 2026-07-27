#include "QValueAxis.h"
#include "QCartesianProjection.h"
#include <QtMath>
#include <QDebug>

// ===== 构造函数 =====
QValueAxis::QValueAxis(QObject* p, Qt::Alignment alignment)
    : QChartAxis(p, alignment) // 父类默认底部对齐
{
    // 设置一个合理的默认范围
    m_min = 0.0;
    m_max = 10.0;
}

CoordinateSystem QValueAxis::coordinateSystem() const { return CoordinateSystem::Cartesian; }

// ===== 坐标映射（线性） =====
qreal QValueAxis::valueToNormalized(qreal value) const {
    if (qFuzzyCompare(m_max, m_min))
        return 0.0;
    return (value - m_min) / (m_max - m_min);
}

qreal QValueAxis::normalizedToValue(qreal norm) const {
    return m_min + norm * (m_max - m_min);
}

// ===== 漂亮步长算法（核心） =====
qreal QValueAxis::niceStep(qreal range) const {
    if (range <= 0 || !std::isfinite(range))
        return 1.0;

    // 目标刻度数：使用 m_tickCount，但至少为 2
    int targetTicks = qMax(2, m_tickCount);
    qreal roughStep = range / targetTicks;

    // 计算数量级（10 的幂）
    qreal exponent = std::floor(std::log10(roughStep));
    qreal magnitude = std::pow(10.0, exponent);

    // 归一化到 [1, 10)
    qreal normalized = roughStep / magnitude;

    // 选择“漂亮”的整数因子：1, 2, 5, 10
    qreal multiplier;
    if (normalized < 1.5)
        multiplier = 1.0;
    else if (normalized < 3.5)
        multiplier = 2.0;
    else if (normalized < 7.5)
        multiplier = 5.0;
    else
        multiplier = 10.0;

    return multiplier * magnitude;
}

// ===== 设置固定步长 =====
void QValueAxis::setTickInterval(qreal v) {
    if (v <= 0) {
        m_tickInterval = 0;
        emit tickCountChanged(); // 通知更新
    }
    else {
        m_tickInterval = v;
        emit tickCountChanged();
    }
}

// ===== 生成主刻度值 =====
QVector<qreal> QValueAxis::tickValues() const {
    QVector<qreal> result;
    if (!m_visible || qFuzzyCompare(m_max, m_min)) {
        result.append(m_min);
        return result;
    }

    qreal step;
    if (m_tickInterval > 0) {
        step = m_tickInterval;
    }
    else {
        step = niceStep(m_max - m_min);
        // 确保步长不会因为浮点误差产生0
        if (step <= 0) step = (m_max - m_min) / qMax(1, m_tickCount - 1);
    }

    // 对齐起始值（向上取整到 step 的倍数）
    qreal first = std::ceil(m_min / step) * step;
    qreal last = std::floor(m_max / step) * step;

    // 如果对齐后区间为空（比如范围太小），退化为直接等分
    if (first > last) {
        int count = qMax(2, m_tickCount);
        for (int i = 0; i < count; ++i) {
            qreal t = static_cast<qreal>(i) / (count - 1);
            result.append(m_min + t * (m_max - m_min));
        }
        return result;
    }

    // 填充刻度
    for (qreal v = first; v <= last + step * 0.0001; v += step) {
        // 防止浮点误差导致超出范围
        if (v > m_max + step * 0.0001) break;
        result.append(v);
    }

    // 如果结果太少（<2），补全首尾
    if (result.size() < 2) {
        result.clear();
        result.append(m_min);
        result.append(m_max);
    }

    return result;
}

// ===== 生成主刻度标签 =====
QStringList QValueAxis::tickLabels() const {
    QStringList labels;
    QVector<qreal> values = tickValues();
    if (values.isEmpty())
        return labels;

    for (qreal v : values) {
        QString label;
        if (!m_labelFormat.isEmpty()) {
            // 用户自定义格式（如 "%.2f cm"）
            label = QString::asprintf(m_labelFormat.toUtf8().constData(), v);
        }
        else if (m_labelDecimals >= 0) {
            // 用户指定小数位数
            label = QString::number(v, 'f', m_labelDecimals);
        }
        else {
            // 自动去零（保留有效位数）
            QString str = QString::number(v, 'f', 8);
            // 去除末尾多余的 '0' 和可能的小数点
            while (str.endsWith('0'))
                str.chop(1);
            if (str.endsWith('.'))
                str.chop(1);
            label = str;
        }
        labels.append(label);
    }
    return labels;
}

// ===== 生成次刻度值（线性插值） =====
QVector<qreal> QValueAxis::subTickValues() const {
    QVector<qreal> result;
    if (m_subTickCount <= 0)
        return result;

    QVector<qreal> mains = tickValues();
    if (mains.size() < 2)
        return result;


    qreal v1 = mains[0];
    qreal v2 = mains[1];
    qreal step = (v2 - v1) / (m_subTickCount + 1);

    // 从第一个主刻度向左补充次刻度
    qreal firstMain = mains.first();
    for (qreal v = firstMain - step; v > m_min; v -= step) {
        result.append(v);
    }

    for (int i = 0; i < mains.size() - 1; ++i) {
        for (int j = 1; j <= m_subTickCount; ++j) {
            result.append(mains[i] + j * step);
        }
    }

    // 从最后一个主刻度向右补充次刻度
    qreal lastMain = mains.last();
    for (qreal v = lastMain + step; v < m_max; v += step) {
        result.append(v);
    }

    return result;
}

// ===== 次刻度标签（默认无文字） =====
QStringList QValueAxis::subTickLabels() const {
    // 次刻度一般不显示标签，返回空列表
    return QStringList();
}