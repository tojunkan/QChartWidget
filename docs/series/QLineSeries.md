# QLineSeries Documentation

## Brief Introduction:
折线系列（2D，QXYSeries 派生）：折线绘制（可选平滑 `smoothPath` 贝塞尔）。Q_PROPERTY×2（lineWidth/smooth）；`setLineStyle`（Qt::PenStyle）、`setCullingEnabled`（粗筛剔除）。`hitTest` 像素 in 折线。信号：lineWidthChanged/smoothChanged（+ 继承 dataChanged 等）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_lineWidth` | 线宽（px；Q_PROPERTY lineWidth）。 | `qreal` | `1.0`（构造设定） | — |
| `Qt::PenStyle` | `m_lineStyle` | 线型（setLineStyle）。 | `Qt::PenStyle` | `Qt::SolidLine` | — |
| `bool` | `m_smooth` | 平滑开关（Q_PROPERTY smooth）。 | `true`/`false` | `false` | — |
| `bool` | `m_cullingEnabled` | 粗筛剔除开关。 | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QLineSeries` | 构造函数。 | `name`, `parent` | public | — | 用户/demo | — |
| `void` | `draw` | 折线绘制（smooth → smoothPath 贝塞尔；否则直线段；culling 粗筛；pen=color+lineWidth+lineStyle）。 | `QPainter*` <br> `toPixel` <br> `const DrawContext* ctx=nullptr` | public override | — | `QChartLayer::drawAllSeries` | `DrawContext` |
| `int` | `hitTest` | 像素 in 折线（近线段距离阈值）。 | `pixel, toPixel, ctx` | public override | `int` | `QChartHitTester` | `QChartHitTester` |
| `void` | `setLineWidth` | 线宽 + emit lineWidthChanged。 | `qreal w` | public | — | 用户 | — |
| `void` | `setLineStyle` | 线型（无 NOTIFY）。 | `Qt::PenStyle s` | public | — | 用户 | — |
| `void` | `setSmooth` | 平滑开关 + emit smoothChanged。 | `bool s` | public | — | 用户 | — |
| `void` | `setCullingEnabled` | 剔除开关（无 NOTIFY）。 | `bool v` | public | — | 用户 | — |
| `QPainterPath` | `smoothPath` | 私有：点列 → 平滑贝塞尔路径。 | `const QVector<QPointF>& pts` | private | `QPainterPath` | `draw`（smooth 时） | — |

Notes:
- 属性变化触发前景重绘链：lineWidthChanged/smoothChanged 由 Widget 系列信号连线 → invalidateForeground（QChartSeries 信号槽机制；本类两个 NOTIFY 信号同经 QChartWidget 接线）。
- 3D 折线是独立类（QChartLineSeries3D，见 docs/series/QChartLineSeries3D.md）——2D/3D 不共享实现。

## Overrided Qt Events:
None.

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `lineWidthChanged` | 线宽变化。 | — | `QChartWidget`（seriesAdded 连线 → invalidateForeground） | `QChartWidget` |
| `smoothChanged` | 平滑开关变化。 | — | `QChartWidget`（同上） | `QChartWidget` |
