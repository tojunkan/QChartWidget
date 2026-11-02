// demos.h —— 演示窗口统一入口（批次 B4：轴阶段 demo）
// 每个 buildDemoXxx(backend) 返回独立演示窗口（parent=nullptr，堆上创建由 main 收管）。
// backend: Cpu=QPainter 后端；Gl=OpenGL 后端（plotArea 对齐 GL 宿主）。
#pragma once
#include <functional>

class QWidget;

enum class DemoBackend { Cpu, Gl };

QWidget* buildDemoAxis(DemoBackend backend);      // 2D：Cartesian value×value + 网格 + 边框轴 + 标签
QWidget* buildDemoAxis3D(DemoBackend backend);    // 3D：三轴 + Lattice 网格 + 域盒 + 初始姿态
QWidget* buildDemoScatter(DemoBackend backend);   // 2D 散点：同一份代码只换投影指针（cartesian|polar）
QWidget* buildDemoScatter3D(DemoBackend backend); // 3D 散点：收集 Numeric 图元，渲染统一由 renderer
QWidget* buildDemoLine(DemoBackend backend);      // 2D 折线：Numeric 直线段 + NaN 断线（cartesian|polar）
QWidget* buildDemoLine3D(DemoBackend backend);    // 3D 折线：螺旋（Numeric 直线段）
QWidget* buildDemoPolygon(DemoBackend backend);   // 2D 多边形：填充（耳切）+ 描边 + NaN 断段
QWidget* buildDemoBar(DemoBackend backend);       // 2D 柱状：Data 矩形 → Rect 图元（cartesian|polar）
QWidget* buildDemoSurface3D(DemoBackend backend); // 3D 曲面：网格 → TriangleMesh（纯色剪影，建议只跑 GL）

struct DemoEntry {
    const char* name;
    std::function<QWidget*(DemoBackend)> build;
};
