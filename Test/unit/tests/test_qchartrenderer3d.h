// test_qchartrenderer3d.h —— Renderer 3D 路径单元测试声明
// 覆盖（design_3d.md §11.1 TestQChartRenderer3D 6 组 + design_3d_axes.md §10.2 3 组）：
//   line 图元计数与 NaN 断段 / surface 线框计数 / depthSort 降序远→近
//   / render3d offscreen 非空白 / nearCoversFar 近者覆盖 / scene_is3D 检测
//   / boxMode 图元计数与分层 / 晶格三族行数 / identity 快速通道（段数）
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
    void boxMode_primitiveCounts();        // 盒 12 边 + 地板 2×(T+1) 线 + tick 点 3×(T+1)，Layer 归属
    void lattice_rowCounts();              // 晶格三族行数公式
    void identityFastPath_layer3d();       // Cartesian3D 每线 2 段 vs 球面 32 段
    void gridBehindSeries_pixel();         // 后方网格被系列盖住（像素 = 系列色）
    void gridInFrontOfSeries_pixel();      // 前方网格遮挡系列（像素 = 网格色）
    void gridTie_seriesWins();             // 同深度处系列优先（kGridDepthBias 生效）
    void decorAlwaysOnTop();               // 盒边/spine 恒在系列之上（像素可见）
    void axesToggle_zero();                // axes3D 关闭 → 无 grid/decor/labels 图元与轴色像素
    void dataBounds3D_viewCubeReverse();   // Cartesian 快速通道：反算 == viewCube 本身
    void dataBounds3D_gridSampling_curved();// 柱坐标 5³ 采样捕获棱中点极值（8 角会漏）
    void dataBounds3D_noBackward_fallback();// Functional 无反向 → Valid=false → 域盒静态
};
