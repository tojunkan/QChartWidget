// QNumericSeriesAnimation.cpp —— 数值点集动画实现
#include "QNumericSeriesAnimation.h"
#include "QXYSeries.h"
#include <QDebug>

QNumericSeriesAnimation::QNumericSeriesAnimation(QObject* parent)
    : QChartAnimation(parent) {}

// ===== 模式 A：从当前数据快照源点集 =====
void QNumericSeriesAnimation::setTargetPoints(const QVector<QPointF>& numericPts) {
    m_dstPoints = numericPts;
    m_useGenerator = false;
    // 快照源：从 Series 当前数据转 Numeric
    // 注意：Series 存的是 Data(QVariant)，无 Axis 无法转 Numeric
    // 源点集由调用者在 start() 前通过外部 setSourcePoints 快照
}

// ===== 模式 B：物理生成器 =====
void QNumericSeriesAnimation::setGenerator(Generator gen) {
    m_gen = std::move(gen);
    m_useGenerator = true;
}

// ===== 每帧更新 =====
void QNumericSeriesAnimation::animate(qreal alpha) {
    if (!m_series) return;

    if (m_useGenerator && m_gen) {
        // Generator 直接产出当前帧点集
        m_gen(alpha, m_tempPoints);
    } else {
        // Numeric 空间逐点 lerp
        int n = qMin(m_srcPoints.size(), m_dstPoints.size());
        m_tempPoints.resize(n);
        for (int i = 0; i < n; ++i)
            m_tempPoints[i] = m_srcPoints[i] + (m_dstPoints[i] - m_srcPoints[i]) * alpha;
    }

    m_series->setRenderOverride(m_tempPoints);
}
