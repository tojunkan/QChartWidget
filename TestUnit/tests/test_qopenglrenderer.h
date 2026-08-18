// test_qopenglrenderer.h —— GL 渲染器（Shader 主 pass）单元测试声明
// 覆盖（design_phase3.md §13.2 部分，t44）：
//   batch_structure：图元→批次（合并规则/顶点数/baseId 连续/pointSize/深度标志，friend 直查）
//   depth_layering_pixels：Grid 偏置生效（同深度系列优先）、decor 关深度后画、不透明清屏（readback）
//   gl_qpainter_equivalence：同场景 GL 与 QPainter 输出一致（同深度系列优先——A4 硬指标）
// 运行环境（§13.2）：真实 GL（xcb/wayland）；offscreen 平台 initTestCase QSKIP 整类。
#pragma once
#include <QObject>

class TestQOpenGLRenderer : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();              // offscreen → QSKIP（§13.2 运行环境；GL 用例不判失败）
    void batch_structure();           // 图元→顶点数/合并/baseId/pointSize/深度标志
    void depth_layering_pixels();     // Grid 偏置 + decor 关深度 + 不透明清屏（像素/readback）
    void gl_qpainter_equivalence();   // 同场景 GL vs QPainter：同深度系列优先（A4 硬指标）

    // ===== ID 帧拾取（design_phase3.md §5.3/§8.1，t46）=====
    void pick_hitVisible();           // 系列线上拾取命中（dataIndex）；背景哨兵
    void pick_occlusionSentinel();    // 网格遮挡系列 → 哨兵（轴/网格不编码）；decor 位置 → 哨兵
    void pick_selfContained();        // 无主 pass（首帧前）直接拾取 → 深度正确（ID 帧自包含）
    void pick_throttle();             // 三闸：m_glReady 守卫 / 位移<1px / <16ms（§5.3）
};
