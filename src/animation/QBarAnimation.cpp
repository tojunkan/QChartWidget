// QBarAnimation.cpp —— 柱集矩形动画实现
#include "QBarAnimation.h"
#include "QBarSeries.h"
#include <QDebug>

QBarAnimation::QBarAnimation(QObject* parent)
    : QChartAnimation(parent) {}

// ===== 模式 A：从当前数据快照源矩形 =====
void QBarAnimation::setTargetRects(const QVector<QRectF>& numericRects) {
    m_dstRects = numericRects;
    m_useGenerator = false;
    // 快照源：从 Series 当前数据转 Numeric
    // 注意：Series 存的是 Data(QVariant)，无 Axis 无法转 Numeric
    // 源矩形由调用者在 start() 前通过外部 setSourceRects 快照
}

// ===== 模式 B：生成器 =====
void QBarAnimation::setGenerator(Generator gen) {
    m_gen = std::move(gen);
    m_useGenerator = true;
}

// ===== 每帧更新 =====
void QBarAnimation::animate(qreal alpha) {
    if (!m_series) return;

    if (m_useGenerator && m_gen) {
        // Generator 直接产出当前帧矩形集
        m_gen(alpha, m_tempRects);
    } else {
        // Numeric 空间逐矩形 lerp（四边独立插值）
        int n = qMin(m_srcRects.size(), m_dstRects.size());
        m_tempRects.resize(n);
        for (int i = 0; i < n; ++i) {
            const QRectF& a = m_srcRects[i];
            const QRectF& b = m_dstRects[i];
            m_tempRects[i] = QRectF(a.left()   + (b.left()   - a.left())   * alpha,
                                    a.top()    + (b.top()    - a.top())    * alpha,
                                    a.width()  + (b.width()  - a.width())  * alpha,
                                    a.height() + (b.height() - a.height()) * alpha);
        }
    }

    m_series->setRenderOverride(m_tempRects);
}
