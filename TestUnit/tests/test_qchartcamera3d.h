// test_qchartcamera3d.h —— QChartCamera3D 单元测试声明
// 覆盖（design_3d.md §11.1 TestQChartCamera3D 全部 8 组）：
//   lookAt 正交性 / 【D-3D-2 硬验收】正交俯视 ≡ 2D cartesianToPixel / orbit 几何与 pitch clamp
//   / dolly / panTarget / 透视 vs 正交 / 退化 no-op / 三 Q_PROPERTY 可动画
#pragma once
#include <QObject>

class TestQChartCamera3D : public QObject {
    Q_OBJECT
private slots:
    void lookAt_orthonormal();              // viewMatrix 行列正交、平移项 = -R·eye
    void orthographicTopDown_equals2D();    // 【D-3D-2 硬验收】正交俯视 ≡ QChartCamera2D::cartesianToPixel
    void orbit_yawPitch_geometry();         // |position-lookAt| 不变、lookAt 不变、pitch clamp ±89°
    void dolly_factor();                    // 距离缩放、lookAt 不变、factor<=0 防除零
    void panTarget_translatesBoth();        // position/lookAt 同移、viewMatrix 平移项正确
    void perspectiveVsOrthographic();       // 同 world 点两模式屏幕坐标差异符合预期
    void degenerate_positionEqualsLookAt(); // orbit/dolly no-op、无 NaN
    void properties_animatable();           // 三 Q_PROPERTY 存在、setter 发 viewChanged、插值器可用
};
