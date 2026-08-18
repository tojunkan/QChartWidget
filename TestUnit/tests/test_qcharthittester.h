// test_qcharthittester.h —— QChartHitTester 统一命中引擎单元测试声明（Phase 3 任务 0）
// 覆盖：2D 委托回归（与 test_hittest 行为一致：命中/未命中/顶层优先/dataIndex==index）
//       + 3D 近邻单测（命中/未命中/8px 阈值/多图元最近/点到线段距离/Grid 与 Decor 层排除/dataIndex 透传）
#pragma once
#include <QObject>

class TestQChartHitTester : public QObject {
    Q_OBJECT
private slots:
    void hit2d_delegation();       // 2D：委托层 hitTest 与直调一致（命中/未命中/dataIndex==index）
    void hit2d_topLayerPriority(); // 2D：顶层可见系列优先
    void hit3d_pointNearest();     // 3D：点命中/未命中/8px 阈值
    void hit3d_segmentDistance();  // 3D：点到线段距离（含端点外投影 clamp）
    void hit3d_layerFilter();      // 3D：Grid/ForegroundDecor 排除 + dataIndex 透传 + dataIndex<0 排除
    void hit3d_multiPrimitive();   // 3D：多图元取最近

    // ===== GPU 拾取解码（design_phase3.md §8.1，t46；纯函数，无 GL 依赖）=====
    void hitTestGPU_normal();        // RGB24→ID→查表→HitResult（series/dataIndex/index）
    void hitTestGPU_sentinel();      // 0xFFFFFF（背景/轴网格 Decor 哨兵）→ 空
    void hitTestGPU_outOfRange();    // 越界/空表 → 空
    void hitTestGPU_crossCPU();      // 简单场景两后端一致（GPU 表解码 vs CPU 近邻，§8.2 交叉验证）
};
