# QChartWidget Documentation

## Brief Introduction:
QChartWidget acts as the main controller for the five‑space model. It exclusively holds the Projection (coordinate mapping), Camera2D (View‑to‑Pixel geometry), and Renderer (rendering backend), and coordinates the assembly and interaction of Axes and Layers. All 2D charts (Cartesian, Polar, Functional) are instantiated via this class. 3D charts are extended through its sole subclass QChartWidget3D, which reuses Theme, Legend, and Renderer components.

数据与几何分工：viewRect 几何归 `QChartCamera2D`（含 fit 策略与 View↔Pixel 映射）；`m_dataBounds`（Numeric 范围）由 Widget 持有并依赖 `projection->computeDataBounds()/computeViewRect()` 反算/正算；渲染走「场景快照 + 参数化渲染」（`buildScreenScene()` → `m_renderer->render(scene, device)`，D2/D13）。交互（pan/zoom/hover/图例点击）与主题/图例/导出编排均在本类。

## Constant Variables:
None.（类级无常量；`wheelEvent` 内局部 `static constexpr qreal SCALE_SENSITIVITY = 0.001` 不属于类成员）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `std::unique_ptr<QChartProjection>` | `m_projection` | （protected）**唯一** Projection 持有者：Numeric↔View Cartesian 映射与包络计算。`setProjection()` 独占赋值。 | `std::unique_ptr<QCartesianProjection>` <br> `std::unique_ptr<QPolarProjection>` <br> `std::unique_ptr<QFunctionalProjection>` <br> `nullptr` | `nullptr` | `QChartProjection` <br> `QCartesianProjection` <br> `QPolarProjection` <br> `QFunctionalProjection` <br> `QChartProjectionFactory` |
| `QChartProjection*` | `m_tempProjection` | （protected）动画临时投影（**非持有**，仅影响渲染路径 DrawContext；`setTemporaryProjection/clearTemporaryProjection` 管理，动画结束必须清除）。 | `QChartProjection*` <br> `nullptr` | `nullptr` | `QChartProjection` <br> `QProjectionSwitchAnimation` |
| `std::unique_ptr<QChartCamera2D>` | `m_camera` | （protected）2D 相机：viewRect 几何 + fit 策略 + View↔Pixel 线性映射（唯一实现）。 | `std::unique_ptr<QChartCamera2D>` | 构造创建 | `QChartCamera2D` |
| `std::unique_ptr<QChartRenderer>` | `m_renderer` | （protected）渲染后端：缓存 + 绘制编排（`render(scene, device)`）；缓存脏标记迁入渲染器。 | `std::unique_ptr<QPainterChartRenderer>` <br>（GL 由 3D 子类替换） | 构造创建 | `QChartRenderer` <br> `QPainterChartRenderer` |
| `QRectF` | `m_dataBounds` | （protected）当前 Numeric 范围（从 viewRect 经 `projection->computeDataBounds` 反算，Widget 持有）。 | `QRectF` | `QRectF()` | `QChartProjection` |
| `bool` | `m_viewInitialized` | （protected）viewRect 是否已初始化（`setProjection` 首次用 defaultDataBounds 初始化）。 | `true` <br> `false` | `false` | — |
| `QList<QChartLayer*>` | `m_layers` | （protected）图层列表（非持有；`addLayer()/removeLayer()` 管理）。 | `QList<QChartLayer*>` | 空 | `QChartLayer` |
| `QList<QChartAxis*>` | `m_axes` | （protected）轴列表（非持有；`addAxis/removeAxis` 管理）。 | `QList<QChartAxis*>` | 空 | `QChartAxis` |
| `QRectF` | `m_plotArea` | （protected）像素绘制区（`layoutAxes()` 计算；映射/命中/滚轮判定均以此为准）。 | `QRectF` | `QRectF()` | `QChartCamera2D` |
| `bool` | `m_layoutDirty` | （protected）布局脏标记（`invalidateLayout` 置 true；`paintEvent` 消费）。 | `true` <br> `false` | `true` | — |
| `bool` | `m_panEnabled` | （protected）平移开关（`Q_PROPERTY panEnabled`）。 | `true` <br> `false` | `true` | — |
| `bool` | `m_zoomEnabled` | （protected）缩放开关（`Q_PROPERTY zoomEnabled`）。 | `true` <br> `false` | `true` | — |
| `QPointF` | `m_panStart` | （protected）拖拽平移起点（像素；`mousePressEvent` 记录、`mouseMoveEvent` 消费）。 | `QPointF` | `QPointF()` | — |
| `bool` | `m_panning` | （protected）是否正在拖拽平移。 | `true` <br> `false` | `false` | — |
| `QChartSeries*` | `m_hoverSeries` | （protected）当前悬停系列（`mouseMoveEvent` 命中写入；`leaveEvent` 清空）。 | `QChartSeries*` <br> `nullptr` | `nullptr` | `QChartSeries` <br> `QChartHitTester` |
| `int` | `m_hoverIndex` | （protected）当前悬停数据索引（与 `m_hoverSeries` 配对）。 | `int` | `-1` | `QChartSeries` |
| `qreal` | `m_marginLeft` | （protected）左边距（`setMargins` 修改）。 | `qreal` | `20.0` | — |
| `qreal` | `m_marginTop` | （protected）上边距。 | `qreal` | `20.0` | — |
| `qreal` | `m_marginRight` | （protected）右边距。 | `qreal` | `20.0` | — |
| `qreal` | `m_marginBottom` | （protected）下边距。 | `qreal` | `20.0` | — |
| `QChartTheme` | `m_theme` | （private）当前主题（base，不含 override；`setTheme()` 赋值，`pushTheme()` 推送）。 | `QChartTheme::light()` <br> `QChartTheme::dark()` <br> 自定义 `QChartTheme` | `QChartTheme::light()` | `QChartTheme` |
| `std::optional<QColor>` | `m_backgroundColorOverride` | （private）背景显式覆盖色（D12 双槽：override 优先于主题默认；`clearBackgroundColor` 清空）。 | `std::optional<QColor>` <br> `std::nullopt` | `std::nullopt` | `QChartTheme` |
| `bool` | `m_followSystemPalette` | （private）系统深/浅自动跟随开关（A4，默认关；`event` 处理 `ApplicationPaletteChange`）。 | `true` <br> `false` | `false` | — |
| `int` | `m_seriesColorIndex` | （private）A5 全局系列调色板索引（跨 layer 递增；`assignSeriesPaletteColor` 消费）。 | `int` | `0` | `QChartSeries` |
| `QChartLegend*` | `m_legend` | （private）图例（构造函数创建并 parent；`legend()` 暴露；可见性/对齐信号已连线）。 | `QChartLegend*` | 构造创建 | `QChartLegend` |
| `QList<QChartSeries*>` | `m_legendItems` | （private）图例条目（paint 前 `rebuildLegendItems` 重建：汇总所有 layer、跳过空 name、按 add 顺序）。 | `QList<QChartSeries*>` | 空 | `QChartSeries` <br> `QChartLegend` |
| `bool` | `m_exportTransparentBackground` | （private）导出透明背景开关（C5，默认 false = 用主题背景填充）。 | `true` <br> `false` | `false` | — |

