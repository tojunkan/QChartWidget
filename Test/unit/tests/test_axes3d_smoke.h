// test_axes3d_smoke.h —— 批次 B1/B2：3D 轴渲染冒烟（CPU offscreen）
#pragma once
#include <QObject>

class TestAxes3DSmoke : public QObject
{
    Q_OBJECT
private slots:
    void cpuThreeGridModes();             // 批次2 B：Box/FaceLine/Lattice 三模式（图元/标签契约 + 出图）
    void cpuFaceLineSafeOnSpherical();    // 批次2 B：面线模式退化安全（球坐标 θ=φ=0；无盒边/网格）
    void boxModeFallbackOnNonCartesian(); // 批次2 B：Box + 非直角投影 → qWarning + 自动回退 FaceLine
};
