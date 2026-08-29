// test_qchartgl.h —— QChartGL 资源池 + QOpenGLChartRenderer 骨架单元测试声明
// 覆盖（design_phase3.md §2.1/§7.3，t42）：
//   renderer_interface_contract：QChartRenderer 接口契约（全平台，无 GL 依赖）
//   sharedContext_refcount：共享根/实例计数生命周期（全平台；offscreen 下创建失败仅降级）
//   gl_probe_context：GL 环境探测（xcb 平台：共享根 + QOpenGLWidget 上下文可建 + 基线记录；
//                     offscreen 平台 QSKIP——§13.2 运行环境）
#pragma once
#include <QObject>

class TestQChartGL : public QObject {
    Q_OBJECT
private slots:
    void renderer_interface_contract();   // 接口契约（device 校验/导出拒绝/脏标记/no-op 缓存）
    void sharedContext_refcount();        // 实例计数与共享根生命周期（成对注册/注销安全）
    void gl_probe_context();              // GL 探测：共享根 + QOpenGLWidget 上下文（offscreen QSKIP）
};
