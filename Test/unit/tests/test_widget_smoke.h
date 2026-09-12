// test_widget_smoke.h —— 批次 A widget 容器化冒烟（offscreen CPU）
#pragma once
#include <QObject>

class TestWidgetSmoke : public QObject
{
    Q_OBJECT
private slots:
    void cpuContainerRenders();        // CPU：plotArea 扣除边距 + 网格/边框轴真实出图 + 广播
    void gridSingleLabelPerSpine();    // 批次2 A：网格脊单标签（自由标签契约/数量=脊数/值轴文字来源/组尾定位）
    void gridSpineLabelModePolicy();   // 批次2 A：图层决定哪些脊用单标签模式（策略钩子 None/Single/Tickwise）

    // ---- 4b：轴=范围唯一持有者 + dataBounds 按需组装（数值断言，与 4a 基线一致）----
    void axisRangeAndDataBoundsContract();

    // ---- 4e：三向驱动链（方向状态 / 计数防回环 / 幂等 / 极端输入 / 像素侧重 fit）----
    void driveChainContract();

    // ---- 4f：鼠标交互（二维平移/缩放 + 开关 + 计数）----
    void mouseInteractionContract();

    // ---- 4g：脏模型（视图变化重收集 / 无变化跳过 / 数据变化仍重收集 + 背景随范围更新）----
    void dirtyModelContract();
};
