// QPainterChartRenderer.h —— QPainter 后端渲染器
#ifndef QPAINTERCHARTRENDERER_H
#define QPAINTERCHARTRENDERER_H

#include "QChartRenderer.h"

class QPainterChartRenderer : public QChartRenderer
{
public:
    QPainterChartRenderer() = default;
    ~QPainterChartRenderer() override = default;

protected:
    // ===== 基类虚函数实现 =====

    /// Numeric → Cartesian 变换（CPU 端调用 projection->toCartesian）
    void transformNumericToCartesian(QChartScene& scene) override;

    /// 裁剪 + 标签解析（精确裁剪，填充 m_visibilityCache）
    void cullAndResolveLabels(QChartScene& scene) override;

    /// 绘制图元（从 cart* 转像素，用 QPainter 画）
    void drawPrimitives(QChartScene& scene,
                        QPaintDevice* device,
                        const QVector<bool>& visibility) override;

    /// 绘制标签（从 cartesianAnchor 转像素，用 QPainter 画文字）
    void drawLabels(QChartScene& scene,
                    QPaintDevice* device) override;

private:
    // ===== 裁剪辅助 =====

    /// 判断图元是否在视口内（2D/3D 通过 camera 多态区分）
    bool isPrimitiveVisible(const QChartPrimitive& prim,
                            const QChartAbstractCamera* camera) const;

    /// 2D 裁剪：图元与 QRectF 相交测试
    bool isPrimitiveVisible2D(const QChartPrimitive& prim, const QRectF& viewRect) const;

    // ===== 绘制辅助 =====
    // S0：CPU 渲染器只含 2D 路径（QChartCamera3D 未入 S0 子集）；
    //     3D 的 drawPrimitives3D/drawLabels3D/isPrimitiveVisible3D 随 3D 阶段恢复。

    void drawPrimitives2D(QPainter& painter,
                          const QChartScene& scene,
                          const QChartCamera* cam2d);

    void drawLabels2D(QPainter& painter,
                      const QChartScene& scene,
                      const QChartCamera* cam2d);
};

#endif // QPAINTERCHARTRENDERER_H