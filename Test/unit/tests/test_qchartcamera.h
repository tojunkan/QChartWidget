// test_qchartcamera.h —— QChartCamera2D 单元测试声明
#pragma once
#include <QObject>

class TestQChartCamera2D : public QObject {
    Q_OBJECT
private slots:
    // ===== 映射往返 =====
    void cartesianToPixel_roundtrip();      // View→Pixel→View 恒等
    void pixelToCartesian_roundtrip();      // Pixel→View→Pixel 恒等

    // ===== 交互几何 =====
    void pan_preservesSize();               // pan 平移不改变尺寸
    void zoom_keepsCenterFixedPoint();      // zoom 以给定中心为不动点

    // ===== Fit / Crop（等比适配）=====
    void fitMode_Stretch_noChange();        // Stretch 不做任何调整
    void fitMode_Fit_expands();             // Fit：留白（缩放系数取 min）
    void fitMode_Crop_shrinks();            // Crop：裁剪（缩放系数取 max）

    // ===== scaleFactor（整体缩放）=====
    void scaleFactor_appliedAfterFit();     // scaleFactor 在 Fit/Crop 之后应用
    void scaleFactor_centerInvariant();     // scaleFactor 不改变 viewRect 中心点
    void scaleFactor_stretchIgnored();      // Stretch 模式下 scaleFactor 被忽略（或仍应用？我们设计为仍应用，但测试要验证）

    // ===== 属性一致性 =====
    void centerZoom_propertyConsistency();  // center/zoom 属性与 viewRect 一致性
};