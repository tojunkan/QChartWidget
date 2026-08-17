// test_qchartsurface3d.h —— 3D 系列单元测试声明（QDataPoint3D + 三子类）
// 覆盖（design_3d.md §11.1 TestQChartSurface3D 全部 7 组）：
//   QVariant 三元组 / grid 行主序与校验 / parametricGrid 格点 / worldCache 尺寸与值
//   / scatter markerSize / line NaN 断段 / collect 投影 NaN 跳过
#pragma once
#include <QObject>

class TestQChartSurface3D : public QObject {
    Q_OBJECT
private slots:
    void data_variantTriple();          // QDataPoint3D append QVariant（qreal/QDateTime）→ count/at/replace
    void grid_layout();                 // setGrid 行主序存取、大小校验
    void parametricGrid_lattice();      // (u0,u1)×(v0,v1) 采样值正确
    void worldCache_filled();           // worldCache 尺寸 = rows*cols、值 = toWorld(u,v)
    void scatter_markerSize();          // 属性存在/生效
    void line_breakOnNaN();             // 断段行为
    void collect_nanSkip();             // 投影 NaN 图元被跳过
};
