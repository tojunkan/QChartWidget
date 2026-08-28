# QScatterSeries Documentation

## Brief Introduction:
散点系列（2D，QXYSeries 派生）：点标记绘制（`MarkerShape{Circle,Square,Triangle,Diamond,Plus,Cross}` 六形）。Q_PROPERTY×2（markerSize/fillColor）；`setPen` 自定义画笔。`hitTest` 像素 in 标记。信号：markerSizeChanged/fillColorChanged。

## Constant Variables:
None.（`MarkerShape` 为类型级枚举）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `int` | `m_markerSize` | 标记尺寸（px；Q_PROPERTY markerSize）。 | `int` | 构造设定（如 6） | — |
| `MarkerShape` | `m_markerShape` | 标记形状（setMarkerShape）。 | 六形枚举 | `MarkerShape::Circle` | — |
| `QPen` | `m_pen` | 标记画笔（setPen）。 | `QPen` | 构造默认 | — |
| `QColor` | `m_fillColor` | 填充色（Q_PROPERTY fillColor）。 | `QColor` | 构造默认 | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QScatterSeries` | 构造函数。 | `name`, `parent` | public | — | 用户/demo | — |
| `void` | `draw` | 逐点绘制标记（drawMarker 按形状；pen/fill 生效）。 | `QPainter*` <br> `toPixel` <br> `const DrawContext* ctx=nullptr` | public override | — | `QChartLayer::drawAllSeries` | `DrawContext` |
| `int` | `hitTest` | 像素 in 标记（markerSize 半径/形状判定）。 | `pixel, toPixel, ctx` | public override | `int` | `QChartHitTester` | `QChartHitTester` |
| `void` | `setMarkerShape` | 标记形状（无 NOTIFY）。 | `MarkerShape s` | public | — | 用户 | — |
| `void` | `setMarkerSize` | 标记尺寸 + emit markerSizeChanged。 | `int size` | public | — | 用户 | — |
| `void` | `setPen` | 画笔（无 NOTIFY）。 | `const QPen& p` | public | — | 用户 | — |
| `void` | `setFillColor` | 填充色 + emit fillColorChanged。 | `const QColor& c` | public | — | 用户 | — |
| `void` | `drawMarker` | 私有：按形状画标记（QPainterPath 构造 Circle/Square/Triangle/Diamond/Plus/Cross）。 | `QPainter* p` <br> `const QPointF& pos` | private | — | `draw` | — |

Notes:
- 3D 散点是独立类（QChartScatterSeries3D，Point 图元收集）——2D/3D 不共享实现。
- markerSizeChanged/fillColorChanged 经 QChartWidget 系列连线 → invalidateForeground。

## Overrided Qt Events:
None.

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `markerSizeChanged` | 标记尺寸变化。 | — | `QChartWidget`（seriesAdded 连线 → invalidateForeground） | `QChartWidget` |
| `fillColorChanged` | 填充色变化。 | — | `QChartWidget`（同上） | `QChartWidget` |