Notes:
- 所有权：`m_projection/m_camera/m_renderer` 为 unique_ptr（独占）；`m_layers/m_axes` 为裸指针列表（**非持有**，由调用方保证生命周期）；`m_legend` 为 parented 裸指针（Qt 父子所有权）；`m_tempProjection` 非持有（动画临时）。
- 访问级别：以上均无 public 成员变量；protected 14 项（子类 QChartWidget3D 可访问 m_projection/m_camera/m_renderer 等），private 10 项。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `void` | `addLayer` | 添加图层；连接 layer 的 seriesAdded/seriesRemoved → 系列属性信号连线与图例重建。 | `QChartLayer* g` | public | — | 用户/demo/测试 | `QChartLayer` |
| `void` | `removeLayer` | 移除图层（断开信号）。 | `QChartLayer* g` | public | — | 用户/demo/测试 | `QChartLayer` |
| `QList<QChartLayer*>` | `layers` | 图层列表访问器（内联）。 | 无 | public | — | 测试/渲染 | `QChartLayer` |
| `void` | `addAxis` | 添加轴；连接 rangeChanged→dataBounds 重算、visible/style/tickCountChanged→invalidateBackground；触发布局失效。 | `QChartAxis* a` | public | — | 用户/demo/测试 | `QChartAxis` |
| `void` | `removeAxis` | 移除轴（断开信号）。 | `QChartAxis* a` | public | — | 用户/demo/测试 | `QChartAxis` |
| `QList<QChartAxis*>` | `axes` | 轴列表访问器（内联）。 | 无 | public | — | 测试/渲染 | `QChartAxis` |
| `QPointF` | `cartesianToPixel` | View Cartesian → Pixel（转发 `QChartCamera2D::cartesianToPixel`，所有投影通用）。 | `qreal cx, qreal cy` | public | — | DrawContext/Layer/命中 | `QChartCamera2D` |
| `QPointF` | `pixelToCartesian` | Pixel → View Cartesian（逆映射转发）。 | `const QPointF& pixel` | public | — | `mouseMoveEvent()` <br> `wheelEvent()` | `QChartCamera2D` |
| `void` | `setViewRect` | 绝对设置 viewRect；自动反算 dataBounds + invalidate + 发 viewChanged。 | `const QRectF& r` | public | — | `QViewRectAnimation` <br> `setDataRangeDim0()` <br> `setDataRangeDim1()`（经相机）/用户 | `QChartCamera2D` <br> `QChartProjection` |
| `void` | `panViewCartesian` | 平移 viewRect（View Cartesian 空间，dx/dy）。 | `qreal dx, qreal dy` | public | — | `mouseMoveEvent`（拖拽） | `QChartCamera2D` |
| `void` | `zoomViewCartesian` | 以 (cx,cy) 为中心缩放（factorX/factorY 独立；factor<1=放大）。 | `qreal cx, qreal cy` <br> `qreal factorX, qreal factorY` | public | — | `wheelEvent`/用户 | `QChartCamera2D` |
| `void` | `setDataRangeDim0` | 语法糖：改 dataBounds dim0 → 重算 viewRect（KeepWidth fit）+ 发 viewChanged。 | `qreal min, qreal max` | public | — | 用户/测试 | `QChartProjection` <br> `QChartCamera2D` |
| `void` | `setDataRangeDim1` | 语法糖：改 dataBounds dim1 → 重算 viewRect（KeepHeight fit）+ 发 viewChanged。 | `qreal min, qreal max` | public | — | 用户/测试 | `QChartProjection` <br> `QChartCamera2D` |
| `void` | `setProjection` | 设置唯一投影；首次用 defaultDataBounds 初始化 viewRect（KeepCenter fit）+ 同步坐标系到所有 Layer + invalidate。 | `std::unique_ptr<QChartProjection> proj` | public | — | 用户/demo（构造/切换） | `QChartProjection` <br> `QChartLayer` |
| `const QChartProjection*` | `projection` | 当前投影访问器（内联）。 | 无 | public | — | 测试/渲染 | `QChartProjection` |
| `void` | `setTemporaryProjection` | 设置动画临时投影（仅渲染路径；须配 `clearTemporaryProjection`）。 | `QChartProjection* p` | public | — | `QProjectionSwitchAnimation` | `QChartProjection` |
| `void` | `clearTemporaryProjection` | 清除临时投影。 | 无 | public | — | `QProjectionSwitchAnimation` | `QChartProjection` |
| `QRectF` | `viewRect` | 视窗访问器（内联，转发相机）。 | 无 | public | — | 测试/渲染 | `QChartCamera2D` |
| `QRectF` | `dataBounds` | Numeric 范围访问器（内联）。 | 无 | public | — | 测试/渲染 | `QChartProjection` |
| `QRectF` | `plotArea` | 像素绘制区访问器（内联）。 | 无 | public | — | 测试/渲染/命中 | — |
| `void` | `setMargins` | 设置四边距 → layoutAxes + update。 | `qreal l, t, r, b` | public | — | 用户/demo | — |
| `ViewRectFitMode` | `viewRectFitMode` | fit 模式访问器（内联，转发相机）。 | 无 | public | — | 测试 | `QChartCamera2D` |
| `void` | `setViewRectFitMode` | 设置 fit 模式 → invalidateBackground+Foreground。 | `ViewRectFitMode mode` | public | — | 用户 | `QChartCamera2D` |
| `qreal` | `fixedAspectRatio` | Fixed 模式长宽比访问器（内联）。 | 无 | public | — | 测试 | `QChartCamera2D` |
| `void` | `setFixedAspectRatio` | 设置 Fixed 长宽比 → invalidate。 | `qreal ratio` | public | — | 用户 | `QChartCamera2D` |
| `void` | `setTheme` | 一键切换预设主题（Light/Dark）。 | `QChartTheme::Preset preset` | public | — | 用户/demo | `QChartTheme` |
| `void` | `setTheme` | 自定义主题（同 struct）→ pushTheme 推送。 | `const QChartTheme& theme` | public | — | 用户/demo | `QChartTheme` |
| `QChartTheme` | `theme` | 当前主题访问器（内联，base）。 | 无 | public | — | 测试 | `QChartTheme` |
| `void` | `setBackgroundColor` | 背景显式覆盖色（D12）。 | `const QColor& c` | public | — | 用户/demo | `QChartTheme` |
| `void` | `clearBackgroundColor` | 清除背景覆盖（回退主题默认）。 | 无 | public | — | 用户 | `QChartTheme` |
| `QColor` | `backgroundColor` | 有效背景色（override 或主题默认）。 | 无 | public | — | `buildScreenScene`/渲染 | `QChartTheme` |
| `void` | `setFollowSystemPalette` | 系统深/浅自动跟随开关（A4）。 | `bool on` | public | — | 用户 | — |
| `bool` | `followSystemPalette` | 跟随开关访问器（内联）。 | 无 | public | — | 测试 | — |
| `QChartLegend*` | `legend` | 图例访问器（内联）。 | 无 | public | — | 用户/测试/渲染 | `QChartLegend` |
| `void` | `setLegendVisible` | 图例可见性（内联转发）。 | `bool v` | public | — | 用户/demo | `QChartLegend` |
| `bool` | `isLegendVisible` | 图例可见访问器（内联）。 | 无 | public | — | 测试 | `QChartLegend` |
| `void` | `setLegendAlignment` | 图例对齐（内联转发）。 | `Qt::Alignment a` | public | — | 用户 | `QChartLegend` |
| `QList<QChartSeries*>` | `legendItems` | 图例条目访问器（内联）。 | 无 | public | — | 测试/交互（图例点击） | `QChartSeries` <br> `QChartLegend` |
| `bool` | `saveAsPng` | 导出 PNG（便捷重载，默认 WholeWidget）。 | `const QString& path` <br> `const QSize& size` <br> `qreal devicePixelRatio` | public | `true`/`false` | 用户/demo | `QChartRenderer` |
| `bool` | `saveAsPng` | 导出 PNG（显式范围重载）。 | `path, QChartExportScope scope, size, dpr` | public | `true`/`false` | 用户 | `QChartRenderer` <br> `QChartExportScope` |
| `bool` | `saveAsSvg` | 导出 SVG（便捷/显式范围两重载）。 | `path, [scope,] size` | public | `true`/`false` | 用户/demo | `QChartRenderer` |
| `bool` | `saveAsPdf` | 导出 PDF（便捷/显式范围两重载；恒填背景，D13）。 | `path, [scope,] size` | public | `true`/`false` | 用户/demo | `QChartRenderer` |
| `void` | `setExportTransparentBackground` | 导出透明背景开关（C5）。 | `bool v` | public | — | 用户 | — |
| `bool` | `exportTransparentBackground` | 开关访问器（内联）。 | 无 | public | — | 测试 | — |
| `bool` | `isCachingEnabled` | 渲染缓存开关访问器（转发渲染器）。 | 无 | public | `true`/`false` | 测试 | `QChartRenderer` |
| `void` | `setCachingEnabled` | 设置缓存开关 → update。 | `bool v` | public | — | 用户 | `QChartRenderer` |
| `bool` | `isPanEnabled` | 平移开关访问器（Q_PROPERTY）。 | 无 | public | `true`/`false` | 测试 | — |
| `void` | `setPanEnabled` | 设置平移开关。 | `bool v` | public | — | 用户 | — |
| `bool` | `isZoomEnabled` | 缩放开关访问器（Q_PROPERTY）。 | 无 | public | `true`/`false` | 测试 | — |
| `void` | `setZoomEnabled` | 设置缩放开关。 | `bool v` | public | — | 用户 | — |
| `void` | `invalidateBackground` | 背景缓存失效 → update（触发 paintEvent）。 | 无 | public | — | 轴/网格变化相关槽、用户 <br> Specifically: <br> `addAxis()` <br> `removeAxis()` <br> `setViewRectFitMode()` <br> `setScale()` <br> `setViewRect()` <br> `panViewCartesian()` <br> `zoomViewCartesian()` <br> `setDataRangeDim0()` <br> `setDataRangeDim1()` <br> `setProjection()` <br> `resizeEvent()` <br> `setTheme()` <br> `setBackgroundColor()` <br> `clearBackgroundColor()` | `QChartRenderer` |
| `void` | `invalidateForeground` | 前景缓存失效 → update。 | 无 | public | — | 系列/图例变化相关槽、用户 <br> Specifically: <br> `@slot`(`@slot:this`<-`@signal:QChartSeries::colorChanged`)<-`@signal:QChartLayer::seriesAdded` <br> `@slot`(`@slot:this`<-`@signal:QChartSeries::opacityChanged`)<-`@signal:QChartLayer::seriesAdded` <br> `@slot`(`@slot:this`<-`@signal:QChartSeries::visibleChanged`)<-`@signal:QChartLayer::seriesAdded` <br> `@slot`(`@slot:this`<-`@signal:QChartSeries::nameChanged`)<-`@signal:QChartLayer::seriesAdded` <br> `@slot`(`@slot:this`<-`@signal:QXYSereies::renderOverrideChanged`)<-`@signal:QChartLayer::seriesAdded` <br> `@slot`(`@slot:this`<-`@signal:QBarSeries::renderOverrideChanged`)<-`@signal:QChartLayer::seriesAdded` <br> `addLayer()` <br> `@slot:this`<-`@signal:QChartLayer::seriesRemoved` <br> `removeLayer()` <br> `setViewRectFitMode()` <br> `setScale()` <br> `setViewRect()` <br> `panViewCartesian()` <br> `zoomViewCartesian()` <br> `setDataRangeDim0()` <br> `setDataRangeDim1()` <br> `setProjection()` <br> `resizeEvent()` <br> `setTheme()` | `QChartRenderer` |
| `void` | `invalidateLayout` | 布局脏标记置位 → update提交paintEvent申请。 | 无 | public | — | `addAxis()` <br> `removeAxis()` | — |
| `QChartScene` | `buildScreenScene` | 组装屏显场景快照 | 无 | protected virtual | — | `paintEvent` | `QChartScene` <br> `QChartRenderer` |
| `QChartScene` | `buildExportScene` | 组装导出场景（按 scope/size 计算设备尺寸与 plotArea；3D 子类可注入）。 | `QChartExportScope scope` <br> `const QSize& size` <br> `QSizeF& outDeviceSize` | protected virtual | — | `saveAsPng/Svg/Pdf` | `QChartScene` <br> `QChartExportScope` |
| `void` | `layoutAxes` | 按 margins 与 sizeHint 重算 plotArea（不动 viewRect）。 | 无 | protected virtual | — | `resizeEvent()`<br>`paintEvent`<br>`setMargins` | — |
| `void` | `fitViewRectToPlotArea` | 触发相机 fit 几何 + 反算 dataBounds（避免 Polar 往返漂移）。 | `FitStrategy strategy` | protected | — | `setViewRect()`（经布局）<br>`setProjection`<br>`setDataRangeDim0()` <br> `setDataRangeDim1()` | `QChartCamera2D` <br> `QChartProjection` |
| `QString` | `buildHoverTooltip` | 悬停 tooltip 内容（命中点 Data→Numeric 坐标）。 | `QChartLayer* g` <br> `QChartSeries* s` <br> `int index` | protected | — | `mouseMoveEvent` | `QChartLayer` <br> `QChartSeries` |
| `bool` | `dimensionInteractive` | 该维度是否允许交互（任一绑定轴 isInteractive()==false → 禁止，如分类轴）。 | `int dim` | protected | `true`/`false` | `mouseMoveEvent` <br> `wheelEvent` | `QChartAxis` |
| `void` | `pushTheme` | 推送主题默认色到所有子组件（axis/layer/series/legend，A5 调色板循环）。 | 无 | private | — | `setTheme()` | `QChartAxis` <br> `QChartLayer` <br> `QChartSeries` <br> `QChartLegend` |
| `bool` | `assignSeriesPaletteColor` | A5：给无 override 的 series 分配 palette[index % size] 并推进索引；显式色/空调色板不分配。 | `QChartSeries* s` | private | `true`/`false` | `addLayer()` 的 seriesAdded 槽 | `QChartSeries` <br> `QChartTheme` |
| `void` | `rebuildLegendItems` | 重建图例条目（汇总所有 layer、跳过空 name、按 add 顺序）。 | 无 | private | — | `@slot`(`@slot:this`<-`@signal:QChartSeries::nameChanged`)<-`@signal:QChartLayer::seriesAdded` <br> `@slot`<-`@signal:QChartLayer::seriesAdded` <br> `@slot`<-`@signal:QChartLayer::seriesRemoved` <br>`addLayer()` <br> `removeLayer()` | `QChartSeries` <br> `QChartLegend` |
| `QRectF` | `plotAreaForSize` | 给定尺寸下重算 plotArea（复用 layoutAxes 逻辑，供 WholeWidget 导出）。 | `const QSize& size` | private | — | `layoutAxes()` <br> `buildExportScene()` | — |

