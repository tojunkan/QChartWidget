#include "QAngleAxis.h"
#include <QtMath>

// ===== 构造函数 =====
QAngleAxis::QAngleAxis(QObject* parent)
    : QChartAxis(parent, Qt::AlignCenter)  // 极坐标轴无固定方向
{
    m_min = 0.0;
    m_max = 360.0;
    m_tickCount = 8;         // 0°, 45°, 90°, ..., 360°
    m_visible     = false;   // 饼图默认不显示角度刻度
    m_panEnabled  = false;   // 角度轴固定范围
    m_zoomEnabled = false;
}

// ===== 坐标映射（线性） =====
qreal QAngleAxis::valueToNormalized(qreal value) const {
    if (qFuzzyCompare(m_max, m_min)) return 0.0;
    return (value - m_min) / (m_max - m_min);
}

qreal QAngleAxis::normalizedToValue(qreal norm) const {
    return m_min + norm * (m_max - m_min);
}

// ===== 主刻度 =====
QVector<qreal> QAngleAxis::tickValues() const {
    qreal step = (m_max - m_min) / qreal(m_tickCount);
    QVector<qreal> ticks;
    for (int i = 0; i <= m_tickCount; ++i) {
        ticks.append(m_min + qreal(i) * step);
    }
    return ticks;
}

// ===== 主刻度标签 =====
QStringList QAngleAxis::tickLabels() const {
    QStringList labels;
    for (qreal value : tickValues()) {
        int deg = int(value) % 360;
        labels.append(QString::number(deg) + QString::fromUtf8("°"));
    }
    return labels;
}

// ===== 次刻度（每 10°） =====
QVector<qreal> QAngleAxis::subTickValues() const {
    QVector<qreal> subs;
    if (m_subTickCount <= 0) return subs;

    qreal step = (m_max - m_min) / qreal(m_tickCount);
    qreal subStep = step / qreal(m_subTickCount + 1);

    for (qreal v = m_min; v <= m_max + subStep * 0.001; v += subStep) {
        // 跳过主刻度位置
        bool isMain = false;
        for (qreal main : tickValues()) {
            if (qFuzzyCompare(v, main)) { isMain = true; break; }
        }
        if (!isMain) subs.append(v);
    }
    return subs;
}
