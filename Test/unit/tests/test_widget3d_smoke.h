// test_widget3d_smoke.h —— 批次 B2：QChartWidget3D 轻量 3D 容器冒烟（offscreen CPU）
#pragma once
#include <QObject>

class TestWidget3DSmoke : public QObject
{
    Q_OBJECT
private slots:
    void cpuWidget3DRenders();      // 3D 容器 CPU 出图（Box 模式：盒+网格+刻度墨迹存在性）
    void cpuWidget3DGridModes();    // 批次2 B：widget 三模式入口转发 + 默认值 + Box 非直角回退
};
