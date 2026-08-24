// QChartAnimation.h —— 动画基类
// 继承 QAbstractAnimation → 自带 start/stop/状态机/finished、兼容 Q*AnimationGroup
// 子类只需实现 animate(easedAlpha)，alpha∈[0,1] 已过 QEasingCurve
#pragma once
#include <QAbstractAnimation>
#include <QEasingCurve>

class QChartAnimation : public QAbstractAnimation {
    Q_OBJECT
public:
    QChartAnimation(QObject* parent = nullptr);

    int duration() const override { return m_durationMs; }
    void setDuration(int ms) { m_durationMs = ms; }

    void setEasingCurve(const QEasingCurve& c) { m_easing = c; }
    QEasingCurve easingCurve() const { return m_easing; }

protected:
    /// updateCurrentTime: Qt 动画驱动每帧调用。currentTime∈[0, duration]
    void updateCurrentTime(int currentTime) override;

    /// 子类核心：easedAlpha∈[0,1]，实现在此更新目标对象
    virtual void animate(qreal easedAlpha) = 0;

    int m_durationMs = 1000;
    QEasingCurve m_easing{QEasingCurve::InOutCubic};
};
