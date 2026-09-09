// test_axis_edge.h —— 批次 B3：drawAtEdge 专项（unit，offscreen）
#pragma once
#include <QObject>

class TestAxisEdge : public QObject
{
    Q_OBJECT
private slots:
    void fourDirectionsRenderOutside();   // Top/Bottom/Left/Right：轴线贴缘+外带墨迹
    void nonBorderAlignmentRejected();    // HCenter/VCenter：qWarning 拒绝且零绘制
    void styleParamsEffective();          // 颜色/tickCount 生效抽查
};
