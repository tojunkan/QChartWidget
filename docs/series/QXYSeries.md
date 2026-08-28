# QXYSeries Documentation

## Brief Introduction:
2D 折线/点系列基类（QChartSeries 派生）：`QVector<QDataPoint>` 权威存储（Data 空间 QVariant 二元组）。数据维护 append/insert/remove/replace/clear/setPoints（整批替换）。**动画覆盖层**：`setRenderOverride(QVector<QPointF> numericPts)` 优先于真实数据被 draw() 使用（绘制覆盖，QPropertyAnimation 帧驱动）。`draw`（toPixel 闭包连线，NaN 跳过）与 `hitTest`（像素 in 折线多边形）。子类：QLineSeries/QScatterSeries/QPolygonSeries/QRegionSeries。信号：dataChanged/renderOverrideChanged。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVector<QDataPoint>` | `m_points` | 数据点权威列表（Data 空间 QVariant 二元组）。 | `QVector<QDataPoint>` | 空 | `QDataPoint` |
| `QVector<QPointF>` | `m_overridePoints` | 动画覆盖层（Numeric 空间；非空时 draw 优先用）。 | `QVector<QPointF>` | 空 | — |
| `bool` | `m_overrideActive` | 覆盖层激活标记（setRenderOverride 置位/clearRenderOverride 复位）。 | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QXYSeries` | 构造函数。 | `const QString& name={}` <br> `QObject* parent=nullptr` | public | — | 子类构造 | — |
| `const QVector<QDataPoint>&` | `points` | 数据访问器（内联）。 | 无 | public | `QVector<QDataPoint>` | 渲染/测试/数据包围盒 | `QDataPoint` |
| `void` | `setRenderOverride` | 设置动画覆盖层（激活）+ emit renderOverrideChanged。 | `const QVector<QPointF>& numericPts` | public | — | 动画（QNumericSeriesAnimation 等） | `QNumericSeriesAnimation` |
| `void` | `clearRenderOverride` | 清除覆盖层（复位）+ emit renderOverrideChanged。 | 无 | public | — | 动画结束/用户 | — |
| `const QVector<QPointF>&` | `renderOverride` | 覆盖层访问器（内联）。 | 无 | public | `QVector<QPointF>` | 绘制 | — |
| `void` | `append` | 追加点（qreal 便捷版/`QDataPoint` 版）+ emit dataChanged。 | `qreal x, qreal y` <br> `const QDataPoint& pt` | public | — | 用户/动画 | `QDataPoint` |
| `void` | `insert` | 插入点（index）+ emit dataChanged。 | `int index` <br> `const QDataPoint& pt` | public | — | 用户 | `QDataPoint` |
| `void` | `remove` | 移除点（index）+ emit dataChanged。 | `int index` | public | — | 用户 | — |
| `void` | `replace` | 替换点（index）+ emit dataChanged。 | `int index` <br> `const QDataPoint& pt` | public | — | 用户 | `QDataPoint` |
| `void` | `clear` | 清空 + emit dataChanged。 | 无 | public | — | 用户 | — |
| `void` | `setPoints` | 整批替换（不逐点信号）+ emit dataChanged。 | `const QVector<QDataPoint>& pts` | public | — | 用户/demo | `QDataPoint` |
| `void` | `draw` | toPixel 闭包绘制（覆盖层优先；NaN 跳过；ctx 曲线边）。 | `QPainter*` <br> `toPixel` <br> `const DrawContext* ctx=nullptr` | public override | — | `QChartLayer::drawAllSeries` | `DrawContext` |
| `int` | `hitTest` | 像素 in 折线多边形。 | `pixel, toPixel, ctx` | public override | `int` | `QChartHitTester::hitTest`（2D） | `QChartHitTester` |

Notes:
- 数据维护均为 O(1)/O(n) 直接操作 + 通知（dataChanged）；批量场景用 setPoints 避免逐点信号风暴。
- 覆盖层语义：绘制时 `m_overrideActive ? m_overridePoints : m_points`（Numeric→Pixel 走 toPixel）——动画帧驱动整批替换。

## Overrided Qt Events:
None.（QObject）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `dataChanged` | 数据变化（append/insert/remove/replace/clear/setPoints）。 | — | `QChartWidget`（addLayer seriesAdded 连线 → invalidateForeground，src/core/QChartWidget.cpp:84 附近） | `QChartWidget` |
| `renderOverrideChanged` | 覆盖层变化。 | — | `QChartWidget`（连线 → invalidateForeground，:93-97） | `QChartWidget` |

Notes:
- Connected slots 实测：QChartWidget::addLayer 的 seriesAdded 槽内对 QXYSeries 类型 connect renderOverrideChanged（:93）、QBarSeries 同款（:96）。
