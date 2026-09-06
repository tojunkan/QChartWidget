// test_axis_matrix.h —— S0 轴渲染矩阵集成测试
// 组合：投影{Cartesian,Polar} × 网格{开/关} × 标签{开/关} × 后端{CPU,GL}
//   TestAxisMatrixCpu：8 组合全部在 QPainter（offscreen 可跑）下断言
//   TestAxisMatrixGl：8 组合在真实 GL（wayland/xcb）下断言；offscreen QSKIP 并记录环境
#pragma once
#include <QObject>

class TestAxisMatrixCpu : public QObject
{
    Q_OBJECT
private slots:
    void cpuMatrix();   // 8 组合（offscreen 常驻 ctest）
};

class TestAxisMatrixGl : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();    // offscreen/无 GL → QSKIP（GL 用例转 wayland/xcb 手动实跑或 Windows 侧）
    void glMatrix();        // 8 组合（真实 GL 环境）
};
