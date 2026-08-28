# QRegionSeries Documentation

## Brief Introduction:
区域系列（2D，QXYSeries 派生）：闭合区域填充（Data 边界 → 折线 → 闭合区域）——语义 = 折线 + 填充（面积图）。`setFillColor/setFillRule/setStrokeVisible`（与 QPolygonSeries 同款样式组）。`hitTest` 像素 in 区域。无新增 Q_PROPERTY/信号。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QColor` | `m_fillColor` | 填充色。 | `QColor` | 构造默认 | — |
| `Qt::FillRule` | `m_fillRule` | 填充规则。 | `WindingFill`/`OddEvenFill` | `Qt::WindingFill` | — |
| `bool` | `m_strokeVisible` | 描边可见。 | `true`/`false` | `true` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QRegionSeries` | 构造函数。 | `name`, `parent` | public | — | 用户/demo | — |
| `void` | `draw` | 闭合区域填充 + 描边（fillColor/fillRule/strokeVisible；与多边形同款样式组）。 | `QPainter*` <br> `toPixel` <br> `const DrawContext* ctx=nullptr` | public override | — | `QChartLayer::drawAllSeries` | `DrawContext` |
| `int` | `hitTest` | 像素 in 区域。 | `pixel, toPixel, ctx` | public override | `int` | `QChartHitTester` | `QChartHitTester` |
| `void` | `setFillColor` | 填充色（内联）。 | `const QColor& c` | public | — | 用户 | — |
| `void` | `setFillRule` | 填充规则（内联）。 | `Qt::FillRule r` | public | — | 用户 | — |
| `void` | `setStrokeVisible` | 描边开关（内联）。 | `bool v` | public | — | 用户 | — |

Notes:
- 与 QPolygonSeries 的区别：区域 = 数据折线闭合（首尾连回基线/首点），多边形 = 显式顶点闭合；绘制/命中实现各自独立。
- setter 均内联无 NOTIFY——变化后调用方主动 invalidateForeground。

## Overrided Qt Events:
None.

## Signals:
None.（继承 QXYSeries/QChartSeries）