Notes:
- 访问级别：public 45 个；protected virtual 3 个（buildScreenScene/buildExportScene/layoutAxes，3D 子类覆写点）；protected 3 个（fitViewRectToPlotArea/buildHoverTooltip/dimensionInteractive）；private 4 个（pushTheme/assignSeriesPaletteColor/rebuildLegendItems/plotAreaForSize）。
- 上述表中信号（seriesHovered/viewChanged）与 Qt 事件覆写（paintEvent 等 8 个）已按范式排除，见下两节。
- 内联访问器（layers/axes/projection/viewRect/dataBounds/plotArea 等）由头文件内联实现，无 .cpp 定义；表内「Called By」为常规消费方，不保证穷举。

## Overrided Qt Events:
| Name | Description | Parameters | Triggered By | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `paintEvent` | 1.若 `m_layoutDirty`：layoutAxes + 清脏 + 渲染器背景/前景失效 <br> 2.`buildScreenScene()` 组装场景快照（3D 子类注入 3D 段） <br> 3.`m_renderer->render(scene, this)` 参数化渲染。 | `QPaintEvent*` | `update()` 调用方： <br> `invalidateBackground()`（轴 visible/style/tickCountChanged 等） <br> `invalidateForeground()`（系列/图例属性变化） <br> `invalidateLayout()`（addAxis/removeAxis） <br> `setCachingEnabled()` <br> Qt 系统（resize/expose/遮挡恢复） | `QChartScene` <br> `QChartRenderer` |
| `resizeEvent()` | 渲染器背景/前景失效 + `layoutAxes()`（只更新 plotArea，不动 viewRect）。 | `QresizeEvent*` | Qt 系统 resize（用户拖拽窗口/布局变化） | `QChartRenderer` |
| `event` | 处理 `QEvent::ApplicationPaletteChange`：`m_followSystemPalette` 开启时按系统亮暗切换主题（Qt 6.4 弃用 paletteChanged 的替代）。 | `QEvent* e` | Qt 系统调色板变化（`QGuiApplication::setPalette` 同步投递） | `QChartTheme` |
| `mousePressEvent` | 1.左键且图例可见 → `m_legend->seriesAt` 命中则切换系列可见性（B4，不进入 pan） <br> 2.左键且 `m_panEnabled` → 记录 `m_panStart`、置 `m_panning`、闭手光标。 | `QMouseEvent* e` | 用户鼠标左键按下 | `QChartLegend` |
| `mouseMoveEvent` | 1.平移中：像素位移→View 位移（禁交互维度置 0）→ `panViewCartesian()` <br> 2.悬停：命中 Series 数据点 → tooltip + `emit seriesHovered(s, i, true)`。 | `QMouseEvent* e` | 用户鼠标移动（pan 拖拽/悬停） | `QChartHitTester` <br> `QChartSeries` |
| `mouseReleaseEvent` | 结束平移：`m_panning=false`、恢复箭头光标。 | `QMouseEvent* e` | 用户鼠标左键释放 | — |
| `wheelEvent` | 滚轮缩放：以鼠标 View 坐标为中心 `zoomViewCartesian()`（factor=exp(−delta·0.001)，clamp [0.8,1.25]；禁交互维度 factor=1）。 | `QWheelEvent* e` | 用户滚轮（plotArea 内、`m_zoomEnabled`） | `QChartCamera2D` |
| `leaveEvent` | 悬停结束：`emit seriesHovered(s, i, false)` + 清空悬停状态 + 恢复光标。 | `QEvent*` | 鼠标离开 widget | `QChartSeries` |

