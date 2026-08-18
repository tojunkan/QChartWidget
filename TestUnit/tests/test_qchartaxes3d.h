// test_qchartaxes3d.h —— QChartAxes3D 编排器单元测试声明
// 覆盖（design_3d_axes.md §10.1 TestQChartAxes3D 全部 8 例）：
//   boxCorners 约定 / boxEdges 12 / spineEdges / ticks 复用 Axis / tickAnchor
//   / markerSizePx 默认与独立 / labels 复用 Axis / per-dim 配置独立
#pragma once
#include <QObject>

class TestQChartAxes3D : public QObject {
    Q_OBJECT
private slots:
    void boxCorners_convention();    // 8 角 = dataMin/dataMax 按 index=u|v<<1|w<<2
    void boxEdges_12();              // 12 条边、u/v/w 各 4 平行、端点恰差一个分量
    void spineEdges_fromMinCorner(); // 3 条 spine 均含角 0、分别沿 u/v/w
    void ticks_reuseAxis();          // ticks == axis->tickValues（复用回归）
    void tickAnchor_onSpine();       // anchor.dim==tickValue、其余==dataMin
    void markerSizePx_default();     // 默认 4.0、可配、per-dim 独立
    void labels_reuseAxis();         // tickLabelTexts == axis->tickLabels
    void axisConfig_perDim();        // 每维 visible/markerSize/labelOffset 独立生效
};
