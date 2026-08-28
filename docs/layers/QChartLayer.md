# QChartLayer Documentation

## Brief Introduction:
图层基类（2D，渲染层第三层）：持有 axisX/axisY 与 Series 列表，**组装坐标转换链** `Data→toNumeric→toCartesian→cartesianToPixel`（`makeToPixel` 闭包，Series 零依赖 Axis 类型），负责 `drawGrid`（数据主脊网格线）与 `drawAllSeries`（遍历系列注入 toPixel）。命中委托 `QChartHitTester`（D27，HitResult 定义提升至 QChartHitTester，本类保留别名调用方零改动）。坐标系类型由 Widget 同步（setProjection/addLayer）。Grid 样式 D12 双槽（gridVisible/gridColor）。3D 子类 QChartLayer3D 复用所有权/图例/主题。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartAxis*` | `m_axisX` | X 轴（非持有；setAxisX 绑定）。 | `QChartAxis*`/`nullptr` | `nullptr` | `QChartAxis` |
| `QChartAxis*` | `m_axisY` | Y 轴（非持有）。 | `QChartAxis*`/`nullptr` | `nullptr` | `QChartAxis` |
| `QList<QChartSeries*>` | `m_series` | 系列列表（**所有权**：析构 qDeleteAll）。 | `QList<QChartSeries*>` | 空 | `QChartSeries` |
| `CoordinateSystem` | `m_coordSys` | 坐标系类型（Widget 同步）。 | `Cartesian`/`Polar`/`Functional` | `CoordinateSystem::Cartesian` | `QChartProjection` |
| `bool` | `m_gridVisible` | 网格可见（Q_PROPERTY gridVisible）。 | `true`/`false` | `true` | — |
| `std::optional<QColor>` | `m_gridColorOverride` | 网格色显式覆盖（D12）。 | `std::optional<QColor>`/`nullopt` | `nullopt` | `QChartTheme` |
| `QColor` | `m_themeGridColor` | 主题默认网格色。 | `QColor` | `QColor(220,220,220)` | `QChartTheme` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartLayer` | 构造函数。 | `QObject* parent=nullptr` | public | — | QChartWidget::addLayer/用户 | — |
| — | `~QChartLayer` | 析构（qDeleteAll m_series——系列所有权在层）。 | — | public | — | — | `QChartSeries` |
| `CoordinateSystem` | `coordinateSystem` | 坐标系访问器（内联）。 | 无 | public | 三值 | 测试 | — |
| `void` | `setCoordinateSystem` | 坐标系设置（内联；Widget 同步）。 | `CoordinateSystem cs` | public | — | `QChartWidget::setProjection` | `QChartWidget` |
| `QChartAxis*` | `axisX`/`axisY` | 轴访问器（内联）。 | 无 | public | `QChartAxis*` | 绘制/测试 | `QChartAxis` |
| `void` | `setAxisX`/`setAxisY` | 轴绑定（非持有）。 | `QChartAxis* a` | public | — | 用户/Widget | `QChartAxis` |
| `bool` | `validateAxes` | 轴有效性校验（virtual）。 | 无 | public virtual | `true`/`false` | `drawAllSeries`（无效则告警中止） | — |
| `void` | `addSeries` | 添加系列（所有权）+ emit seriesAdded。 | `QChartSeries* s` | public | — | `QChartWidget::addLayer` 接线后用户/demo | `QChartSeries` |
| `void` | `removeSeries` | 移除系列 + emit seriesRemoved。 | `QChartSeries* s` | public | — | 用户 | `QChartSeries` |
| `QList<QChartSeries*>` | `seriesList` | 系列列表访问器（内联）。 | 无 | public | `QList<QChartSeries*>` | Widget/测试/渲染 | `QChartSeries` |
| `void` | `clearSeries` | 清空系列。 | 无 | public | — | 用户 | `QChartSeries` |
| `void` | `drawAllSeries` | 遍历系列：makeToPixel 注入 → 每系列 save/opacity/draw/restore（无效轴告警中止）。 | `QPainter* p` <br> `const DrawContext& ctx` | public | — | `QPainterChartRenderer::drawForeground` | `DrawContext` |
| `void` | `drawGrid` | 网格：axisX/Y 的 tickValues 作 offset → 轴 drawAtPosition（只画轴线，无标签刻度）。 | `QPainter* p` <br> `const DrawContext& ctx` | public | — | `QPainterChartRenderer::drawBackground` | `QChartAxis` <br> `DrawContext` |
| `HitResult` | `hitTest` | 命中：makeToPixel 注入 → 委托 `QChartHitTester::hitTest`（2D 顶层优先）。 | `const QPointF& pixel` <br> `const DrawContext& ctx` | public | `HitResult` | `QChartWidget::mouseMoveEvent`（悬停） | `QChartHitTester` |
| `bool` | `isGridVisible`/`setGridVisible` | 网格可见 + emit gridChanged。 | `bool v` | public | — | 用户 | — |
| `QColor` | `gridColor` | 有效网格色（override 或主题）。 | 无 | public | `QColor` | 绘制 | `QChartTheme` |
| `void` | `setGridColor`/`setThemeGridColor`/`clearGridColor` | D12 双槽（override/主题/回退）+ 条件 emit gridChanged。 | `const QColor& c` | public | — | 用户/`QChartWidget::pushTheme` | `QChartTheme` |
| `std::optional<QColor>` | `gridColorOverride` | 覆盖值访问器（内联）。 | 无 | public | `std::optional<QColor>` | 主题判断 | — |
| `std::function<QPointF(QVariant,QVariant)>` | `makeToPixel` | **核心组装（protected）**：注入 toNumeric0/1 到 ctx + 返回 Data→Pixel 四跳闭包（toNumeric→NaN 检查→toCartesian→cartesianToPixel）。 | `DrawContext& ctx` | protected | lambda | `drawAllSeries`/`hitTest` | `DrawContext` <br> `QChartCamera2D` |

Notes:
- **makeToPixel 四跳链**（全库核心组装）：`Data →[axisX/Y::toNumeric]→ Numeric →[NaN 检查]→ [projection::toCartesian]→ View Cartesian →[QChartCamera2D::cartesianToPixel]→ Pixel`——Series 不知道 Axis 类型（零耦合落点），完整流程见 docs/layers/QChartLayer_makeToPixel_flow.md。
- 系列所有权：m_series 拥有（qDeleteAll）——Series 单归属硬约束（D18）。

## Overrided Qt Events:
None.（QObject）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `seriesAdded` | 系列添加。 | `QChartSeries*` | `QChartWidget::addLayer` 连线（→ 系列属性信号接线 + 图例重建 + invalidateForeground，src/core/QChartWidget.cpp:80-103） | `QChartWidget` |
| `seriesRemoved` | 系列移除。 | `QChartSeries*` | `QChartWidget` 连线（→ 出列 + invalidateForeground，:103-109） | `QChartWidget` |
| `gridChanged` | 网格样式/可见变化。 | — | `QChartWidget` 连线（→ invalidateBackground） | `QChartWidget` |
