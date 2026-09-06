// test_axis_pipeline.h —— S0 轴渲染管线单元测试（CPU/QPainter 后端）
// 组装：QValueAxis + 2D 投影 → axis->drawAtPosition 提交 Numeric 图元 →
//      QChartScene → QChartCamera(2D) → QPainterChartRenderer::render(scene, QImage)
// 覆盖：Cartesian/Polar 轴脊与刻度点真实落屏（offscreen 像素断言）、
//      网格开/关 × 标签开/关组合。
#pragma once
#include <QObject>

class TestAxisPipeline : public QObject
{
    Q_OBJECT
private slots:
    void cartesianSpineTicksRender();      // 笛卡尔：轴脊 + 刻度点像素 + 非空冒烟
    void cartesianGridLabelCombos();       // 笛卡尔：网格 × 标签 组合
    void polarSpineTicksRender();          // 极坐标：外环 + 半径脊 + 刻度点像素
    void polarGridLabelCombos();           // 极坐标：网格 × 标签 组合
};
