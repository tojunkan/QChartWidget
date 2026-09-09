# QChartLayer Documentation

## Brief Introduction:
QChartLayer 是**图层基类**（QObject，Q_OBJECT；v2 修订：批次 A 相机归层 + widget 容器渲染链语义定稿）：按轴绑定的 axisX/axisY 与**自持值成员相机**（`m_camera`，构造即 `m_scene.camera=&m_camera`）组织一层的网格/系列绘制；2D 渲染单位（QChartAbstractWidget::renderLayers/renderLayersGL 每帧逐层 `collectPrimitives()` → renderer.render(layer->scene(), device)）。`collectPrimitives()`：**每次收集前复位场景负载**（primitives/labels 清空、maxSourceId=0、PrimitiveIdPrefixSum 重置 {0}——每帧重收集、sourceId 从 0 起重新分配）→ `drawGrid(m_scene)`：以 axisY tickValues 作 offset 沿 dim0 反复 `axisX->drawAtPosition(...)`、以 axisX tickValues 沿 dim1 画 axisY（"每网格脊带标签"语义沿用），addLine 回填 gridColor/penWidth/sourceId(=maxSourceId) + 前缀和。`setNumericBounds(bounds)`：写 m_dataBounds 并**同步已绑定轴语法糖范围**（X: left→right、Y: bottom→top，须 bottom≤top 才 setRange）——widget viewRect→dataBounds 驱动链末端。网格样式 gridVisible/gridColor（override/主题默认双色；gridChanged→invalidateData 自连接）。**Series 管理/hitTest/makeToPixel/drawAllSeries 注释保留（随 Series/拾取阶段恢复）**；`recomputeDataBounds()` 虚默认空实现（批次 A 由纯虚放宽）。旧 `(projection, plotArea)` 构造声明已注释（待 Widget 阶段恢复）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartAxis*` | `m_axisX` | （protected）X/主维轴绑定（非持有） | 指针/`nullptr` | `nullptr` | `QChartAxis` |
| `QChartAxis*` | `m_axisY` | （protected）Y/次维轴绑定（非持有） | 指针/`nullptr` | `nullptr` | `QChartAxis` |
| `QRectF` | `m_dataBounds` | （protected）Numeric 范围（legacy 取向：left/right=dim0、top/bottom=dim1 数值极值；setAxisX/Y 与 setNumericBounds 写入） | `QRectF` | 空 | — |
| `QChartCamera` | `m_camera` | （protected）★ 相机**值成员**（批次 A：相机归 layer；构造 `m_scene.camera=&m_camera`；viewRect 由 widget 驱动链/调用方设置） | `QChartCamera` | 构造创建 | `QChartCamera` |
| `QChartScene` | `m_scene` | （protected）当前场景快照（collectPrimitives 填充；camera 恒指向 m_camera；projection/plotArea/背景由容器 pushContextToLayers 注入） | `QChartScene` | 构造创建 | `QChartScene` |
| `bool` | `m_dataDirty` | （protected）数据脏标记（invalidateData 置位；gridChanged→自连接） | `true`/`false` | `true` | — |
| `bool` | `m_gridVisible` | （protected）网格可见 | `true`/`false` | `true` | — |
| `std::optional<QColor>` | `m_gridColorOverride` | （protected）用户显式网格色（setGridColor） | 值/`std::nullopt` | `std::nullopt` | — |
| `QColor` | `m_themeGridColor` | （protected）主题注入默认网格色（setThemeGridColor） | `QColor` | `QColor(220,220,220)` | `QChartTheme` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartLayer` | 构造：连接 gridChanged→invalidateData；**`m_scene.camera=&m_camera`**（批次 A）；Widget 自动注入/series 连接随阶段恢复 | `QObject* parent=nullptr` | public | — | QChartWidget::addLayer（demo/测试）、QChartLayer3D 基类 | — |
| — | `~QChartLayer` | 析构（default） | 无 | public | — | — | — |
| `QChartAxis*` | `axisX/axisY` | 轴访问器（内联） | 无 | public | 指针 | QChartWidget（便捷轴挂载/移除、calculatePlotArea、drawExternalContent） | — |
| `void` | `setAxisX/setAxisY` | 绑定轴；非空时按轴 min/max 更新 m_dataBounds（axisX 定 left/right；axisY 定 top/bottom） | `QChartAxis* a` | public | — | QChartWidget::attachToLayer、QChartWidget3D 透传、用户 | `QChartAxis` |
| `virtual bool` | `validateAxes` | 双轴非空校验（任一空 qWarning + false） | 无 | public | `true`/`false` | 绘制守卫（未来 drawAllSeries） | — |
| `QChartCamera*` | `camera` | 层相机访问器（内联；非 const/const） | 无 | public | 指针 | QChartWidget（viewRect/setViewRect/pan/zoom/cartesianToPixel/pixelToCartesian 广播）、测试 | `QChartCamera` |
| `void` | `setSceneProjection/PlotArea/Background` | 场景上下文注入（内联） | 指针/`QRectF`/`QColor` | public | — | `QChartAbstractWidget::pushContextToLayers` | — |
| `void` | `setNumericBounds` | 设 Numeric 范围（直接写 m_dataBounds）并同步绑定轴语法糖 setRange（X: left→right 需 left≤right；Y: bottom→top 需 bottom≤top；→ rangeChanged+styleChanged） | `const QRectF& bounds` | public | — | QChartWidget::recomputeDataBounds/onBeforePaint（广播） | `QChartAxis` |
| `const QChartScene&`/`QChartScene&` | `scene` | 快照访问器（collectPrimitives 之后使用） | 无 | public | — | QChartAbstractWidget::renderLayers（render 入参）、测试 | — |
| `void` | `drawGrid` | 画网格数据主脊：gridVisible/轴空守卫；segments=m_scene.projection 的 samplingSegmentsHint（无投影 72）；addLine 语义见引言（两方向 tick 脊带标签） | `QChartScene& scene` | public | — | `collectPrimitives` 内部 | `QChartAxis` |
| `void` | `collectPrimitives` | 收集本层图元到 m_scene：**每帧复位负载**（primitives/labels/maxSourceId=0/前缀和{0}）→ drawGrid(m_scene)（S0 只网格；drawAllSeries 届时恢复） | 无 | public | — | `QChartAbstractWidget::renderLayers/renderLayersGL`、测试直接调用 | — |
| `void` | `invalidateData` | 数据脏标记置位（内联） | 无 | public | — | 构造自连接（gridChanged 槽） | — |
| `bool` | `isGridVisible` | 访问器（内联） | 无 | public | `true`/`false` | 测试 | — |
| `void` | `setGridVisible` | 同值忽略，变化发 gridChanged | `bool v` | public | — | 用户/测试 | — |
| `QColor` | `gridColor` | 有效网格色 = override 或主题默认（内联） | 无 | public | `QColor` | drawGrid | — |
| `void` | `setGridColor` | 写 override（同值忽略）→ gridChanged | `const QColor& c` | public | — | 用户/测试 | — |
| `void` | `setThemeGridColor` | 主题注入（内联）；仅无 override 时 gridChanged | `const QColor& c` | public | — | Widget 主题推送（阶段） | `QChartTheme` |
| `void` | `clearGridColor` | 清 override（无则不动）→ gridChanged | 无 | public | — | 用户 | — |
| `std::optional<QColor>` | `gridColorOverride` | override 访问器（内联） | 无 | public | 值 | 主题判断 | — |
| `virtual void` | `recomputeDataBounds` | 交互/数据链虚函数：S0 默认空实现（批次 A 由纯虚放宽） | 无 | public | — | S0 无调用方 | — |
| — | `HitResult` | using = QChartHitTester::HitResult（拾取定义提升；S0 仅类型别名） | — | public | — | 拾取阶段 | `QChartHitTester` |

