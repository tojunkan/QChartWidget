// QInterpolatedProjection.cpp —— 合成投影实现
#include "QInterpolatedProjection.h"
#include "QChartDebug.h"
#include <QDebug>
#include <cmath>

// logProjection 类别定义：留驻本文件（t8 定案）。
// 背景：原定义在旧 QChartWidget.cpp（未纳入 S0 子集），而 QPolarProjection 等
// 2D 投影 header 内 qCDebug(logProjection) 需要其定义才能链接，S0 时迁入本文件；
// 批次 A 后 QChartWidget.cpp 已重写为容器形态并回编，其内仅定义 logWidget
//（"chart.widget"，不同类别）→ logProjection 定义留驻本文件，不再迁回。
Q_LOGGING_CATEGORY(logProjection, "chart.projection")

QInterpolatedProjection::QInterpolatedProjection(QChartProjection* a, QChartProjection* b)
    : m_a(a), m_b(b) {}

// ===== 核心：两投影结果之间 lerp =====
// NaN 自然传播——若任一投影在奇点返回 NaN，插值结果也是 NaN，路径会断开
QPointF QInterpolatedProjection::toCartesian(qreal num0, qreal num1) const {
    QPointF pa = m_a ? m_a->toCartesian(num0, num1) : QPointF(num0, num1);
    QPointF pb = m_b ? m_b->toCartesian(num0, num1) : QPointF(num0, num1);

    // 混合：lerp(a, b, alpha)
    qreal t = qBound(0.0, m_alpha, 1.0);
    qreal x = pa.x() + (pb.x() - pa.x()) * t;
    qreal y = pa.y() + (pb.y() - pa.y()) * t;
    return QPointF(x, y);
}

QPointF QInterpolatedProjection::fromCartesian(qreal x, qreal y) const {
    QPointF pa = m_a ? m_a->fromCartesian(x, y) : QPointF(x, y);
    QPointF pb = m_b ? m_b->fromCartesian(x, y) : QPointF(x, y);
    qreal t = qBound(0.0, m_alpha, 1.0);
    return QPointF(pa.x() + (pb.x() - pa.x()) * t,
                   pa.y() + (pb.y() - pa.y()) * t);
}

// ===== 包络委托给目标投影 B =====
// 动画期间刻度生成基于 B 的最终范围即可（过渡过程本身不做 pan/zoom）
QRectF QInterpolatedProjection::computeDataBounds(const QRectF& viewRect) const {
    return m_b ? m_b->computeDataBounds(viewRect) : viewRect;
}

QRectF QInterpolatedProjection::computeViewRect(const QRectF& dataBounds) const {
    return m_b ? m_b->computeViewRect(dataBounds) : dataBounds;
}
