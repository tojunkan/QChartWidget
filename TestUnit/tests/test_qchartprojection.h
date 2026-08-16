// test_qchartprojection.h —— QChartProjection 单元测试声明
#pragma once
#include <QObject>

class TestQChartProjection : public QObject {
    Q_OBJECT
private slots:
    void cartesian_roundtrip();          // Cartesian toCartesian/fromCartesian 恒等往返
    void polar_roundtrip();              // Polar toCartesian/fromCartesian 往返
    void polar_pole_returnsNaN();        // Polar 极点 fromCartesian 返回 NaN 角度
    void cartesian_bounds_identity();    // Cartesian computeDataBounds/computeViewRect 恒等
    void polar_bounds_contain();         // Polar computeDataBounds(computeViewRect(db)) 包含原 db
    void createPath_breaksOnNaN();       // createPath 在 NaN 采样点断路径
};
