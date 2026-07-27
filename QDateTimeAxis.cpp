#include "QDateTimeAxis.h"
#include <QDebug>
#include <QtMath>

// ===== 构造函数 =====
QDateTimeAxis::QDateTimeAxis(QObject* parent, Qt::Alignment alignment)
    : QChartAxis(parent, alignment)
{
    // 直接初始化基类的 m_min / m_max（纪元秒）
    QDateTime now = QDateTime::currentDateTime();
    m_min = toEpoch(now);
    m_max = toEpoch(now.addDays(1));  // 默认范围：今天 ~ 明天
    m_tickCount = 7;
    m_format.clear(); // 空表示自动
}

CoordinateSystem QDateTimeAxis::coordinateSystem() const { return CoordinateSystem::Cartesian; }

// ===== 业务层便利接口 =====
void QDateTimeAxis::setRange(const QDateTime& min, const QDateTime& max) {
    if (min > max) return;
    // 直接调用基类的 setRange，只发一次信号
    QChartAxis::setRange(toEpoch(min), toEpoch(max));
}

QDateTime QDateTimeAxis::dateTimeMin() const {
    return QDateTime::fromSecsSinceEpoch(qint64(m_min));
}

QDateTime QDateTimeAxis::dateTimeMax() const {
    return QDateTime::fromSecsSinceEpoch(qint64(m_max));
}

// ===== 重写基类接口（防止负数时间戳） =====
void QDateTimeAxis::setMin(qreal v) {
    if (v < 0) v = 0;
    QChartAxis::setMin(v); // 调用基类
}

void QDateTimeAxis::setMax(qreal v) {
    if (v < 0) v = 0;
    QChartAxis::setMax(v);
}

// ===== 核心映射（线性，基于 m_min/m_max） =====
qreal QDateTimeAxis::valueToNormalized(qreal value) const {
    if (qFuzzyCompare(m_max, m_min)) return 0.0;
    return (value - m_min) / (m_max - m_min);
}

qreal QDateTimeAxis::normalizedToValue(qreal norm) const {
    return m_min + norm * (m_max - m_min);
}

// ===== 对齐工具：将 QDateTime 向下对齐到步长的整数倍（从 1970-01-01 开始） =====
QDateTime QDateTimeAxis::floorToStep(const QDateTime& dt, qreal stepSeconds) {
    qint64 epoch = dt.toSecsSinceEpoch();
    qint64 step = qint64(stepSeconds);
    if (step <= 0) return dt;
    qint64 remainder = epoch % step;
    qint64 floored = epoch - remainder;
    return QDateTime::fromSecsSinceEpoch(floored);
}

// ===== 计算美观的步长与格式（完全基于 m_min/m_max 的差值） =====
QDateTimeAxis::TimeStepInfo QDateTimeAxis::calculateStepInfo() const {
    TimeStepInfo info;
    info.subDivisions = 0;

    qreal rangeSecs = m_max - m_min;
    if (rangeSecs <= 0) {
        info.stepSeconds = 1.0;
        info.format = "yyyy-MM-dd hh:mm:ss";
        return info;
    }

    int targetTicks = qMax(2, m_tickCount);
    qreal rawStep = rangeSecs / (targetTicks - 1);

    // 定义时间单位列表（按升序）
    struct Unit { qreal seconds; QString format; int subTicks; };
    QList<Unit> units = {
        { 1,          "hh:mm:ss",       0 },
        { 5,          "hh:mm:ss",       0 },
        { 10,         "hh:mm:ss",       0 },
        { 15,         "hh:mm:ss",       0 },
        { 30,         "hh:mm:ss",       0 },
        { 60,         "hh:mm",          0 },
        { 120,        "hh:mm",          0 },
        { 300,        "hh:mm",          0 },
        { 600,        "hh:mm",          0 },
        { 900,        "hh:mm",          0 },
        { 1800,       "hh:mm",          0 },
        { 3600,       "hh:mm",          0 },
        { 7200,       "hh:mm",          0 },
        { 10800,      "hh:mm",          0 },
        { 21600,      "hh:mm",          0 },
        { 43200,      "hh:mm",          0 },
        { 86400,      "MM-dd",          0 },
        { 172800,     "MM-dd",          0 },
        { 259200,     "MM-dd",          0 },
        { 604800,     "MM-dd",          0 },
        { 1209600,    "MM-dd",          0 },
        { 2592000,    "yyyy-MM",        0 },
        { 7776000,    "yyyy-MM",        0 },
        { 15552000,   "yyyy-MM",        0 },
        { 31536000,   "yyyy",           0 }
    };

    // 选择第一个 >= rawStep 的单位
    Unit chosen = units.last();
    for (const Unit& u : units) {
        if (u.seconds >= rawStep) {
            chosen = u;
            break;
        }
    }

    // 如果步长导致刻度数过多，则尝试更大的单位
    int estimatedTicks = qCeil(rangeSecs / chosen.seconds) + 1;
    int idx = 0;
    for (int i = 0; i < units.size(); ++i) {
        if (units[i].seconds == chosen.seconds && units[i].format == chosen.format) {
            idx = i;
            break;
        }
    }
    while (estimatedTicks > targetTicks * 2 && idx < units.size() - 1) {
        chosen = units[++idx];
        estimatedTicks = qCeil(rangeSecs / chosen.seconds) + 1;
    }

    info.stepSeconds = chosen.seconds;
    info.format = chosen.format;

    // 设置次刻度数（辅助网格线）
    if (chosen.seconds >= 86400) info.subDivisions = 4;
    else if (chosen.seconds >= 3600) info.subDivisions = 3;
    else if (chosen.seconds >= 60) info.subDivisions = 4;
    else info.subDivisions = 0;

    return info;
}

