# QChartCamera Documentation

## Brief Introduction:
QChartCamera 是 **2D 相机**（继承 QChartAbstractCamera；重构后 2D 相机唯一实现，旧 `QChartCamera2D` 职责并入本类），掌管 **View Cartesian ↔ Pixel 线性映射**：`project` 沿 viewRect → plotArea 线性映射（Y 翻转），矩阵接口退化——`viewMatrix` 平移+缩放输出标准 NDC（Y 取反），`projectionMatrix` 为单位阵。提供 viewRect/center/zoom 三种视图状态（Q_PROPERTY，均 NOTIFY viewChanged）、pan/zoom 交互操作、`fitToPlotArea`（fit 模式 × fit 策略 × scale 附加缩放）。五空间链中相机不参与 Data/Numeric/Cartesian 映射，只做 Cartesian→Pixel 末两环（含 ViewNorm 概念上的 NDC 化）。

## Constant Variables:
None.（同头文件定义两个枚举，非类成员：`ViewRectFitMode{Stretch, Expand, Crop, Preserve}` 与 `FitStrategy{KeepCenter, KeepTopLeft, KeepTopRight, KeepBottomLeft, KeepBottomRight, KeepTop, KeepBottom, KeepLeft, KeepRight}`）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QRectF` | `m_viewRect` | （private）视图窗口（View Cartesian 范围；zoom()=宽度） | `QRectF` | `QRectF()`（空） | — |
| `ViewRectFitMode` | `m_fitMode` | （private）fit 模式：Preserve=保面积；Expand=外扩填满；Stretch=跳过（返回 false）；Crop=裁剪填满 | Stretch/Expand/Crop/Preserve | `ViewRectFitMode::Preserve` | — |
| `FitStrategy` | `m_fitStrategy` | （private）fit 后锚点策略（哪条边/中心不动） | 9 值 | `FitStrategy::KeepCenter` | — |
| `qreal` | `m_scale` | （private）fit 时附加缩放系数（>0；Preserve 模式下以面积守恒覆盖，不乘 scale） | `qreal(>0)` | `1.0` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartCamera` | 构造：转交父类 | `QObject* parent=nullptr` | public | — | 用户/测试夹具（值成员或栈对象） | `QChartAbstractCamera` |
| `QRectF` | `viewRect` | 视窗访问器（内联） | 无 | public | `QRectF` | `QChartScene::camera` 消费者、`isPrimitiveVisible2D`（QPainterChartRenderer）、测试 | — |
| `void` | `setViewRect` | 设置视窗；同值忽略，否则更新 + emit viewChanged | `const QRectF& r` | public | — | TestAxisPipeline/TestAxisMatrix 夹具、Widget 驱动链 | — |
| `QPointF` | `center` | 视窗中心访问器（内联 = viewRect.center） | 无 | public | `QPointF` | 测试 | — |
| `void` | `setCenter` | 平移视窗使中心到 c（moveCenter）；同值忽略，变化 emit viewChanged | `const QPointF& c` | public | — | Widget 交互/动画阶段 | — |
| `qreal` | `zoom` | 缩放访问器（内联 = viewRect.width） | 无 | public | `qreal` | 测试 | — |
| `void` | `setZoom` | 设置宽度：`z≤0` qWarning 忽略；同宽忽略；否则按中心缩放（factor=z/旧宽） | `qreal z` | public | — | 用户/测试 | — |
| `void` | `panViewCartesian` | 平移视窗（dx/dy，View Cartesian 单位）；无同值判定，直接 emit viewChanged | `qreal dx, qreal dy` | public | — | Widget mouseMove 拖拽阶段 | — |
| `void` | `zoomViewCartesian` | 以 (cx,cy) 为锚缩放（factorX/factorY 独立；factor≤0 忽略）→ 更新视窗 + emit viewChanged | `qreal cx, qreal cy, qreal factorX, qreal factorY` | public | — | `setZoom` 内部、Widget wheel 阶段 | — |
| `bool` | `fitToPlotArea` | 覆写：Stretch 或 plotArea≤0 → false；按 aspect 差（view 更宽/更高）+ fitMode 调宽高；Preserve 保面积，否则乘 m_scale；按 m_fitStrategy 锚定；无实际变化返回 false，变化则更新 + emit viewChanged + true | `const QRectF& plotArea` | public | `true`/`false` | Widget 布局阶段（QChartAbstractWidget 未入 S0） | — |
| `void` | `setFitMode` | 设置 fit 模式（内联，无信号） | `ViewRectFitMode mode` | public | — | Widget/用户 | — |
| `ViewRectFitMode` | `fitMode` | fit 模式访问器（内联） | 无 | public | `ViewRectFitMode` | 测试 | — |
| `void` | `setFitStrategy` | 设置 fit 锚点策略（内联，无信号） | `FitStrategy strategy` | public | — | Widget/用户 | — |
| `FitStrategy` | `fitStrategy` | 策略访问器（内联） | 无 | public | `FitStrategy` | 测试 | — |
| `void` | `setScale` | 设置附加缩放（内联） | `qreal ratio` | public | — | Widget/用户 | — |
| `qreal` | `scale` | 缩放访问器（内联） | 无 | public | `qreal` | 测试 | — |
| `QMatrix4x4` | `viewMatrix` | 覆写：平移使 viewRect 中心到原点 + 缩放 `(2/w, -2/h, 1)`（Y 取反 → NDC） | 无 | public | `QMatrix4x4` | GL `viewProjectionMatrix` 链 | — |
| `QMatrix4x4` | `projectionMatrix` | 覆写：单位阵（2D 视图矩阵已输出 NDC；aspect 忽略） | `qreal aspect` | public | `QMatrix4x4` | GL `viewProjectionMatrix` 链 | — |
| `QChartProjectedPoint` | `project` | 覆写：viewRect → plotArea 线性映射（x 同向、y 翻转 `bottom - ny*h`）；depth=0；cart 原样回传 | `const QVector3D& cart, const QRectF& plotArea` | public | `QChartProjectedPoint` | `QPainterChartRenderer::drawPrimitives2D/drawLabels2D`、GL `drawLabels` | — |
| `Ray` | `unproject` | 覆写：像素 → 世界（Y 翻转逆映射，z=0 平面）；direction=+z 单位向量 | `const QPointF& pixel, const QRectF& plotArea` | public | `Ray` | S0 无（拾取阶段恢复） | — |

Notes:
- Q_PROPERTY：viewRect/center/zoom（NOTIFY 均 viewChanged）。
- fit 语义补充：同 aspect 时直接进入 Preserve/scale 分支；Preserve 模式下 m_scale 不生效（面积守恒优先）。
- S0 实测用法：测试以栈/值成员 `QChartCamera camera` + `camera.setViewRect([-10,10]²)`，随后 `scene.camera=&camera` 交渲染器；CPU 路径经 `dynamic_cast<const QChartCamera*>` 走 2D 分支。

## Overrided Qt Events:
无（QObject 非 QWidget；本类无 Qt 事件覆写）。

## Signals:
None.（本类无新信号；继承并发射 `viewChanged`——实测触发点：setViewRect/setCenter/setZoom（经 zoomViewCartesian）/panViewCartesian/zoomViewCartesian/fitToPlotArea）
