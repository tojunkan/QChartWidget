// test_legend_interact.h —— 图例点击交互集成测试声明
#pragma once
#include <QObject>

class TestLegendInteract : public QObject {
    Q_OBJECT
private slots:
    void click_togglesVisibility();    // 点击图例项 → 对应 series 可见性翻转、再点复原
    void click_emptyArea_noToggle();   // 点 plotArea 空白不切换
};
