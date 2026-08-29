// test_qchartcamera3d.h —— QChartCamera3D 单元测试声明（R5：viewCube 主状态派生不变量）
// 覆盖（design_3d.md §11.1 TestQChartCamera3D R5 8 例）：
//   派生 position/lookAt / 【D-3D-2 硬验收】正交俯视 ≡ 2D cartesianToPixel / orbit 几何
//   / dolly 缩放 / pan 平移 / 透视 vs 正交 / 零尺寸退化 / 五 Q_PROPERTY 可动画
#pragma once
#include <QObject>

class TestQChartCamera3D : public QObject {
    Q_OBJECT
private slots:
    void derivedPosition_lookAt();      // position = lookAt − forward·d、|position−lookAt|==d、lookAt==盒中心
    void orthographicTopDown_equals2D();// 【D-3D-2 硬验收】正交+viewCube=viewRect 范围+俯视 ≡ cartesianToPixel
    void orbit_geometry();              // orbit 后 |position−lookAt|==d、lookAt==盒中心、pitch clamp ±89°、viewCube 不动
    void dolly_scale();                 // 缩放 f 倍 → d'==f·d、lookAt 不变、同 world 点屏幕外扩
    void pan_translates();              // panViewCube → 盒中心/position 同位移、viewMatrix 平移项正确
    void perspectiveVsOrthographic();   // 同 world 点两模式屏幕坐标差异符合预期
    void degenerate_zeroSize();         // viewCube 零尺寸 → orbit/dolly no-op、无 NaN
    void properties_animatable();       // 五 Q_PROPERTY 存在、setter 发 viewChanged、插值器可用
};
