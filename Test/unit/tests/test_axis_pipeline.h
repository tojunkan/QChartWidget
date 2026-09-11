// test_axis_pipeline.h —— S0 轴渲染管线单元测试（CPU/QPainter 后端）
// 组装：QValueAxis + 2D 投影 → axis->drawAtPosition 提交 Numeric 图元 →
//      QChartScene → QChartCamera(2D) → QPainterChartRenderer::render(scene, QImage)
// 覆盖：Cartesian/Polar 轴脊与刻度点真实落屏（offscreen 像素断言）、
//      网格开/关 × 标签开/关组合、批次1 标签契约内核（LabelMode 三态 + 锚点三级优先级 + NaN 哨兵）。
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

    // ---- 批次1：标签契约内核 ----
    void labelModeThreeStates();           // LabelMode 三态标签数量（Single=中间刻度；默认参=Tickwise）
    void anchorTierPriority();             // tier1 显式锚点 > tier2 refId 绑定 > tier3 自由标签组尾
    void anchorVisibilityRules();          // NaN 分量判定 + 负例（组内无可见图元/越界绑定/plotArea 外）
    void glAnchorTierResolution();         // GL 后端 cull 同契约（无 GL context 下的步骤 2 解析）
    void nanAnchorRuleLocked();            // 批次2 C③：未设置必须写 NaN；{0,0,0}=显式坐标（原点）锁定
};
