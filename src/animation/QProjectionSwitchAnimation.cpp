// QProjectionSwitchAnimation.cpp —— 投影切换动画实现
#include "QProjectionSwitchAnimation.h"
#include "QChartWidget.h"
#include "QChartProjection.h"
#include "QInterpolatedProjection.h"
#include "QChartDebug.h"
#include <QDebug>

QProjectionSwitchAnimation::QProjectionSwitchAnimation(QObject* parent)
    : QChartAnimation(parent) {}

QProjectionSwitchAnimation::~QProjectionSwitchAnimation() {
    // 析构时如果动画没跑完（被中止），确保临时投影清理干净
    if (m_widget && m_interp) {
        m_widget->clearTemporaryProjection();
        delete m_interp;
        m_interp = nullptr;
    }
}

void QProjectionSwitchAnimation::setTargetProjection(std::unique_ptr<QChartProjection> dst) {
    m_dst = std::move(dst);
}

// ===== 状态机钩子：start 时挂临时投影，结束时落地 =====
void QProjectionSwitchAnimation::updateState(QAbstractAnimation::State newState,
                                             QAbstractAnimation::State oldState) {
    QAbstractAnimation::updateState(newState, oldState);

    if (newState == QAbstractAnimation::Running && oldState != QAbstractAnimation::Running) {
        // start：快照当前投影为源，创建合成投影挂到 Widget
        if (!m_widget || !m_dst) {
            qWarning() << "QProjectionSwitchAnimation: widget or target projection not set";
            return;
        }
        QChartProjection* src = const_cast<QChartProjection*>(m_widget->projection());
        m_interp = new QInterpolatedProjection(src, m_dst.get());
        m_widget->setTemporaryProjection(m_interp);
        qCDebug(logWidget) << "投影切换开始: src type=" << (src ? (int)src->type() : -1)
                           << "→ dst type=" << (int)m_dst->type();
    }

    if (newState == QAbstractAnimation::Stopped && oldState == QAbstractAnimation::Running) {
        // 结束：清除临时投影，目标投影落地（所有权转移给 Widget）
        if (m_widget && m_interp) {
            m_widget->clearTemporaryProjection();
            delete m_interp;
            m_interp = nullptr;
        }
        if (m_widget && m_dst) {
            m_widget->setProjection(std::move(m_dst));
            qCDebug(logWidget) << "投影切换完成，目标投影已落地";
        }
    }
}

// ===== 每帧：推进混合因子 → 触发重绘 =====
void QProjectionSwitchAnimation::animate(qreal alpha) {
    if (!m_widget || !m_interp) return;
    m_interp->setBlend(alpha);
    // 前景（series）+ 背景（网格线/数据主脊）都要刷新：
    // 网格线是 drawAtPosition 数据主脊，依赖投影，切换时也要跟着扭曲
    m_widget->invalidateForeground();
    m_widget->invalidateBackground();
}
