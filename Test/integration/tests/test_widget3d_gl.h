// test_widget3d_gl.h —— 批次 B2：QChartWidget3D GL 实跑冒烟（wayland；offscreen QSKIP）
#pragma once
#include <QObject>

class TestWidget3DGl : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();       // offscreen → QSKIP
    void glWidget3DRenders();  // 3D 容器走 plotArea 对齐 GL 宿主出图（FBO 取证）
};
