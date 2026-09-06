# QChartLayer Documentation

## Brief Introduction:
QChartLayer 是**图层基类**（QObject，Q_OBJECT）：按轴绑定的 axisX/axisY 与**自持值成员相机**（批次 A：相机归 layer——`m_camera` 值成员，构造即 `m_scene.camera = &m_camera`，恒指向本层相机）组织一层的网格/系列绘制。`collectPrimitives()` → `drawGrid(m_scene)`：以 axisY 的 tickValues 作 offset 沿 dim0 反复 `axisX->drawAtPosition(...)`、以 axisX 的 tickValues 作 offset 沿 dim1 画 axisY（"每网格脊带标签"语义沿用，labels=true），addLine 对产出图元回填 gridColor/penWidth/sourceId(=maxSourceId) 并 push PrimitiveIdPrefixSum。网格样式：gridVisible + gridColor（override/主题默认双色模型，同值忽略、变更发 gridChanged→invalidateData 自连接）。**Series 管理（addSeries/removeSeries/…）、drawAllSeries、makeToPixel、hitTest 注释保留（随 Series/拾取阶段恢复）**；`recomputeDataBounds()` 现为虚默认空实现（批次 A 由纯虚放宽）。旧 `(projection, plotArea)` 构造声明已注释（待 Widget 阶段恢复完整实现）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartAxis*` | `m_axisX` | （protected）X/主维轴绑定（非持有；setAxisX 注入） | 指针/`nullptr` | `nullptr` | `QChartAxis` |
| `QChartAxis*` | `m_axisY` | （protected）Y/次维轴绑定（非持有） | 指针/`nullptr` | `nullptr` | `QChartAxis` |
| `QRectF` | `m_dataBounds` | （protected）由 axisX/axisY 的 min/max 计算（setAxisX/setAxisY 写入：axisX 定 left/right、axisY 定 top/bottom），亦可整体注入（setNumericBounds），供 drawGrid/collectPrimitives | `QRectF` | 空 | — |
| `QChartCamera` | `m_camera` | （protected）★ 相机**值成员**（批次 A：相机归 layer）；构造时 `m_scene.camera=&m_camera`；viewRect 由 Widget 驱动链或调用方显式设置后再渲染 | `QChartCamera` | 构造创建 | `QChartCamera` |
| `QChartScene` | `m_scene` | （protected）当前场景快照（collectPrimitives 填充；camera 恒指向 m_camera，projection/plotArea/backgroundColor 由 widget 注入） | `QChartScene` | 构造创建 | `QChartScene` |
| `bool` | `m_dataDirty` | （protected）数据脏标记（invalidateData 置位；gridChanged→invalidateData 自连接） | `true`/`false` | `true` | — |
| `bool` | `m_gridVisible` | （protected）网格可见开关 | `true`/`false` | `true` | — |
| `std::optional<QColor>` | `m_gridColorOverride` | （protected）用户显式网格色（setGridColor 写入；无值 = 用主题默认） | `std::optional<QColor>`/`std::nullopt` | `std::nullopt` | — |
| `QColor` | `m_themeGridColor` | （protected）主题注入默认网格色（setThemeGridColor；无 override 时变化发 gridChanged） | `QColor` | `QColor(220,220,220)` | `QChartTheme` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartLayer` | 构造：连接 gridChanged→invalidateData；**`m_scene.camera=&m_camera`**（批次 A）；Widget 自动注入分支与 series 信号连接随 Widget/Series 阶段恢复 | `QObject* parent=nullptr` | public | — | Widget/用户 addLayer（S0 未实例化——测试以直接 drawAtPosition 复刻语义） | — |
| — | `~QChartLayer` | 析构（default） | 无 | public | — | — | — |
| `QChartAxis*` | `axisX` | 轴访问器（内联） | 无 | public | 指针 | 测试/用户 | `QChartAxis` |
| `QChartAxis*` | `axisY` | 轴访问器（内联） | 无 | public | 指针 | 测试/用户 | `QChartAxis` |
| `void` | `setAxisX` | 绑定 X 轴；非空时按 `axis->min()/max()` 更新 m_dataBounds left/right | `QChartAxis* a` | public | — | Widget/用户 | `QChartAxis` |
| `void` | `setAxisY` | 绑定 Y 轴；非空时按 `axis->max()/min()` 更新 m_dataBounds top/bottom | `QChartAxis* a` | public | — | Widget/用户 | `QChartAxis` |
| `virtual bool` | `validateAxes` | 校验双轴非空；任一空 qWarning + false | 无 | public | `true`/`false` | Widget 阶段 | — |
| `QChartCamera*` | `camera` | 本层相机访问器（内联；非 const 重载） | 无 | public | 指针 | Widget/渲染装配 | `QChartCamera` |
| `const QChartCamera*` | `camera` | 本层相机访问器（const 重载，内联） | 无 | public | 指针 | — | `QChartCamera` |
| `void` | `setSceneProjection` | 注入场景投影（widget 渲染前调用，内联） | `const QChartAbstractProjection* p` | public | — | Widget 阶段 | `QChartAbstractProjection` |
| `void` | `setScenePlotArea` | 注入场景绘制区（内联） | `const QRectF& plotArea` | public | — | Widget 阶段 | — |
| `void` | `setSceneBackground` | 注入场景底色（内联） | `const QColor& c` | public | — | Widget 阶段 | — |
| `void` | `setNumericBounds` | 设置 Numeric 数据范围（legacy 取向：left/right=dim0 极值、bottom/top=dim1 极值；直接写 m_dataBounds）并**同步已绑定轴语法糖范围**——axisX: left→right（需 left≤right）、axisY: bottom→top（需 bottom≤top）调 `QChartAxis::setRange`（→ rangeChanged+styleChanged）；widget viewRect→dataBounds 驱动链末端 | `const QRectF& bounds` | public | — | Widget 阶段 | `QChartAxis` |
| `const QChartScene&` | `scene` | 收集结果快照访问器（collectPrimitives 之后用；const） | 无 | public | — | Widget render 前 | `QChartScene` |
| `QChartScene&` | `scene` | 快照访问器（非 const 重载） | 无 | public | — | 内部/调用方 | `QChartScene` |
| `void` | `drawGrid` | 画网格数据主脊：gridVisible/轴空守卫；segments=m_scene.projection 的 samplingSegmentsHint（无投影 72）；addLine 语义见引言；dim0 方向扫 axisX（offsets=axisY ticks）、dim1 方向扫 axisY（offsets=axisX ticks） | `QChartScene& scene` | public | — | `collectPrimitives` 内部；Widget 阶段 | `QChartAxis` |
| `void` | `collectPrimitives` | 收集本层图元到 m_scene：**批次 A 起每次收集前复位场景负载**（primitives/labels 清空、maxSourceId=0、PrimitiveIdPrefixSum 重置为 {0}——widget 每帧重收集，sourceId 从 0 重新分配）→ `drawGrid(m_scene)`（S0 只收集网格；drawAllSeries 届时恢复） | 无 | public | — | Widget 阶段 buildScene | — |
| `void` | `invalidateData` | 数据脏标记置位（内联） | 无 | public | — | 构造自连接（gridChanged 槽） | — |
| `bool` | `isGridVisible` | 网格可见访问器（内联） | 无 | public | `true`/`false` | 测试 | — |
| `void` | `setGridVisible` | 设置网格可见；同值忽略，变化发 gridChanged | `bool v` | public | — | 用户/测试 | — |
| `QColor` | `gridColor` | 有效网格色 = override 或主题默认（内联） | 无 | public | `QColor` | `drawGrid` 内部 | — |
| `void` | `setGridColor` | 写 override（同值忽略）；发 gridChanged | `const QColor& c` | public | — | 用户/测试 | — |
| `void` | `setThemeGridColor` | 主题注入默认色（内联）；仅无 override 时发 gridChanged | `const QColor& c` | public | — | Widget 主题推送 | `QChartTheme` |
| `void` | `clearGridColor` | 清 override 回主题默认（内联）；无 override 则不动 | 无 | public | — | 用户 | `QChartTheme` |
| `std::optional<QColor>` | `gridColorOverride` | override 访问器（内联） | 无 | public | `std::optional<QColor>` | 主题判断 | — |
| `virtual void` | `recomputeDataBounds` | 交互/数据链虚函数：**S0 默认空实现**（批次 A 由纯虚放宽；头注释：交互/数据链阶段可视需要恢复纯虚） | 无 | public | — | S0 无调用方 | — |
| — | `HitResult` | using 别名 = `QChartHitTester::HitResult`（拾取定义提升于此，S0 仅类型别名） | — | public | — | 拾取阶段 | `QChartHitTester` |

Notes:
- 头注释保留（非删除）项：Series 管理四方法、drawAllSeries、makeToPixel、hitTest、hook/unhookSeriesDirty、seriesAdded/seriesRemoved 信号——随 Series/拾取/Widget 阶段恢复（R11）。
- Q_PROPERTY：gridVisible/gridColor（NOTIFY gridChanged）。
- S0 状态：本类 cpp 入 QCHART_SOURCES 但无实例化/调用方（轴矩阵测试以直接 drawAtPosition 装配等价几何）；批次 A"相机归 layer"已随头文件与实现同步落地。

## Overrided Qt Events:
无（QObject 非 QWidget；本类无 Qt 事件覆写）。

## Signals:

| Name | Description | Parameters | Emitted By | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `gridChanged` | 网格样式/可见性变化 | 无 | `setGridVisible`/`setGridColor`/`setThemeGridColor`（无 override 时）/`clearGridColor`（有 override 时） | — |

（seriesAdded/seriesRemoved 注释保留，待 Series 阶段恢复）
