// test_qchartmath.h —— QChartMath + Projection3D 家族单元测试声明
// 覆盖（design_3d.md §11.1 TestQChartMath）：
//   QChartMath：clipToNdc/ndcToScreen/clipToScreen、透视/正交矩阵性质、
//               viewDepth、projectBatch 对齐
//   Projection3D 家族：圆柱/球/笛卡尔/函数式映射、莫比乌斯采样、computeWorldBounds 兜底
#pragma once
#include <QObject>

class TestQChartMath : public QObject {
    Q_OBJECT
private slots:
    // ===== QChartMath（§11.1 1-7）=====
    void clipToNdc_divideByW();         // 齐次除法；w<=0 → NaN 哨兵
    void ndcToScreen_viewport();        // (±1,±1) 四角 → plotArea 四角（y 翻转）
    void clipToScreen_roundtrip();      // 正交矩阵下 world→clip→ndc→screen 与手算一致
    void perspectiveMatrix_properties();// near 平面 x/y 缩放 = 1/tan(fov/2)，far 比例正确
    void orthographicMatrix_properties();// 盒角点映射正确
    void viewDepth_viewSpaceZ();        // 前方点 depth>0、越远越大（= -viewZ，§3 公式）
    void projectBatch_alignment();      // 批量结果与逐点一致，两数组对齐

    // ===== Projection3D 家族（§11.1 8）=====
    void cylindrical_roundtrip();       // (r,θ,z) 往返；r=0 → θ NaN
    void spherical_roundtrip();         // (r,θ,φ) 往返；r=0 → θ/φ NaN
    void cartesian3d_identity();        // 恒等映射
    void functional3d_mobiusSamples();  // 采样点 |dist−R| ≤ 带宽容差
    void computeWorldBounds_sampling(); // 采样包围盒；全 NaN 兜底
};
