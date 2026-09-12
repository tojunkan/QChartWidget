// test_widget3d_smoke.h —— 批次 B2：QChartWidget3D 轻量 3D 容器冒烟（offscreen CPU）
#pragma once
#include <QObject>

class TestWidget3DSmoke : public QObject
{
    Q_OBJECT
private slots:
    void cpuWidget3DRenders();      // 3D 容器 CPU 出图（Box 模式：盒+网格+刻度墨迹存在性）
    void cpuWidget3DGridModes();    // 批次2 B：widget 三模式入口转发 + 默认值 + Box 非直角回退

    // ---- 4b：三维数据盒不再独立持有（由三根轴范围组装）----
    void dataBoundsFromAxesContract();

    // ---- 4c：QCube 工具最小集 + 盒子 API QCube 化 ----
    void cubeToolsAndBoxApiContract();

    // ---- 4d：相机自动适配契约（autoFit 三路径 / 掩码四约束 / override 保护 / pitch 钳制）----
    void cameraFitContract();

    // ---- 4e：像素侧驱动（plotArea→重解算镜头）+ 竖高视口 8 角点硬证据 ----
    void plotAreaDriveContract();

    // ---- 4f：鼠标交互（orbit / dolly+autoFit 联动 / panViewCube / 开关 / 计数）----
    void mouseInteraction3DContract();
};
