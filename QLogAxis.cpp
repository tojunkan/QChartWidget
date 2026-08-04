// QLogAxis.cpp —— 对数轴实现
#include "QLogAxis.h"
#include "QChartDebug.h"
#include <QtMath>
#include <QDebug>
#include <cmath>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logLogAxis, "chart.axis.log")

QLogAxis::QLogAxis(QObject* parent, Qt::Alignment alignment)
    : QChartAxis(parent, alignment) {
    m_tickCount = 6; // log 轴默认少一点
}

void QLogAxis::setBase(qreal b) {
    if (b <= 1.0) {
        qWarning() << "QLogAxis::setBase: base must be > 1, ignoring" << b;
        return;
    }
    m_base = b;
}

// ===== 刻度生成：log 空间均匀步长 =====
QVector<qreal> QLogAxis::tickValues(qreal numericMin, qreal numericMax) const {
    // numeric = log10(v)，所以范围是 log 数量级
    // 步长：1（跨一个数量级），0.5（跨半个），取决于范围
    QVector<qreal> ticks;
    if (numericMin >= numericMax) { ticks << numericMin; return ticks; }

    qreal range = numericMax - numericMin;
    qreal step = 1.0; // 默认每个数量级一个刻度（10, 100, 1000...）
    if (range < 1.5) step = 0.5;   // 狭范围：半数量级
    if (range < 0.5) step = 0.25;  // 更窄：1/4 数量级

    // 对齐到步长
    qreal first = std::ceil(numericMin / step) * step;
    for (qreal v = first; v <= numericMax + step * 0.001; v += step) {
        ticks.append(v);
    }

    if (ticks.size() < 2) { ticks.clear(); ticks << numericMin << numericMax; }
    return ticks;
}

QStringList QLogAxis::tickLabels(const QVector<qreal>& ticks) const {
    QStringList labels;
    for (qreal t : ticks) {
        qreal value = std::pow(m_base, t);
        // 科学计数格式：1e+3, 1e+4...
        if (value >= 1e4 || value <= 1e-3) {
            labels.append(QString("%1e%2").arg(
                QString::number(value / std::pow(m_base, std::floor(t)), 'g', 3),
                QString::number(static_cast<int>(std::floor(t)))));
        } else {
            labels.append(QString::number(value, 'g', 4));
        }
    }
    return labels;
}

// ===== 次刻度：log 空间的主刻度间内插 =====
QVector<qreal> QLogAxis::subTickValues(qreal numericMin, qreal numericMax) const {
    QVector<qreal> subs;
    if (m_subTickCount <= 0) return subs;

    QVector<qreal> mains = tickValues(numericMin, numericMax);
    if (mains.size() < 2) return subs;

    // log 空间线性插值：每个主刻度间插入 subTickCount 个次刻度
    qreal step = (mains[1] - mains[0]) / (m_subTickCount + 1);
    for (int i = 0; i < mains.size() - 1; ++i) {
        for (int j = 1; j <= m_subTickCount; ++j) {
            qreal val = mains[i] + j * step;
            if (val >= numericMin && val <= numericMax)
                subs.append(val);
        }
    }
    return subs;
}