Notes:
- 头注释保留（恢复点）：Series 管理四方法（`addSeries(QChartSeries*)/removeSeries(QChartSeries*)/clearSeries()/seriesList()`）、drawAllSeries、makeToPixel、hitTest、hookSeriesDirty/unhookSeriesDirty、seriesAdded/seriesRemoved——随 Series/拾取/Widget 阶段恢复（R11）。
- Q_PROPERTY：gridVisible/gridColor（NOTIFY gridChanged）。
- 2D 渲染链实测：容器 paintEvent → pushContextToLayers（幂等）→ renderLayers：每层 collectPrimitives + renderer.invalidateView（每层场景各自重算）+ render(layer->scene(), device)；场景含 plotArea 对齐（GL 容器经 renderLayersGL 时 device=labelDev QImage，标签锚点经 renderer translate 落局部坐标）。
- S0 实例化方：demo_axis（new QChartLayer(w) + setGridVisible + w->addAxis×2 + w->addLayer）、测试（轴矩阵以直接 drawAtPosition 复刻 drawGrid 语义 + TestWidgetSmoke 经容器）。

## Overrided Qt Events:
无（QObject 非 QWidget；本类无 Qt 事件覆写）。

## Signals:

| Name | Description | Parameters | Emitted By | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `gridChanged` | 网格样式/可见性变化 | 无 | `setGridVisible`/`setGridColor`/`setThemeGridColor`（无 override 时）/`clearGridColor`（有 override 时） | — |

（seriesAdded/seriesRemoved 注释保留，待 Series 阶段恢复）
