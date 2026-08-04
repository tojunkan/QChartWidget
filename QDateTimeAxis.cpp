// QDateTimeAxis.cpp —— 日期时间轴实现
#include "QDateTimeAxis.h"
#include "QChartDebug.h"
#include <QDebug>
#include <QtMath>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logDateTimeAxis, "chart.axis.datetime")

QDateTimeAxis::QDateTimeAxis(QObject* parent, Qt::Alignment alignment)
    : QChartAxis(parent, alignment)
{
    m_tickCount = 7; // 时间轴默认 7 个刻度
}

// ===== 刻度生成：给定 Numeric 范围（epoch ms），生成合适的时间刻度 =====
QVector<qreal> QDateTimeAxis::tickValues(qreal numericMin, qreal numericMax) const {
    QVector<qreal> ticks;
    if (numericMin >= numericMax) { ticks << numericMin; return ticks; }

    qreal rangeMs = numericMax - numericMin;
    TimeStepInfo info = chooseStep(static_cast<qint64>(rangeMs));
    qint64 stepMs = info.stepMs;
    if (stepMs <= 0) { ticks << numericMin << numericMax; return ticks; }

    // 对齐起始到步长整数倍（从 epoch 0 开始）
    qint64 minMs = static_cast<qint64>(numericMin);
    qint64 maxMs = static_cast<qint64>(numericMax);
    qint64 start = (minMs / stepMs) * stepMs;
    if (start < minMs) start += stepMs;

    for (qint64 t = start; t <= maxMs; t += stepMs) {
        ticks.append(static_cast<qreal>(t));
    }

    if (ticks.size() < 2) { ticks.clear(); ticks << numericMin << numericMax; }

    qCDebug(logDateTimeAxis) << "tickValues: range=" << rangeMs << "ms step=" << stepMs
                             << "ms count=" << ticks.size();
    return ticks;
}

QStringList QDateTimeAxis::tickLabels(const QVector<qreal>& ticks) const {
    QStringList labels;
    TimeStepInfo info = chooseStep(0); // 只用来自动格式
    QString fmt = m_format.isEmpty() ? info.format : m_format;
    for (qreal t : ticks) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(t));
        labels.append(dt.toString(fmt));
    }
    return labels;
}

QVector<qreal> QDateTimeAxis::subTickValues(qreal numericMin, qreal numericMax) const {
    QVector<qreal> subs;
    qreal rangeMs = numericMax - numericMin;
    TimeStepInfo info = chooseStep(static_cast<qint64>(rangeMs));
    int n = info.subDivisions;
    if (n <= 0) return subs;

    QVector<qreal> mains = tickValues(numericMin, numericMax);
    if (mains.size() < 2) return subs;

    qreal subStep = info.stepMs / static_cast<qreal>(n + 1);
    for (int i = 0; i < mains.size() - 1; ++i) {
        for (int j = 1; j <= n; ++j) {
            qreal val = mains[i] + j * subStep;
            if (val >= numericMin && val <= numericMax)
                subs.append(val);
        }
    }
    return subs;
}

// ===== 时间步长选择算法 =====
QDateTimeAxis::TimeStepInfo QDateTimeAxis::chooseStep(qint64 rangeMs) const {
    // 时间单位表（ms, 格式, 次刻度数）
    struct Unit { qint64 ms; QString fmt; int sub; };
    static const QList<Unit> units = {
        { 1000LL,              "hh:mm:ss",     0 },
        { 5000LL,              "hh:mm:ss",     4 },
        { 10000LL,             "hh:mm:ss",     4 },
        { 30000LL,             "hh:mm:ss",     5 },
        { 60000LL,             "hh:mm",        5 },
        { 300000LL,            "hh:mm",        4 },
        { 600000LL,            "hh:mm",        5 },
        { 1800000LL,           "hh:mm",        5 },
        { 3600000LL,           "hh:mm",        3 },
        { 21600000LL,          "hh:mm",        5 },
        { 43200000LL,          "hh:mm",        5 },
        { 86400000LL,          "MM-dd",        3 },
        { 172800000LL,         "MM-dd",        3 },
        { 604800000LL,         "MM-dd",        6 },
        { 2592000000LL,        "yyyy-MM",      2 },
        { 15552000000LL,       "yyyy-MM",      5 },
        { 31536000000LL,       "yyyy",         3 },
        { 315360000000LL,      "yyyy",         9 },
    };

    qreal range = static_cast<qreal>(rangeMs);
    int target = qMax(2, m_tickCount);
    qreal rawStepMs = range / (target - 1);

    const Unit* chosen = &units.last();
    for (const auto& u : units) {
        if (u.ms >= rawStepMs) { chosen = &u; break; }
    }

    // 如果刻度过多，升一档
    qreal estTicks = range / chosen->ms + 1;
    int idx = static_cast<int>(&*chosen - units.data());
    while (estTicks > target * 2.5 && idx < units.size() - 1) {
        chosen = &units[++idx];
        estTicks = range / chosen->ms + 1;
    }

    return { chosen->ms, chosen->fmt, chosen->sub };
}
