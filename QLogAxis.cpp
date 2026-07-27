#include "QLogAxis.h"
#include <QtMath>
#include <QDebug>
#include <cmath>

// ===== 构造函数 =====
QLogAxis::QLogAxis(QObject* parent, Qt::Alignment alignment)
    : QChartAxis(parent, alignment)
{
    m_min = 1.0;       // log(0) 无意义
    m_max = 1000.0;
    m_panEnabled  = true;   // 归一化层线性 → 可拖动
    m_zoomEnabled = true;
}

CoordinateSystem QLogAxis::coordinateSystem() const { return CoordinateSystem::Cartesian; }

void QLogAxis::setBase(qreal b) {
    if (b <= 1.0) return;
    m_base = b;
}

// ===== 坐标映射 =====
qreal QLogAxis::valueToNormalized(qreal value) const {
    if (value <= 0.0 || m_min <= 0.0) return 0.0;
    qreal logMin = std::log(m_min) / std::log(m_base);
    qreal logMax = std::log(m_max) / std::log(m_base);
    if (qFuzzyCompare(logMax, logMin)) return 0.0;
    qreal logVal = std::log(value) / std::log(m_base);
    return (logVal - logMin) / (logMax - logMin);
}

qreal QLogAxis::normalizedToValue(qreal norm) const {
    if (m_min <= 0.0) return m_min;
    qreal logMin = std::log(m_min) / std::log(m_base);
    qreal logMax = std::log(m_max) / std::log(m_base);
    qreal logVal = logMin + norm * (logMax - logMin);
    return std::pow(m_base, logVal);
}

// ===== 主刻度（基数的整数幂） =====
QVector<qreal> QLogAxis::tickValues() const {
    QVector<qreal> ticks;
    if (m_min <= 0.0) {
        qWarning() << "Axis alignment: " << m_alignment;
        qWarning()<< "minimum is incorrect: " << m_min;
        return ticks;
    }

    qreal logMin = std::floor(std::log(m_min) / std::log(m_base));
    qreal logMax = std::ceil(std::log(m_max)  / std::log(m_base));

    for (qreal e = logMin; e <= logMax; e += 1.0) {
        qreal value = std::pow(m_base, e);
        if (value >= m_min && value <= m_max) {
            ticks.append(value);
        }
    }
    return ticks;
}

// ===== 主刻度标签 =====
QStringList QLogAxis::tickLabels() const {
    QStringList labels;
    for (qreal value : tickValues()) {
        if (value >= 1e6 || value <= 1e-3) {
            labels.append(QString::number(value, 'e', 1));
        } else {
            labels.append(QString::number(value, 'g', 4));
        }
    }
    return labels;
}

QVector<qreal> QLogAxis::subTickValues() const {
    QVector<qreal> subs;
    if (m_subTickCount <= 0 || m_min <= 0.0 || m_max <= m_min)
        return subs;

    // 计算对数空间的范围
    qreal logMin = std::log(m_min) / std::log(m_base);
    qreal logMax = std::log(m_max) / std::log(m_base);

    // 确定需要覆盖的指数区间（向下取整到最小指数，向上取整到最大指数）
    int startExp = static_cast<int>(std::floor(logMin));
    int endExp = static_cast<int>(std::ceil(logMax));

    // 遍历每一个数量级区间 [base^e, base^(e+1)]
    for (int e = startExp; e < endExp; ++e) {
        qreal v1 = std::pow(m_base, e);
        qreal v2 = std::pow(m_base, e + 1);
        // 限制区间范围到 [m_min, m_max]
        qreal low = qMax(v1, m_min);
        qreal high = qMin(v2, m_max);
        if (high <= low) continue;

        // 在对数空间生成次刻度（log space）
        qreal logLow = std::log(low) / std::log(m_base);
        qreal logHigh = std::log(high) / std::log(m_base);
        qreal step = (logHigh - logLow) / (m_subTickCount + 1);
        for (int j = 1; j <= m_subTickCount; ++j) {
            qreal logVal = logLow + j * step;
            qreal subVal = std::pow(m_base, logVal);
            if (subVal > m_min && subVal < m_max) { // 避免与主刻度重合
                subs.append(subVal);
            }
        }
    }
    return subs;
}

void QLogAxis::pan(qreal deltaNorm) {
    // 在对数空间中平移
    qreal logMin = std::log(m_min) / std::log(m_base);
    qreal logMax = std::log(m_max) / std::log(m_base);
    qreal logSpan = logMax - logMin;
    qreal logShift = -deltaNorm * logSpan;  // 负号保持拖动方向一致

    qreal newLogMin = logMin + logShift;
    qreal newLogMax = logMax + logShift;

    qreal newMin = std::pow(m_base, newLogMin);
    qreal newMax = std::pow(m_base, newLogMax);

    // 防止越界（对数轴最小值必须 > 0）
    if (newMin > 0 && newMax > 0) {
        setMin(newMin);
        setMax(newMax);
    }
}

void QLogAxis::zoom(qreal centerNorm, qreal factor) {
    if (factor <= 0) return;

    // 计算归一化中心对应的对数空间值
    qreal logMin = std::log(m_min) / std::log(m_base);
    qreal logMax = std::log(m_max) / std::log(m_base);
    qreal logCenter = logMin + centerNorm * (logMax - logMin);

    // 在对数空间缩放
    qreal halfLogSpan = (logMax - logMin) * factor / 2.0;
    qreal newLogMin = logCenter - halfLogSpan;
    qreal newLogMax = logCenter + halfLogSpan;

    qreal newMin = std::pow(m_base, newLogMin);
    qreal newMax = std::pow(m_base, newLogMax);

    if (newMin > 0 && newMax > 0 && (newMax - newMin) > 1e-10) {
        setMin(newMin);
        setMax(newMax);
    }
}