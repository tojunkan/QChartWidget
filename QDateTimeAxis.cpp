// QDateTimeAxis.cpp —— 日期时间轴实现
#include "QDateTimeAxis.h"
#include "QChartDebug.h"
#include <QDebug>
#include <QtMath>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logDateTimeAxis, "chart.axis.datetime")

// ============================================================
// 时间单位表（全局静态常量）
// ============================================================
struct TimeUnit {
    qint64 ms;          // 步长（毫秒）
    QString fmt;        // 对应的显示格式
    int subDivisions;   // 建议次刻度数
};

static const QList<TimeUnit> timeUnits = {
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

// ============================================================
// 辅助函数：根据时间间隔推断合适的显示格式
// ============================================================
static QString inferFormatFromInterval(qint64 intervalMs)
{
    // 如果间隔为 0，返回秒格式
    if (intervalMs <= 0) return "hh:mm:ss";

    for (const auto& unit : timeUnits) {
        if (unit.ms >= intervalMs) {
            return unit.fmt;
        }
    }
    // 如果超过最大单位，使用最大单位格式
    return timeUnits.last().fmt;
}

// ============================================================
// 构造函数
// ============================================================
QDateTimeAxis::QDateTimeAxis(QObject* parent, Qt::Alignment alignment)
    : QChartAxis(parent, alignment)
{
    m_tickCount = 7; // 时间轴默认 7 个刻度
}

// ============================================================
// 刻度生成
// ============================================================
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

// ============================================================
// 标签格式化（自动推断格式或使用用户自定义格式）
// ============================================================
QStringList QDateTimeAxis::tickLabels(const QVector<qreal>& ticks) const {
    QStringList labels;
    if (ticks.isEmpty()) return labels;

    // 如果用户指定了格式，直接使用
    if (!m_format.isEmpty()) {
        for (qreal t : ticks) {
            labels.append(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(t)).toString(m_format));
        }
        return labels;
    }

    // 自动推断格式：计算相邻刻度的最小间隔
    qint64 minInterval = std::numeric_limits<qint64>::max();
    for (int i = 1; i < ticks.size(); ++i) {
        qint64 diff = qAbs(static_cast<qint64>(ticks[i] - ticks[i - 1]));
        if (diff > 0 && diff < minInterval) minInterval = diff;
    }

    // 如果没有有效间隔（只有一个刻度），使用秒格式
    if (minInterval == std::numeric_limits<qint64>::max())
        minInterval = 1000; // 默认秒

    QString fmt = inferFormatFromInterval(minInterval);
    for (qreal t : ticks) {
        labels.append(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(t)).toString(fmt));
    }

    qCDebug(logDateTimeAxis) << "tickLabels: interval=" << minInterval << "ms format=" << fmt;
    return labels;
}

// ============================================================
// 次刻度
// ============================================================
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

// ============================================================
// 时间步长选择算法（使用全局单位表）
// ============================================================
QDateTimeAxis::TimeStepInfo QDateTimeAxis::chooseStep(qint64 rangeMs) const {
    if (rangeMs <= 0) {
        return { 1000LL, "hh:mm:ss", 0 };
    }

    int target = qMax(2, m_tickCount);
    qreal rawStepMs = static_cast<qreal>(rangeMs) / (target - 1);

    // 从单位表中找到第一个 >= rawStepMs 的单位
    const TimeUnit* chosen = &timeUnits.last();
    for (const auto& unit : timeUnits) {
        if (static_cast<qreal>(unit.ms) >= rawStepMs) {
            chosen = &unit;
            break;
        }
    }

    // 如果刻度过多，升一档
    qreal estTicks = static_cast<qreal>(rangeMs) / chosen->ms + 1;
    int idx = static_cast<int>(&*chosen - timeUnits.data());
    while (estTicks > target * 2.5 && idx < timeUnits.size() - 1) {
        chosen = &timeUnits[++idx];
        estTicks = static_cast<qreal>(rangeMs) / chosen->ms + 1;
    }

    return { chosen->ms, chosen->fmt, chosen->subDivisions };
}