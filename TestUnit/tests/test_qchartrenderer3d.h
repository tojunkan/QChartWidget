// test_qchartrenderer3d.h —— Renderer 3D 路径单元测试声明
// 覆盖（design_3d.md §11.1 TestQChartRenderer3D 全部 6 组）：
//   line 图元计数与 NaN 断段 / surface 线框计数 / depthSort 降序远→近
//   / render3d offscreen 非空白 / nearCoversFar 近者覆盖 / scene_is3D 检测
#pragma once
#include <QObject>

class TestQChartRenderer3D : public QObject {
    Q_OBJECT
private slots:
    void line_collectPrimitives_count();   // n 点 → n-1 线段；NaN 断段正确
    void surface_wireframeCount();         // rows×cols → rows·(cols-1) + cols·(rows-1)
    void depthSort_farToNear();            // 已知深度图元 → 降序，近者在后
    void render3d_offscreen_ok();          // offscreen QImage 渲染不崩、非空白
    void render3d_nearCoversFar();         // 两重叠线段（近红远蓝）→ 顶部像素为近者色
    void scene_is3D_detection();           // camera3D 非空 ↔ is3D()
};
