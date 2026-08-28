# QCartesianProjection Documentation

## Brief Introduction:
笛卡尔坐标投影（2D）：Numeric ↔ View Cartesian **恒等映射**——两种空间同构（design_notes §Projection 统一性）。`toCartesian/fromCartesian` 恒等；`computeDataBounds/computeViewRect` 恒等（dataBounds ≡ viewRect）；`defaultDataBounds` = QRectF(0,0,10,10)。header-only，无状态，无 Q_OBJECT。

## Constant Variables:
None.

## Member Variables:
None.（无状态——纯恒等映射）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QCartesianProjection` | 构造函数（轴名 "x"/"y"）。 | 无 | public | — | `QChartProjectionFactory::create(Cartesian)`/`QChartWidget`（默认投影） | `QChartProjectionFactory` |
| `CoordinateSystem` | `type` | 坐标系类型（内联）。 | 无 | public override | `CoordinateSystem::Cartesian` | 测试/Widget 同步 | — |
| `QPointF` | `toCartesian` | 恒等：Numeric (num0,num1) → View (num0,num1)。 | `qreal num0, qreal num1` | public override | `QPointF(num0,num1)` | `DrawContext`（QChartAxis.cpp:27 / QChartLayer.cpp:74）/轴路径 | — |
| `QPointF` | `fromCartesian` | 恒等：View (x,y) → Numeric (x,y)。 | `qreal x, qreal y` | public override | `QPointF(x,y)` | 交互反向/命中 | — |
| `QRectF` | `computeDataBounds` | 恒等：Numeric 范围 == View Cartesian 范围。 | `const QRectF& viewRect` | public override | `viewRect` | `QChartWidget`（fit/反算，QChartWidget.cpp:191/215/227/239） | `QChartWidget` |
| `QRectF` | `computeViewRect` | 恒等：反过来也恒等。 | `const QRectF& dataBounds` | public override | `dataBounds` | `QChartWidget`（setProjection/setDataRange 初始化，:62） | `QChartWidget` |
| `QRectF` | `defaultDataBounds` | 初始默认范围。 | 无 | public override | `QRectF(0,0,10,10)` | `QChartWidget::setProjection`（首次初始化，:61/292） | `QChartWidget` |

Notes:
- 恒等投影是全库默认（QChartWidget 构造/未设投影时）；Polar/Functional 的退化特例（Polar 在 θ 全范围 + 大半径时接近恒等，功能上仍独立）。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
