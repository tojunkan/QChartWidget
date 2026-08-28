# QPolarProjection Documentation

## Brief Introduction:
极坐标投影（2D）：dim0 = 角度（度，θ）、dim1 = 半径（物理单位，r）。`toCartesian(θ°,r)` = `(r·cosθ, r·sinθ)`；`fromCartesian(x,y)`：`θ = atan2(y,x) ∈ [0°,360°)`、`r = √(x²+y²)`，**极点 r=0 处 θ 无定义 → 返回 (NaN, 0)**（奇点 NaN 策略）。`computeDataBounds`：32×32 网格采样 + **跨 0° 边界修复**（完整圆盘语义，详见 deepdive 级 flow）；`computeViewRect`：扇形 Cartesian 轴对齐包围盒（内外弧 32 段采样）。`defaultDataBounds` = 完整圆盘 QRectF(0,0,360,10)。header-only，无状态。

## Constant Variables:
None.（采样常数 `grid=32`/`arcSamples=32` 为函数内局部）

## Member Variables:
None.（无状态）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QPolarProjection` | 构造函数（轴名 "θ"/"r"）。 | 无 | public | — | `QChartProjectionFactory::create(Polar)`/demo | `QChartProjectionFactory` |
| `CoordinateSystem` | `type` | 坐标系类型（内联）。 | 无 | public override | `CoordinateSystem::Polar` | 测试/Widget 同步 | — |
| `QPointF` | `toCartesian` | θ°→弧度 → `(r·cosθ, r·sinθ)`。 | `qreal num0`（θ°，度） <br> `qreal num1`（r） | public override | `QPointF` | `DrawContext`（QChartAxis.cpp:27/QChartLayer.cpp:74）/createPath | — |
| `QPointF` | `fromCartesian` | `r=√(x²+y²)`；`θ=atan2∈[0°,360°)`；**极点 r≈0 → (NaN, 0)**。 | `qreal x, qreal y` | public override | `QPointF(deg, r)` <br> `(NaN, 0)`（极点） | 交互反向/computeDataBounds 采样 | — |
| `QRectF` | `computeDataBounds` | **32×32 网格采样** viewRect 四边/内部 → θ min/max + r min/max；原点在视口内 → rMin=0；**跨 0° 边界（θSpan>180° 或矩形覆盖正 X 轴）→ 返回完整圆盘 [0°,360°)×[rMin,rMax]**（核心修复）；全 NaN 兜底 rMin=0/rMax=1。 | `const QRectF& viewRect` | public override | `QRectF(θMin,rMin,θSpan,rSpan)` <br> 或完整圆盘 `QRectF(0,rMin,nextafter(360,−∞),rMax−rMin)` | `QChartWidget`（fit/反算，QChartWidget.cpp:191 等） | `QChartWidget` |
| `QRectF` | `computeViewRect` | 扇形 Cartesian 轴对齐包围盒：内弧 + 外弧各 32 段采样（径向边含于弧端点）。 | `const QRectF& dataBounds` | public override | `QRectF(xMin,yMin,dx,dy)` | `QChartWidget`（setProjection/setDataRange，:62） | `QChartWidget` |
| `QRectF` | `defaultDataBounds` | 完整圆盘。 | 无 | public override | `QRectF(0,0,360,10)` | `QChartWidget::setProjection`（首次初始化） | `QChartWidget` |

Notes:
- **跨 0° 边界是 computeDataBounds 的复杂点**（32×32 采样 + 双判据修复 + nextafter 防闭区间），完整推导见 docs/projection/QPolarProjection_computeDataBounds_flow.md。
- 奇点语义与 3D 柱坐标一致（r=0 → θ NaN；Cylindrical3D fromWorld 同款，z 保留）。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
