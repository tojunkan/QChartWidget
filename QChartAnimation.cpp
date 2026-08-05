// QChartAnimation.cpp —— 动画基类实现
#include "QChartAnimation.h"

QChartAnimation::QChartAnimation(QObject* parent)
    : QAbstractAnimation(parent) {}

void QChartAnimation::updateCurrentTime(int currentTime) {
    // currentTime ∈ [0, duration] → normalize → easing
    if (m_durationMs <= 0) return;
    qreal alpha = qBound(0.0, qreal(currentTime) / m_durationMs, 1.0);
    qreal eased = m_easing.valueForProgress(alpha);
    animate(eased);
}
