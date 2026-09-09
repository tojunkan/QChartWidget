// demos.h —— 演示窗口统一入口（批次 B4：轴阶段 demo）
// 每个 buildDemoXxx(backend) 返回独立演示窗口（parent=nullptr，堆上创建由 main 收管）。
// backend: Cpu=QPainter 后端；Gl=OpenGL 后端（plotArea 对齐 GL 宿主）。
#pragma once
#include <functional>

class QWidget;

enum class DemoBackend { Cpu, Gl };

QWidget* buildDemoAxis(DemoBackend backend);      // 2D：Cartesian value×value + 网格 + 边框轴 + 标签
QWidget* buildDemoAxis3D(DemoBackend backend);    // 3D：三轴 + Lattice 网格 + 域盒 + 初始姿态

struct DemoEntry {
    const char* name;
    std::function<QWidget*(DemoBackend)> build;
};
