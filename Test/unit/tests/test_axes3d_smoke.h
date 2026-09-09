// test_axes3d_smoke.h —— 批次 B1：3D 轴渲染冒烟（CPU offscreen）
#pragma once
#include <QObject>

class TestAxes3DSmoke : public QObject
{
    Q_OBJECT
private slots:
    void cpuBoxAndLatticeRender();   // Box/Lattice 两模式 CPU 出图（盒边/脊/刻度/标签存在性）
};
