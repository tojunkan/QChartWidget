// QViewRectAnimation.h —— 视窗相机动画
// 驱动 QChartWidget 的 viewRect（相机位置+缩放），影响所有 Layer/Series。
// 中心路径：可选 waypoint → Quad Bézier，否则 src→dst 直线。
// 视窗大小：可选 sizeCurve（单参数，长宽比由 plotArea 决定），否则线性 lerp。
// 也可使用 Generator 模式完全自定义。
#pragma once
#include "QChartAnimation.h"
#include <QRectF>
#include <QPointF>
#include <functional>

class QChartWidget;

class QViewRectAnimation : public QChartAnimation {
    Q_OBJECT
public:
    explicit QViewRectAnimation(QObject* parent = nullptr);

    void setTargetWidget(QChartWidget* w) { m_widget = w; }
    QChartWidget* targetWidget() const { return m_widget; }

    // ── 模式 A：lerp + 可选路径/尺寸曲线 ──
    void setTargetViewRect(const QRectF& target);   // 必设：目标视窗
    void setWaypoint(const QPointF& center);         // 可选：相机中心弧线经过点
    void setSizeCurve(std::function<qreal(qreal)> curve); // 可选：视图宽度 vs α

    // ── 模式 B：完全自定义 ──
    using Generator = std::function<void(qreal alpha, QRectF& out)>;
    void setGenerator(Generator gen);

protected:
    void animate(qreal alpha) override;

private:
    QChartWidget* m_widget = nullptr;
    QRectF m_srcRect, m_dstRect;
    QPointF m_waypoint;
    bool m_hasWaypoint = false;
    std::function<qreal(qreal)> m_sizeCurve;
    Generator m_gen;
    bool m_useGenerator = false;
    qreal m_aspectRatio = 1.0; // plotArea 长宽比（start 前快照）
};
