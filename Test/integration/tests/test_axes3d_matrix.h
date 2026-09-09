// test_axes3d_matrix.h —— 批次 B3：3D 轴矩阵（CPU/GL 各 8 组合）+ spherical 冒烟
#pragma once
#include <QObject>

class TestAxes3dMatrixCpu : public QObject
{
    Q_OBJECT
private slots:
    void cpuMatrix8();      // Cartesian3D × {Box,Lattice} × {姿态A(0,0), 姿态B(45,30)}
    void cpuSpherical();    // spherical 3D CPU 冒烟（非恒等投影）
};

class TestAxes3dMatrixGl : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();    // offscreen → QSKIP
    void glMatrix8();       // 同 8 组合（wayland 实跑）
    void glSpherical();     // spherical 3D GL 冒烟
};
