# QFunctionalProjection Documentation

## Brief Introduction:
用户自定义坐标投影（2D，免子类化）：通过 lambda 定义 Numeric ↔ View Cartesian 映射。最简用法只传 `forward` + `backward`；包络转换（`dataToView/viewToData`）不传则 fallback 采样（computeDataBounds 32×32 / computeViewRect 16×16）。`forward` 为 null 时 `toCartesian` 返回 NaN + qWarning；`backward` 为 null 时 `fromCartesian` 返回 NaN + qWarning（反向缺失场景）。header-only，无 Q_OBJECT。工厂入口：`QChartProjectionFactory::createFunctional`。

## Constant Variables:
None.（采样常数 grid=32/16 为函数内局部）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `std::function<QPointF(qreal,qreal)>` | `m_forward` | Numeric → View 映射（必传）。 | lambda / 空 | 构造传入 | — |
| `std::function<QPointF(qreal,qreal)>` | `m_backward` | View → Numeric 映射（可选；null → fromCartesian 返回 NaN）。 | lambda / `nullptr` | `nullptr` | — |
| `QRectF` | `m_defaultBounds` | 默认 Numeric 范围。 | `QRectF` | `QRectF(0,0,10,10)` | — |
| `std::function<QRectF(const QRectF&)>` | `m_dataToView` | dataBounds → viewRect（可选；null → 采样 fallback）。 | lambda / `nullptr` | `nullptr` | — |
| `std::function<QRectF(const QRectF&)>` | `m_viewToData` | viewRect → dataBounds（可选；null → 采样 fallback）。 | lambda / `nullptr` | `nullptr` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QFunctionalProjection` | 构造函数（forward 必传；backward/defaultBounds/dataToView/viewToData/name0/name1 可选默认）。 | `forward` <br> `backward=nullptr` <br> `defaultBounds=QRectF(0,0,10,10)` <br> `dataToView=nullptr` <br> `viewToData=nullptr` <br> `name0="x", name1="y"` | public | — | `QChartProjectionFactory::createFunctional`/demo（swirl 等） | `QChartProjectionFactory` |
| `CoordinateSystem` | `type` | 坐标系类型（内联）。 | 无 | public override | `CoordinateSystem::Functional` | 测试/Widget 同步 | — |
| `QPointF` | `toCartesian` | 委托 m_forward；forward 空 → NaN + qWarning。 | `qreal num0, qreal num1` | public override | `QPointF` | `DrawContext`（QChartAxis.cpp:27/QChartLayer.cpp:74）/createPath | — |
| `QPointF` | `fromCartesian` | 委托 m_backward；空 → NaN + qWarning。 | `qreal x, qreal y` | public override | `QPointF` | 交互反向/computeDataBounds 采样 | — |
| `QRectF` | `computeDataBounds` | `m_viewToData` 优先；否则 32×32 网格采样 fromCartesian 聚合（全 NaN → 回退恒等 viewRect）。 | `const QRectF& viewRect` | public override | `QRectF` | `QChartWidget`（fit/反算） | `QChartWidget` |
| `QRectF` | `computeViewRect` | `m_dataToView` 优先；否则 dataBounds 边界 16×16 采样 toCartesian 估算包围盒（全 NaN → 回退恒等 dataBounds）。 | `const QRectF& dataBounds` | public override | `QRectF` | `QChartWidget`（setProjection/setDataRange） | `QChartWidget` |
| `QRectF` | `defaultDataBounds` | 返回 m_defaultBounds。 | 无 | public override | `m_defaultBounds` | `QChartWidget::setProjection` | `QChartWidget` |

Notes:
- 应用：鱼眼/扭曲 Cartesian 等"显式给映射函数"场景；`ProjectionToolKit`（utils）基于 createFunctional 提供即用投影（恒等/Power2/Exp/Log）。
- NaN 自然传播（forward 结果 NaN/Inf 由调用方跳过）；全 NaN 采样 → 回退恒等（避免垃圾包围盒）。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
