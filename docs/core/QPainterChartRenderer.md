# QPainterChartRenderer Documentation

## Brief Introduction:
QPainter 后端渲染器（D2 第一层）：持有 bg/fg 两张 QPixmap 缓存 + 脏标记，收编原 QChartWidget 的 drawBackground/drawForeground 绘制编排（轴、网格、系列、调试黄框、图例、3D 前景子路径）。缓存启用：背景脏→重建背景缓存、前景脏→重建前景缓存，再 blit 到目标 device；缓存禁用：直接绘制。3D 子路径（design_3d_axes §7.2）：collect → 分桶（Grid+Series / ForegroundDecor）→ Grid 深度偏置 → 深度降序 → decor 顺序 → labels → 2D overlay 后画。统一后端 CPU 侧实现（D26：渲染+拾取同后端）。

## Constant Variables:
None.（继承 QChartRenderer 的 kGridDepthBias）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QPixmap` | `m_bgCache` | 背景缓存（轴/网格层；脏则重建）。 | `QPixmap` | 空 | `QChartScene` |
| `QPixmap` | `m_fgCache` | 前景缓存（系列/图例/调试黄框；脏则重建）。 | `QPixmap` | 空 | `QChartScene` |
| `bool` | `m_bgDirty` | 背景脏标记。 | `true` <br> `false` | `true` | — |
| `bool` | `m_fgDirty` | 前景脏标记。 | `true` <br> `false` | `true` | — |
| `bool` | `m_cachingEnabled` | 缓存开关（禁用=直接绘制）。 | `true` <br> `false` | `true` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QPainterChartRenderer` | 构造函数（default）。 | — | public | — | QChartWidget 构造（m_renderer） | — |
| — | `~QPainterChartRenderer` | 析构（default）。 | — | public | — | — | — |
| `void` | `render` | 缓存路径：脏→重建对应缓存→blit 到 device（背景 → 前景 → 调试黄框逻辑内）；调用 `drawDirect`（缓存禁用时）。 | `const QChartScene& scene` <br> `QPaintDevice* device` | public override | — | `QChartWidget::paintEvent` | `QChartScene` |
| `void` | `renderUncached` | 无缓存直绘（drawDirect）：导出专用（PNG/SVG/PDF 真矢量，不污染屏显缓存，D13）。 | `const QChartScene& scene` <br> `QPaintDevice* device` | public override | — | `QChartWidget::saveAsPng/Svg/Pdf` | `QChartScene` |
| `void` | `invalidateBackground` | `m_bgDirty=true`。 | 无 | public override | — | `QChartWidget::invalidateBackground` | — |
| `void` | `invalidateForeground` | `m_fgDirty=true`。 | 无 | public override | — | `QChartWidget::invalidateForeground` | — |
| `void` | `setCachingEnabled` | 置缓存开关（关闭时清缓存）。 | `bool enabled` | public override | — | `QChartWidget::setCachingEnabled` | — |
| `bool` | `isCachingEnabled` | 缓存状态访问器（内联）。 | 无 | public override | `true`/`false` | 测试 | — |
| `void` | `drawBackground` | 背景绘制：背景色填充 + 各 layer 轴/网格（axes 遍历 drawAtEdge/drawAtPosition + layer drawGrid）。 | `QPainter* p` <br> `const QChartScene& scene` | private | — | `render`（背景缓存重建）/`drawDirect` | `QChartAxis` <br> `QChartLayer` |
| `void` | `drawForeground` | 前景绘制：各 layer `drawAllSeries`（toPixel 闭包）+ 图例 + 调试黄框（exportMode 跳过）+ 3D 前景子路径（scene.is3D()）。 | `QPainter* p` <br> `const QChartScene& scene` | private | — | `render`（前景缓存重建）/`drawDirect` | `QChartLayer` <br> `QChartLegend` |
| `void` | `drawForeground3D` | 3D 子路径：collect（Layer3D collectPrimitives 图元 + labels）→ 分桶（depthItems=Grid+Series / decor=ForegroundDecor）→ Grid 深度偏置（kGridDepthBias）→ depthItems 降序（远→近）→ decor 顺序 → labels → 2D overlay 后画。 | `QPainter* p` <br> `const QChartScene& scene` | private | — | `drawForeground`（scene.is3D()） | `QChartLayer3D` <br> `QChartPrimitive` |
| `void` | `drawPrimitives` | 逐图元绘制：Point=drawEllipse（markerSize）、LineSegment=drawLine（pen=color+penWidth）。 | `QPainter* p` <br> `const QVector<QChartPrimitive>& items` | private | — | `drawForeground3D` | `QChartPrimitive` |
| `void` | `drawLabels` | billboard 文本（drawText，裁剪 plotArea；isTitle 加大加粗）。 | `QPainter* p` <br> `const QChartScene& scene` <br> `const QVector<QChartTextLabel>& labels` | private | — | `drawForeground3D` | `QChartTextLabel` |
| `void` | `drawDirect` | 无缓存直接绘制（drawBackground + drawForeground）。 | `QPainter* p` <br> `const QChartScene& scene` | private | — | `renderUncached`/`render`（缓存禁用） | — |

Notes:
- 2D 路径逐字节未动红线（Phase 2/3 全程）：3D 段只在 scene.is3D() 时进入 drawForeground3D；2D 场景字段保持默认值零行为变化。
- 深度语义：painter's algorithm（无硬件 z-buffer）——排序在 drawForeground3D 内完成（D16）；GL 后端等价性见 docs/core/deepdive_layerDepth.md。

## Overrided Qt Events:
None.（非 QWidget）

## Signals:
None.（非 QObject）
