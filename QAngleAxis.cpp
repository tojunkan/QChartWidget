#include "QAngleAxis.h"
#include "QPolarProjection.h"
#include <QtMath>
#include <QDebug>
#include "QChartDebug.h"  // 假设包含 logAxis 声明

// ===== 构造函数 =====
QAngleAxis::QAngleAxis(QObject* parent)
    : QChartAxis(parent, Qt::AlignCenter)
{
    m_min = 0.0;
    m_max = 360.0;
    m_tickCount = 8;
    m_visible = false;
    m_panEnabled = false;
    m_zoomEnabled = false;

    qCDebug(logAxis) << "QAngleAxis constructed: min=" << m_min
        << "max=" << m_max
        << "tickCount=" << m_tickCount
        << "visible=" << m_visible;
    
    if (logAxis().isDebugEnabled()) {
        QPolarProjection a;
        qCDebug(logAxis)<<a.mapToPixel(valueToNormalized(-90), 1, QRectF(QPointF(20, 20), QSizeF(200, 600)));
        valueToNormalized(540);
        valueToNormalized(-540);
        normalizedToValue(0);
        normalizedToValue(0.5);
        normalizedToValue(1);

        tickValues();
        tickLabels();
        subTickValues();
    }
}

CoordinateSystem QAngleAxis::coordinateSystem() const {
    return CoordinateSystem::Polar;
}

// ===== 坐标映射（线性） =====
qreal QAngleAxis::valueToNormalized(qreal value) const {
    qreal norm = 0.0;
    if (!qFuzzyCompare(m_max, m_min)) {
        norm = (value - m_min) / (m_max - m_min);
    }
    qCDebug(logAxis) << "valueToNormalized: value=" << value
        << "norm=" << norm
        << "m_min=" << m_min << "m_max=" << m_max;
    return norm;
}

qreal QAngleAxis::normalizedToValue(qreal norm) const {
    qreal value = m_min + norm * (m_max - m_min);
    qCDebug(logAxis) << "normalizedToValue: norm=" << norm
        << "value=" << value;
    return value;
}

// ===== 主刻度 =====
QVector<qreal> QAngleAxis::tickValues() const {
    qreal step = (m_max - m_min) / qreal(m_tickCount);
    QVector<qreal> ticks;
    for (int i = 0; i <= m_tickCount; ++i) {
        ticks.append(m_min + qreal(i) * step);
    }
    qCDebug(logAxis) << "tickValues: ticks=" << ticks;
    return ticks;
}

// ===== 主刻度标签 =====
QStringList QAngleAxis::tickLabels() const {
    QStringList labels;
    for (qreal value : tickValues()) {
        int deg = int(value) % 360;
        labels.append(QString::number(deg) + QString::fromUtf8("°"));
    }
    qCDebug(logAxis) << "tickLabels: labels=" << labels;
    return labels;
}

// ===== 次刻度 =====
QVector<qreal> QAngleAxis::subTickValues() const {
    QVector<qreal> subs;
    if (m_subTickCount <= 0) {
        qCDebug(logAxis) << "subTickValues: m_subTickCount=" << m_subTickCount << ", returning empty";
        return subs;
    }

    qreal step = (m_max - m_min) / qreal(m_tickCount);
    qreal subStep = step / qreal(m_subTickCount + 1);

    for (qreal v = m_min; v <= m_max + subStep * 0.001; v += subStep) {
        bool isMain = false;
        for (qreal main : tickValues()) {
            if (qFuzzyCompare(v, main)) { isMain = true; break; }
        }
        if (!isMain) subs.append(v);
    }
    qCDebug(logAxis) << "subTickValues: subs=" << subs;
    return subs;
}