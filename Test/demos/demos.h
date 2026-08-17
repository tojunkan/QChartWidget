// demos.h —— 演示窗口统一入口
// 每个 buildDemoXxx() 返回一个独立的演示窗口（parent=nullptr）
// 签名统一为 QWidget*（2D demo 返回 QChartWidget*，派生→基类指针隐式升级；
// surface3d 返回双 Widget 容器）。main 按需调用，想跑哪个调哪个
#pragma once
class QWidget;

QWidget* buildDemoPolar();     // Polar 五边形（曲线边验证）
QWidget* buildDemoBar();       // Cartesian 柱状图
QWidget* buildDemoPendulum();  // 单摆动画（Generator 模式）
QWidget* buildDemoSort();      // 冒泡排序动画
QWidget* buildDemoCamera();    // 相机漫游
QWidget* buildDemoSwirl();     // 投影切换（恒等 ↔ Swirl）
QWidget* buildDemoStress();    // 折线粗筛压力（1M 点缩放）
QWidget* buildDemoTheme();     // 深色模式（主题 + 图例 + 一键导出）
QWidget* buildDemoScatter3D(); // 3D 散点（球面采样，柱面/球面投影切换）
QWidget* buildDemoLine3D();    // 3D 参数螺旋线（相机沿路径动画）
QWidget* buildDemoSurface3D(); // 参数曲面（球面/莫比乌斯 + 双 Widget 联动）
