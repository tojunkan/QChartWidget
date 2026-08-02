// QValueAxis.cpp —— 数值轴实现
#include "QValueAxis.h"
#include "QChartDebug.h"
#include <QtMath>
#include <QDebug>
#include <cmath>

Q_LOGGING_CATEGORY(logValueAxis, "chart.axis.value")

QValueAxis::QValueAxis(QObject* parent, Qt::Alignment alignment)
    : QChartAxis(parent, alignment)
{
    // 数值轴无特殊初始化——刻度生成依赖调用者传入的 numericMin/numericMax
    qCDebug(logValueAxis) << "QValueAxis created, alignment:" << alignment;
}

// ===== 数值化：恒等 =====
qreal QValueAxis::toNumeric(QVariant data) const {
    bool ok = false;
    qreal val = data.toDouble(&ok);
    if (!ok) {
        qWarning() << "QValueAxis::toNumeric: invalid data —" << data
                   << "— returning NaN";
        return qQNaN();
    }
    return val;
}

QVariant QValueAxis::fromNumeric(qreal num) const {
    // 直通：NaN→NaN, Inf→Inf, 正常值→正常值
    // 调用方负责在绘制前检查 NaN 并跳过
    return QVariant::fromValue(num);
}

// ===== niceStep 算法 =====
qreal QValueAxis::niceStep(qreal range) const {
    if (range <= 0.0 || !std::isfinite(range))
        return 1.0;

    int targetTicks = qMax(2, m_tickCount);
    qreal roughStep = range / targetTicks;

    // 计算数量级（10 的幂）
    qreal exponent = std::floor(std::log10(roughStep));
    qreal magnitude = std::pow(10.0, exponent);

    // 归一化到 [1, 10)
    qreal normalized = roughStep / magnitude;

    // 漂亮因子候选表（按升序排列）
    static const qreal factors[] = { 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 8.0, 10.0 };

    // 找最接近 normalized 的因子
    qreal bestFactor = factors[0];
    qreal bestDist = std::abs(normalized - factors[0]);
    for (const qreal& f : factors) {
        qreal dist = std::abs(normalized - f);
        if (dist < bestDist) {
            bestDist = dist;
            bestFactor = f;
        }
    }

    return bestFactor * magnitude;
}

void QValueAxis::setTickInterval(qreal v) {
    if (v <= 0.0) {
        m_tickInterval = 0.0;
        qCDebug(logValueAxis) << "tickInterval reset to auto";
    } else {
        m_tickInterval = v;
        qCDebug(logValueAxis) << "tickInterval set to" << v;
    }
    emit tickCountChanged();
}

// ===== 生成主刻度值 =====
QVector<qreal> QValueAxis::tickValues(qreal numericMin, qreal numericMax) const {
    QVector<qreal> result;
    if (qFuzzyCompare(numericMin, numericMax)) {
        qCDebug(logValueAxis) << "tickValues: degenerate range [" << numericMin
                              << "," << numericMax << "] — returning single tick";
        result.append(numericMin);
        return result;
    }

    qreal range = numericMax - numericMin;
    qreal step;
    if (m_tickInterval > 0.0) {
        step = m_tickInterval;
    } else {
        step = niceStep(range);
        if (step <= 0.0 || !std::isfinite(step))
            step = range / qMax(1, m_tickCount - 1);
    }

    // 对齐起始值（向上取整到 step 的倍数）
    qreal first = std::ceil(numericMin / step) * step;
    qreal last  = std::floor(numericMax / step) * step;

    // 如果对齐后区间为空（范围太小），退化为等分
    if (first > last) {
        int count = qMax(2, m_tickCount);
        for (int i = 0; i < count; ++i) {
            qreal t = static_cast<qreal>(i) / (count - 1);
            result.append(numericMin + t * range);
        }
        qCDebug(logValueAxis) << "tickValues: degenerate range, using" << count
                              << "equal divisions";
        return result;
    }

    // 填充刻度
    for (qreal v = first; v <= last + step * 0.0001; v += step) {
        if (v > numericMax + step * 0.0001) break;
        result.append(v);
    }

    // 保障至少有 2 个刻度
    if (result.size() < 2) {
        result.clear();
        result.append(numericMin);
        result.append(numericMax);
    }

    qCDebug(logValueAxis) << "tickValues: [" << numericMin << "," << numericMax
                          << "] step=" << step << " count=" << result.size();
    return result;
}

// ===== 标签格式化 =====
QStringList QValueAxis::tickLabels(const QVector<qreal>& ticks) const {
    QStringList labels;
    for (qreal v : ticks) {
        QString label;
        if (!m_labelFormat.isEmpty()) {
            // 用户自定义 printf 格式（如 "%.2f°"）
            label = QString::asprintf(m_labelFormat.toUtf8().constData(), v);
        } else if (m_labelPrecision >= 0) {
            // 固定小数位数
            label = QString::number(v, 'f', m_labelPrecision);
        } else {
            // 自动去零
            QString str = QString::number(v, 'f', 8);
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

// ===== 次刻度 =====
QVector<qreal> QValueAxis::subTickValues(qreal numericMin, qreal numericMax) const {
    QVector<qreal> result;
    if (m_subTickCount <= 0)
        return result;

    QVector<qreal> mains = tickValues(numericMin, numericMax);
    if (mains.size() < 2)
        return result;

    qreal step = (mains[1] - mains[0]) / (m_subTickCount + 1);

    // 从第一个主刻度向左补充
    qreal firstMain = mains.first();
    for (qreal v = firstMain - step; v > numericMin; v -= step)
        result.append(v);

    // 主刻度之间
    for (int i = 0; i < mains.size() - 1; ++i) {
        for (int j = 1; j <= m_subTickCount; ++j)
            result.append(mains[i] + j * step);
    }

    // 从最后一个主刻度向右补充
    qreal lastMain = mains.last();
    for (qreal v = lastMain + step; v < numericMax; v += step)
        result.append(v);

    return result;
}
