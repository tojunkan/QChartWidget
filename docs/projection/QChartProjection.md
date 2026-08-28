# QChartProjection Documentation

## Brief Introduction:
2D 坐标投影基类（Phase 0 起）：Numeric 空间 ↔ View Cartesian 空间双向映射 + 视窗包络计算（dataBounds↔viewRect）。五空间链路：`Numeric ─[toCartesian]→ View Cartesian ─[cartesianToPixel]→ Pixel`。关键语义（design_notes §Projection 统一性）：**映射是纯几何、无需 dataBounds**（toCartesian/fromCartesian 不吃范围）；dataBounds 只用于包络互转（computeDataBounds/computeViewRect）。子类：QCartesianProjection/QPolarProjection/QFunctionalProjection（+ 合成 QInterpolatedProjection）。header-only 基类（纯虚接口 + createPath 默认实现），无 Q_OBJECT。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QString` | `m_name0` | dim0 轴名（"x"/"θ"/"u" 等；dimensionName 返回）。 | `QString` | 构造传入（默认 "x"） | — |
| `QString` | `m_name1` | dim1 轴名。 | `QString` | 构造传入（默认 "y"） | — |

Notes:
- 本头还定义类型级枚举 `CoordinateSystem{Cartesian, Polar, Functional}`（文件级，非类内——D11 命名规范前遗留；QChartWidget/QChartLayer 使用）。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartProjection` | 构造函数（轴名默认 "x"/"y"）。 | `QString name0="x"` <br> `QString name1="y"` | public | — | 子类构造 | — |
| `QString` | `dimensionName` | 维度名访问器（越界返回空）。 | `int dim` | public | `QString`/空 | 轴标题/测试 | — |
| `CoordinateSystem` | `type` | **纯虚**：坐标系类型。 | 无 | public pure virtual | 子类实现 | 测试/Widget 同步坐标系（setProjection → layer setCoordinateSystem） | `QChartLayer` |
| `QPointF` | `toCartesian` | **纯虚**：Numeric → View Cartesian（纯几何；NaN/Inf 自然传播）。 | `qreal num0, qreal num1` | public pure virtual | 子类实现 | `DrawContext`（QChartAxis.cpp:27/QChartLayer.cpp:74）/createPath | — |
| `QPointF` | `fromCartesian` | **纯虚**：View Cartesian → Numeric（奇点 NaN 策略由子类定义）。 | `qreal x, qreal y` | public pure virtual | 子类实现 | 交互反向/包络采样 | — |
| `QRectF` | `computeDataBounds` | **纯虚**：viewRect → dataBounds（Pan/Zoom 后可见数据范围，刻度生成用）。 | `const QRectF& viewRect` | public pure virtual | 子类实现 | `QChartWidget`（fit/反算，QChartWidget.cpp:191/215/227/239） | `QChartWidget` |
| `QRectF` | `computeViewRect` | **纯虚**：dataBounds → viewRect（setRange 语法糖后）。 | `const QRectF& dataBounds` | public pure virtual | 子类实现 | `QChartWidget`（setProjection/setDataRange，:62） | `QChartWidget` |
| `QRectF` | `defaultDataBounds` | 默认 Numeric 范围（Widget 首次构造确定初始 viewRect）。 | 无 | public virtual | `QRectF(0,0,10,10)` | `QChartWidget::setProjection`（首次初始化，:61/292） | `QChartWidget` |
| `QPainterPath` | `createPath` | 采样 `dataCurve(t)→(num0,num1)` 经 toCartesian 连接为 QPainterPath；**NaN/Inf 断开**（moveTo 重开子路径，处理极坐标奇点）。 | `std::function<QPointF(qreal t)> dataCurve` <br> `int segments=64` | public | `QPainterPath` | `DrawContext::toPath`（QChartAxis.h:50）/`QChartAxis::drawAtEdge`（QChartAxis.cpp:400，segments=72） | `DrawContext` <br> `QChartAxis` |

Notes:
- 本头保留 Phase 0/1 遗留：注释掉的 `//Q_LOGGING_CATEGORY(logProjection, ...)` 死行、`<summary>` XML 注释风格、制表符缩进（见 docs/audit/semantic_vs_implementation_audit.md D1/D2）。
- createPath 全流程（NaN 断路径语义）：docs/projection/QChartProjection_createPath_flow.md。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