Notes:
- 触发链核心：所有 `invalidate*()` 最终 `update()` → `paintEvent`（Qt 合并同帧多次 update）。`invalidateLayout` 仅置脏，实际布局在 paintEvent 内执行（延迟布局）。
- `mouseMoveEvent` 中 pan 分支优先于悬停分支（平移期间不触发 hover）；图例点击在 `mousePressEvent` 中先于 pan 分支判定。

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `seriesHovered` | **still need further support.** <br> 悬停系列广播信号（`mouseMoveEvent` 命中发 `(s,i,true)`；`leaveEvent` 发 `(s,i,false)`）。供自定义 hover 功能使用。 | `QChartSeries*` <br> `int` <br> `bool` | NULL（内部未连线；demo/外部消费方按需连接） | `QChartSeries` <br> `QChartHitTester` |
| `viewChanged` | 视图状态变化广播：`setViewRect()`<br>`panViewCartesian()`<br>`zoomViewCartesian()`<br>`setDataRangeDim0()` <br> `setDataRangeDim1()` 均 `emit viewChanged()`。 | — | NULL（内部未连线；测试/demo 按需连接） | `QChartCamera2D` <br> `QChartProjection` |

Notes:
- 两信号均无内部 Connected slots——Widget 自身用 invalidate* 重绘链路（信号槽表详见 module_core.md §4）；外部（demo/测试/联动）连接点见 Test/demos 与 TestUnit。
- `seriesHovered` 的进一步支撑（如 hover 高亮扩展）属已知遗留（范式原注保留）。
