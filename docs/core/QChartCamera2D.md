# QChartCamera2D Documentation

## Brief Introduction:
2D 相机 = 原 QChartCamera 整体搬入（行为零变化，D1/design_3d §4.4 改名映射表）：**viewRect 几何的唯一实现**。拥有 viewRect + fit 策略（Stretch/Fit/Crop/Fixed）+ `center/zoom` 属性（QPropertyAnimation 直接驱动）+ View Cartesian ↔ Pixel 线性映射（静态纯函数，DrawContext/Widget/Layer 复用）。分工红线：不知道 Projection、不反算 dataBounds（归 QChartWidget）、不拥有 plotArea（映射/拟合均以参数传入）。2D 是 3D 相机的退化特例（正交俯视 + 恒等映射 ≡ 本类映射）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QRectF` | `m_viewRect` | 视窗（View Cartesian 空间，物理长度单位）：映射/拟合/center/zoom 的主状态。 | `QRectF` | `QRectF()` | `QChartProjection`（dataBounds 互转） |
| `ViewRectFitMode` | `m_fitMode` | fit 模式（`setFitMode` 修改）。 | `Stretch` <br> `Fit` <br> `Crop` <br> `Fixed` | `ViewRectFitMode::Fit` | — |
| `qreal` | `m_fixedAspectRatio` | Fixed 模式目标长宽比。 | `qreal` | `1.0` | — |

Notes:
- `ViewRectFitMode` 与 `FitStrategy`（KeepWidth/KeepHeight/KeepCenter）均为本头文件级枚举（非类内）。
- 无 Q_PROPERTY 之外的 Qt 元数据；`Q_PROPERTY(viewRect/center/zoom)` 三个属性 NOTIFY viewChanged。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartCamera2D` | 构造函数（parent 透传基类）。 | `QObject* parent` | public | — | QChartWidget 构造 | `QChartCamera` |
| `QRectF` | `viewRect` | 视窗访问器（内联）。 | 无 | public | — | QChartWidget/渲染/测试 | — |
| `void` | `setViewRect` | 绝对设置 viewRect（设置什么就是什么，不做 fit 修正）+ emit viewChanged（值变化时）。 | `const QRectF& r` | public | — | `QChartWidget::setViewRect`/`setDataRangeDim0/1`/`setProjection`/`QViewRectAnimation` | `QChartWidget` |
| `QPointF` | `center` | viewRect 中心访问器（内联 = viewRect.center()）。 | 无 | public | — | QPropertyAnimation/测试 | — |
| `void` | `setCenter` | 平移 viewRect 使中心变为 c（zoom=宽度不变）+ emit viewChanged。 | `const QPointF& c` | public | — | QPropertyAnimation | — |
| `qreal` | `zoom` | 缩放值访问器（内联 = viewRect.width()，以宽度度量）。 | 无 | public | — | QPropertyAnimation/测试 | — |
| `void` | `setZoom` | 以当前 center 为中心把宽度设为 z（高度按长宽比同步）+ emit viewChanged。 | `qreal z` | public | — | QPropertyAnimation | — |
| `void` | `panViewCartesian` | 平移 viewRect（dx/dy 在 View Cartesian 空间）+ emit viewChanged。 | `qreal dx, qreal dy` | public | — | `QChartWidget::panViewCartesian` | `QChartWidget` |
| `void` | `zoomViewCartesian` | 以 (cx,cy) 为中心两维独立缩放（factor<1=放大，>1=缩小）+ emit viewChanged。 | `qreal cx, qreal cy` <br> `qreal factorX, qreal factorY` | public | — | `QChartWidget::zoomViewCartesian`/`setZoom` | `QChartWidget` |
| `ViewRectFitMode` | `fitMode` | fit 模式访问器（内联）。 | 无 | public | — | `QChartWidget::viewRectFitMode` | `QChartWidget` |
| `void` | `setFitMode` | 设置 fit 模式（内联）。 | `ViewRectFitMode mode` | public | — | `QChartWidget` | `QChartWidget` |
| `qreal` | `fixedAspectRatio` | Fixed 长宽比访问器（内联）。 | 无 | public | — | 测试 | — |
| `void` | `setFixedAspectRatio` | 设置 Fixed 长宽比（内联）。 | `qreal ratio` | public | — | 用户 | — |
| `bool` | `fitViewRectToPlotArea` | fit 几何：调整 viewRect 使长宽比匹配 plotArea（Fit 扩张/Crop 收缩/Fixed 强制，× KeepWidth/KeepHeight/KeepCenter）；**返回 true 表示 viewRect 实际被修改**（调用方据此决定是否重算 dataBounds）。 | `const QRectF& plotArea` <br> `FitStrategy strategy` | public | `true`/`false` | `QChartWidget::fitViewRectToPlotArea` | `QChartWidget` |
| `QPointF` | `cartesianToPixel` | **静态纯函数**：View Cartesian → Pixel 线性映射（ViewNorm 归一 + y 翻转，全推导见 deepdive_viewRect）。 | `const QRectF& viewRect` <br> `const QRectF& plotArea` <br> `qreal cx, qreal cy` | public static | — | `DrawContext`（QChartAxis.h:42/53）/`QChartWidget::cartesianToPixel`/Layer | `DrawContext` |
| `QPointF` | `pixelToCartesian` | **静态纯函数**：Pixel → View Cartesian 逆线性映射。 | `const QRectF& viewRect` <br> `const QRectF& plotArea` <br> `const QPointF& pixel` | public static | — | `QChartWidget::pixelToCartesian`/交互 | `DrawContext` |
| `QPointF` | `cartesianToPixel` | 实例版（使用 m_viewRect；plotArea 由调用方传入）。 | `const QRectF& plotArea` <br> `qreal cx, qreal cy` | public | — | Layer/渲染路径 | — |
| `QPointF` | `pixelToCartesian` | 实例版（使用 m_viewRect）。 | `const QRectF& plotArea` <br> `const QPointF& pixel` | public | — | Layer/交互 | — |

Notes:
- **映射为纯函数**：静态版与实例版公式一致（`px = pl + pw·(cx−vl)/vw`；`py = (pt+ph) − ph·(cy−vt)/vh`），y 翻转与 3D `QChartMath::clipToScreen` 一致（正交俯视 ≡ 2D 硬验收）。
- `fitViewRectToPlotArea` 的返回值语义是 dataBounds 反算的开关（避免 Polar 往返漂移）——调用方（QChartWidget）仅在 true 时重算。

## Overrided Qt Events:
None.（非 QWidget）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `viewChanged` | 视图状态变化（viewRect/center/zoom/fit 几何实际变化时 emit）。 | — | NULL（内部未连线；QPropertyAnimation 驱动属性 + 外部按需监听） | `QChartWidget` <br> `QViewRectAnimation` |

Notes:
- 发射点：setViewRect/setCenter/setZoom/panViewCartesian/zoomViewCartesian（值实际变化才发）。
- 与 QChartWidget 的 viewChanged 是**两个独立信号**：Widget 的 viewChanged 由 Widget 自己的 pan/zoom/setViewRect 方法内联 emit（src/core/QChartWidget.cpp:220/233/247/272/286）；本信号服务于属性动画与外部监听。
