# QBarSeries Documentation

## Brief Introduction:
柱状系列（2D，QChartSeries 直接派生）：`QVector<QDataRect>` 权威存储（Data 空间矩形）。绘制快路径：Cartesian 下四角投影后构成轴对齐矩形 → `drawRect`；Polar/Functional 变形 → `drawPolygon`。**动画覆盖层**：`setRenderOverride(QVector<QRectF> numericRects)`（Numeric 空间，优先于真实数据）。信号：dataChanged/renderOverrideChanged。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVector<QDataRect>` | `m_rects` | 柱矩形权威列表（Data 空间）。 | `QVector<QDataRect>` | 空 | `QDataRect` |
| `QVector<QRectF>` | `m_overrideRects` | 动画覆盖层（Numeric 空间；非空时 draw 优先用）。 | `QVector<QRectF>` | 空 | — |
| `bool` | `m_overrideActive` | 覆盖层激活标记。 | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QBarSeries` | 构造函数。 | `name`, `parent` | public | — | 用户/demo（柱状图/排序动画） | — |
| `void` | `append` | 追加柱（qreal×4 便捷版/`QDataRect` 版）+ emit dataChanged。 | `qreal left, top, right, bottom` <br> `const QDataRect& rect` | public | — | 用户/动画 | `QDataRect` |
| `void` | `replace` | 替换柱（index）+ emit dataChanged。 | `int i` <br> `const QDataRect& rect` | public | — | 动画（冒泡排序每帧） | `QDataRect` |
| `void` | `remove` | 移除柱（index）+ emit dataChanged。 | `int i` | public | — | 用户 | — |
| `void` | `clear` | 清空 + emit dataChanged。 | 无 | public | — | 用户 | — |
| `const QVector<QDataRect>&` | `rectangles` | 柱列表访问器（内联）。 | 无 | public | `QVector<QDataRect>` | 渲染/测试 | `QDataRect` |
| `void` | `draw` | 逐柱绘制：四角 toPixel → 轴对齐矩形快路径（drawRect）/变形多边形（drawPolygon）；覆盖层优先。 | `QPainter*` <br> `toPixel` <br> `const DrawContext* ctx=nullptr` | public override | — | `QChartLayer::drawAllSeries` | `DrawContext` |
| `int` | `hitTest` | 像素 in 柱矩形。 | `pixel, toPixel, ctx` | public override | `int` | `QChartHitTester` | `QChartHitTester` |
| `void` | `setRenderOverride` | 设置动画覆盖层 + emit renderOverrideChanged。 | `const QVector<QRectF>& numericRects` | public | — | `QBarAnimation` | `QBarAnimation` |
| `void` | `clearRenderOverride` | 清除覆盖层 + emit renderOverrideChanged。 | 无 | public | — | 动画结束 | — |

Notes:
- 绘制快路径判定：Cartesian 投影下四角构成轴对齐矩形（宽/高非零）→ drawRect（性能）；否则 drawPolygon（Polar/Functional 正确性）。
- 3D 无柱状系列（Phase 3+ 候选）。

## Overrided Qt Events:
None.

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `dataChanged` | 数据变化。 | — | `QChartWidget`（seriesAdded 连线 → invalidateForeground） | `QChartWidget` |
| `renderOverrideChanged` | 覆盖层变化。 | — | `QChartWidget`（连线 → invalidateForeground，src/core/QChartWidget.cpp:96-97） | `QChartWidget` |
