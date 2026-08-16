// demos.h —— 演示窗口统一入口
// 每个 buildDemoXxx() 返回一个独立的 QChartWidget 演示窗口（parent=nullptr）
// main 按需调用，想跑哪个调哪个
#pragma once
class QChartWidget;

QChartWidget* buildDemoPolar();     // Polar 五边形（曲线边验证）
QChartWidget* buildDemoBar();       // Cartesian 柱状图
QChartWidget* buildDemoPendulum();  // 单摆动画（Generator 模式）
QChartWidget* buildDemoSort();      // 冒泡排序动画
QChartWidget* buildDemoCamera();    // 相机漫游
QChartWidget* buildDemoSwirl();     // 投影切换（恒等 ↔ Swirl）
QChartWidget* buildDemoStress();    // 折线粗筛压力（1M 点缩放）
