// test_qchartcamera.h —— QChartCamera2D 单元测试声明
#pragma once
#include <QObject>

class TestQChartCamera2D : public QObject {
    Q_OBJECT
private slots:
    void cartesianToPixel_roundtrip();      // View→Pixel→View 恒等
    void pixelToCartesian_roundtrip();      // Pixel→View→Pixel 恒等
    void pan_preservesSize();               // pan 平移不改变尺寸
    void zoom_keepsCenterFixedPoint();      // zoom 以给定中心为不动点（归一化位置不变）
    void fitMode_Stretch_noChange();        // Stretch 不做拟合
    void fitMode_Fit_expands();             // Fit 扩张较小维度
    void fitMode_Crop_shrinks();            // Crop 收缩较大维度
    void fitMode_Fixed_aspectRatio();       // Fixed 匹配指定长宽比
    void centerZoom_propertyConsistency();  // center/zoom 属性与 viewRect 一致性
};
