// test_widget3d_smoke.h —— 批次 B2：QChartWidget3D 轻量 3D 容器冒烟（offscreen CPU）
#pragma once
#include <QObject>

class TestWidget3DSmoke : public QObject
{
    Q_OBJECT
private slots:
    void cpuWidget3DRenders();   // 3D 容器 CPU 出图（盒+网格+刻度墨迹存在性）
};
