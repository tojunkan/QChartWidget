// QChartLineSeries3D.h —— 3D 折线系列（用户点名，不用 Curve）
// collectPrimitives：相邻有效点成 LineSegment；任一端投影 screen 非有限 → 断段（延续 createPath 断路径语义）；
// 深度 = 两端点 depth 均值（非线性投影下不得投影 Data 中点，队长裁决 a）；dataIndex = 线段起点索引（裁决 c）。
// cullingEnabled 默认 true，沿用 2D QLineSeries 语义（屏外线段跳过）——本层无 plotArea，
// 属性存留由 Renderer 3D 路径（t11）按屏矩形执行剔除；collect 阶段输出全部有效段（QPainter 会裁剪）。
#pragma once
#include "QChartSeries3D.h"

class QChartLineSeries3D : public QChartSeries3D {
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(bool cullingEnabled READ isCullingEnabled WRITE setCullingEnabled NOTIFY cullingChanged)
public:
    explicit QChartLineSeries3D(const QString& name = {}, QObject* parent = nullptr);

    qreal lineWidth() const { return m_lineWidth; }     // 默认 2.0
    void setLineWidth(qreal w);
    bool isCullingEnabled() const { return m_cullingEnabled; }  // 默认 true
    void setCullingEnabled(bool v);

    void collectPrimitives(const ProjectFn3D& projectFn,
                           QVector<QChartPrimitive>& out) const override;
    void draw(QPainter* painter, const ProjectFn3D& projectFn,
              const DrawContext3D* ctx = nullptr) const override;

signals:
    void lineWidthChanged();
    void cullingChanged();

private:
    qreal m_lineWidth = 2.0;
    bool m_cullingEnabled = true;
};
