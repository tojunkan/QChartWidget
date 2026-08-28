# QPolygonSeries Documentation

## Brief Introduction:
多边形系列（2D，QXYSeries 派生）：`draw` 闭合多边形填充 + 描边（`setFillColor/setFillRule/setStrokeVisible`）。`hitTest` 像素 in 多边形（Qt::FillRule 判定）。无新增 Q_PROPERTY/信号（继承 QXYSeries 的 dataChanged 等）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QColor` | `m_fillColor` | 填充色（setFillColor）。 | `QColor` | 构造默认 | — |
| `Qt::FillRule` | `m_fillRule` | 填充规则（setFillRule）。 | `WindingFill`/`OddEvenFill` | `Qt::WindingFill` | — |
| `bool` | `m_strokeVisible` | 描边可见（setStrokeVisible）。 | `true`/`false` | `true` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QPolygonSeries` | 构造函数。 | `name`, `parent` | public | — | 用户/demo | — |
| `void` | `draw` | 闭合多边形（首尾相连）填充 + 描边（fillColor/fillRule/strokeVisible）。 | `QPainter*` <br> `toPixel` <br> `const DrawContext* ctx=nullptr` | public override | — | `QChartLayer::drawAllSeries` | `DrawContext` |
| `int` | `hitTest` | 像素 in 多边形（fillRule 判定）。 | `pixel, toPixel, ctx` | public override | `int` | `QChartHitTester` | `QChartHitTester` |
| `void` | `setFillColor` | 填充色（内联）。 | `const QColor& c` | public | — | 用户 | — |
| `void` | `setFillRule` | 填充规则（内联）。 | `Qt::FillRule r` | public | — | 用户 | — |
| `void` | `setStrokeVisible` | 描边开关（内联）。 | `bool v` | public | — | 用户 | — |

Notes:
- 样式 setter 均内联无 NOTIFY——变化后由调用方主动 invalidateForeground（或经数据/属性链路）。
- 3D 无对应类（曲面是网格线框，非多边形）。

## Overrided Qt Events:
None.

## Signals:
None.（继承 QXYSeries/QChartSeries）
