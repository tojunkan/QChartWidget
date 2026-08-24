// QProjectionSwitchAnimation.h —— 投影切换动画
// 把一个投影过渡到另一个：创建 QInterpolatedProjection 临时挂到 Widget，
// 每帧推进 blend；结束后把目标投影所有权交给 Widget 落地。
#pragma once
#include "QChartAnimation.h"
#include <memory>

class QChartWidget;
class QChartProjection;
class QInterpolatedProjection;

class QProjectionSwitchAnimation : public QChartAnimation {
    Q_OBJECT
public:
    explicit QProjectionSwitchAnimation(QObject* parent = nullptr);
    ~QProjectionSwitchAnimation() override;

    void setTargetWidget(QChartWidget* w) { m_widget = w; }
    QChartWidget* targetWidget() const { return m_widget; }

    /// 目标投影：动画结束时所有权转移给 Widget
    void setTargetProjection(std::unique_ptr<QChartProjection> dst);

protected:
    void animate(qreal alpha) override;
    void updateState(QAbstractAnimation::State newState,
                     QAbstractAnimation::State oldState) override;

private:
    QChartWidget* m_widget = nullptr;
    std::unique_ptr<QChartProjection> m_dst;
    QInterpolatedProjection* m_interp = nullptr; // 动画期间持有，结束后删除
};
