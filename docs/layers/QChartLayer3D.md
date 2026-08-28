# QChartLayer3D Documentation

## Brief Introduction:
3D 图层（QChartLayer 派生，design_3d §7.1 + design_3d_axes §8.3）：**3D 侧唯一投影点**（toWorld/投影在 Layer3D；QChartAxes3D 只产 Numeric 几何；Renderer 只画——三层分离）。职责：系列存入基类 m_series（复用图例/主题/调色板/所有权）同时登记 m_series3D 类型化遍历；持有 QChartAxes3D 编排器（拥有，setAxisX/Y/Z 自动重绑 dim0/1/2）；`makeProjectFn` 组装 ProjectFn3D 全链闭包；`collectPrimitives` 分层图元收集（Grid 盒/晶格 → Series（worldCache 直算）→ ForegroundDecor → labels）+ worldCache 置脏重建（投影/轴/数据变化才重建，免每帧 O(N)）；`emitLine` 直线采样（identity 快速通道：Numeric≡World 直通、段数=samplingSegmentsHint）。GridMode{Box, Lattice}（§5.1 默认 Box）；gridFloorVisible/gridFloorHalfSize 已移除（§8.5 并入 Box 模式地板网格——历史）。

## Constant Variables:
None.（`GridMode{Box,Lattice}` 为类型级枚举）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartAxis*` | `m_axisZ` | Z 轴（仅 toNumeric；X/Y 复用基类）。 | `QChartAxis*`/`nullptr` | `nullptr` | `QChartAxis` |
| `QList<QChartSeries3D*>` | `m_series3D` | 3D 系列类型化登记（基类 m_series 之外）。 | `QList<QChartSeries3D*>` | 空 | `QChartSeries3D` |
| `const QChartProjection3D*` | `m_projection3D` | 3D 投影（Widget3D 注入，**非持有**；变化 → worldCache 置脏）。 | `const QChartProjection3D*`/`nullptr` | `nullptr` | `QChartProjection3D` |
| `std::unique_ptr<QChartAxes3D>` | `m_axes3D` | 轴参照系编排器（**拥有**）。 | `std::unique_ptr<QChartAxes3D>` | 构造创建 | `QChartAxes3D` |
| `GridMode` | `m_gridMode` | 网格模式（setGridMode）。 | `Box`/`Lattice` | `GridMode::Box` | — |
| `QVector3D` | `m_axesDataMin/Max` | 轴/网格数据盒（Numeric；Widget3D 注入：dataBounds3D 或 A9 域盒）。 | `QVector3D` | `{(0,0,0),(0,0,0)}`（无效） | `QChartWidget3D` |
| `mutable bool` | `m_worldCacheDirty` | worldCache 脏标记（投影/轴/数据变化 → true；collectPrimitives 重建后 false）。 | `true`/`false` | `true` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartLayer3D` | 构造函数（创建 axes3D + 绑定 axisX/Y/Z 配置槽）。 | `QObject* parent=nullptr` | public | — | `QChartWidget3D::addLayer3D` | `QChartWidget3D` |
| `void` | `setAxisX/Y/Z` | 轴绑定（Z 仅 toNumeric）+ **同步 axes3D 配置槽**（dim0/1/2）+ worldCache 置脏。 | `QChartAxis* a` | public | — | Widget3D/用户 | `QChartAxis` <br> `QChartAxes3D` |
| `QChartAxis*` | `axisZ` | Z 轴访问器（内联）。 | 无 | public | `QChartAxis*` | 测试 | — |
| `void` | `addSeries3D` | 添加 3D 系列：基类 addSeries（复用接线）+ m_series3D 登记 + hookSeriesDirty + 置脏。 | `QChartSeries3D* s` | public | — | `QChartWidget3D::addLayer3D` 后用户 | `QChartSeries3D` |
| `void` | `removeSeries3D` | 移除：基类 removeSeries + unhook + 置脏。 | `QChartSeries3D* s` | public | — | 用户 | `QChartSeries3D` |
| `QList<QChartSeries3D*>` | `series3DList` | 3D 系列列表访问器（内联）。 | 无 | public | `QList<QChartSeries3D*>` | 渲染/测试 | `QChartSeries3D` |
| `void` | `setProjection3D` | 注入 3D 投影（非持有；变化 → worldCache 置脏）。 | `const QChartProjection3D* proj` | public | — | `QChartWidget3D::setProjection3D` | `QChartProjection3D` |
| `const QChartProjection3D*` | `projection3D` | 投影访问器（内联）。 | 无 | public | `const QChartProjection3D*` | 渲染/测试 | `QChartProjection3D` |
| `QChartAxes3D*` | `axes3D` | 编排器访问器（内联，const 版）。 | 无 | public | `QChartAxes3D*` | collectPrimitives/测试 | `QChartAxes3D` |
| `void` | `setGridMode` | 网格模式（内联）。 | `GridMode m` | public | — | 用户/demo | — |
| `GridMode` | `gridMode` | 网格模式访问器（内联）。 | 无 | public | `Box`/`Lattice` | collectPrimitives | — |
| `void` | `setAxesDataBox` | 注入轴/网格数据盒（Numeric；默认无效 → 不生成轴/网格图元）。 | `const QVector3D& dataMin` <br> `const QVector3D& dataMax` | public | — | `QChartWidget3D::pushAxesDataBoxToLayers` | `QChartWidget3D` |
| `bool` | `hasValidAxesDataBox` | 数据盒有效判定（min≤max 且 min≠max）。 | 无 | public | `true`/`false` | collectPrimitives（守卫） | — |
| `void` | `collectPrimitives` | **分层收集（★核心）**：①worldCache 重建（dirty 时：数值型=toWorld(numericCache)、曲面=toWorld(toNumeric(grid))、混合不填）②轴/网格图元（Grid 层：盒模式 tick 对齐网格/晶格三族 + ForegroundDecor 层：spine/刻度点/标签）③系列图元（Series 层，makeProjectFn 闭包）④labels 出参。 | `const QChartCamera3D* cam` <br> `const QRectF& plotArea` <br> `QVector<QChartPrimitive>& out` <br> `QVector<QChartTextLabel>* labels=nullptr` | public | — | Renderer（QPainter 3D 子路径/GL collectScene） | `QChartPrimitive` <br> `QChartCamera3D` <br> `QChartTextLabel` |
| `ProjectFn3D` | `makeProjectFn` | **闭包组装（★核心）**：`Data →[axisX/Y/Z::toNumeric]→ Numeric →[projection3D::toWorld]→ World →[camera3D.project]→ {screen,depth}`；public 供 Widget3D 悬停复用。 | `const QChartCamera3D* cam` <br> `const QRectF& plotArea` | public | `ProjectFn3D` | collectPrimitives/`QChartWidget3D::updateHover`（CPU 分支） | `ProjectFn3D` <br> `QChartCamera3D` |
| `QChartProjectedPoint` | `projectNumeric` | 私有：Numeric 点 → Screen（toWorld → camera3D->project）。 | `const QVector3D& num` <br> `cam, plotArea` | protected | `QChartProjectedPoint` | emitLine/collect | `QChartCamera3D` |
| `void` | `emitLine` | 私有：直线采样（Numeric 两点 → samplingSegmentsHint 段；**identity 快速通道免 toWorld**；每段 depth=段中点；任一端 NaN 跳过该段；dataIndex=-1）。 | `QVector3D numA, numB` <br> `Layer layer, color, penWidth` <br> `cam, plotArea, out` | protected | — | collectPrimitives（网格/刻度） | `QChartPrimitive` |
| `QVector<qreal>` | `dimTicks` | 私有：该维刻度值（axes3D 委托；axis null → 空）。 | `int dim` | protected | `QVector<qreal>` | collectPrimitives | `QChartAxes3D` |
| `void` | `hookSeriesDirty`/`unhookSeriesDirty` | 私有：series dataChanged → worldCache 置脏（连接/断开；series 零耦合红线不变——本层持有引用）。 | `QChartSeries3D* s` | protected | — | add/removeSeries3D | `QChartSeries3D` |

Notes:
- **gridFloor 并入历史**：gridFloorVisible/gridFloorHalfSize 已移除（§8.5 并入 Box 模式地板网格）——API 面不含旧字段。
- **三层分离**：本类做 toWorld/投影（3D 侧唯一投影点）；QChartAxes3D 只产 Numeric 几何；Renderer 只画（reviewer grep 验证点）。
- 系列所有权与信号：基类机制（m_series qDeleteAll + seriesAdded/Removed + 基类属性信号连线）。

## Overrided Qt Events:
None.（QObject）

## Signals:
None.（继承 QChartLayer 的 seriesAdded/seriesRemoved/gridChanged——无新增）
