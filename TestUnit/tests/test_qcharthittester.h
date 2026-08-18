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
};
