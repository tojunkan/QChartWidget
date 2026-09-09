// test_axes3d_gl.h —— 批次 B1：3D 轴渲染 GL 冒烟（wayland 实窗；offscreen QSKIP）
#pragma once
#include <QObject>

class TestAxes3DGl : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();       // offscreen → QSKIP
    void gl3dWireframeRenders();  // 3D 相机+投影 GL 出图（u_viewProj 通路；深度关=全边可见）
};