// ===== 生成主刻度值（返回纪元秒列表） =====
QVector<qreal> QDateTimeAxis::tickValues() const {
    QVector<qreal> ticks;
    TimeStepInfo info = calculateStepInfo();
    qreal step = info.stepSeconds;
    if (step <= 0) return ticks;

    // 将 m_min 对齐到步长整数倍（转成 QDateTime 对齐后再转回 qreal）
    QDateTime startDt = floorToStep(QDateTime::fromSecsSinceEpoch(qint64(m_min)), step);
    qreal start = toEpoch(startDt);
    if (start < m_min) {
        start += step;
    }

    // 循环生成刻度
    qreal current = start;
    while (current <= m_max + step * 0.0001) {
        if (current >= m_min && current <= m_max) {
            ticks.append(current);
        }
        current += step;
        if (current > m_max + step) break; // 防止死循环
    }

    // 保证至少有两个刻度
    if (ticks.size() < 2) {
        ticks.clear();
        ticks.append(m_min);
        ticks.append(m_max);
    }
    return ticks;
}

// ===== 生成主刻度标签（把纪元秒转成漂亮的日期字符串） =====
QStringList QDateTimeAxis::tickLabels() const {
    QStringList labels;
    QVector<qreal> values = tickValues();
    TimeStepInfo info = calculateStepInfo();
    QString fmt = m_format.isEmpty() ? info.format : m_format;

    for (qreal epoch : values) {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(qint64(epoch));
        labels.append(dt.toString(fmt));
    }
    return labels;
}

// ===== 生成次刻度值（在主刻度之间均匀插值） =====
QVector<qreal> QDateTimeAxis::subTickValues() const {
    QVector<qreal> subTicks;
    TimeStepInfo info = calculateStepInfo();
    int subDiv = info.subDivisions;
    if (subDiv <= 0) return subTicks;

    QVector<qreal> mains = tickValues();
    if (mains.size() < 2) return subTicks;

    qreal mainStep = info.stepSeconds;   // 主刻度步长（秒）
    qreal subStep = mainStep / (subDiv + 1);

    // 辅助 lambda：尝试添加一个次刻度，但避开主刻度位置（防止重叠）
    auto tryAdd = [&](qreal val) {
        if (val < m_min || val > m_max) return;
        // 检查是否与某个主刻度过于接近（浮点误差容忍）
        for (qreal m : mains) {
            if (qFabs(val - m) < 1e-9) return;
        }
        subTicks.append(val);
        };

    // 1. 在主刻度之间生成
    for (int i = 0; i < mains.size() - 1; ++i) {
        qreal start = mains[i];
        qreal end = mains[i + 1];
        for (int j = 1; j <= subDiv; ++j) {
            tryAdd(start + j * subStep);
        }
    }

    // 2. 在最小主刻度左侧区间 [m_min, mains.first()] 生成
    qreal firstMain = mains.first();
    if (firstMain > m_min) {
        // 从 m_min 向上对齐到 subStep 的倍数
        qreal start = std::ceil(m_min / subStep) * subStep;
        for (qreal val = start; val < firstMain - 1e-9; val += subStep) {
            tryAdd(val);
        }
    }

    // 3. 在最大主刻度右侧区间 [mains.last(), m_max] 生成
    qreal lastMain = mains.last();
    if (lastMain < m_max) {
        // 从 lastMain 之后第一个 subStep 倍数开始
        qreal start = std::floor(lastMain / subStep) * subStep + subStep;
        for (qreal val = start; val <= m_max + 1e-9; val += subStep) {
            tryAdd(val);
        }
    }

    return subTicks;
}