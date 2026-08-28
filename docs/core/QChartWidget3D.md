# QChartWidget3D Documentation

## Brief Introduction:
3D 图表控件（D17 定案：`QChartWidget3D : QChartWidget` 子类）。3D 特定 API 与交互（orbit/dolly/panViewCube + 悬停命中 + 联动信号）全部隔离在本类；基类仅两处最小改动（`buildScreenScene`/`buildExportScene` 虚化钩子，2D 类行为零变化）。构造时序 ⚠：先 `setProjection(默认 QCartesianProjection)` 满足基类流程（addLayer 接线/布局/2D 相机初始化），该 2D projection 仅占位——渲染走 camera3D/layers3D 的 3D 段。Phase 3 起内置 **GlHost**（内嵌 QOpenGLWidget）+ 统一后端开关（D26：同时决定渲染与拾取；QCHART_GL=0 兜底）。

## Constant Variables:
None.（`RenderBackend{OpenGL, QPainter}` 为类型级枚举）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `std::unique_ptr<QChartCamera3D>` | `m_camera3D` | 3D 相机（构造内置默认，`setCamera3D` 可替换；非持有暴露 camera3D()）。 | `std::unique_ptr<QChartCamera3D>` | 构造创建 | `QChartCamera3D` |
| `std::unique_ptr<QChartProjection3D>` | `m_projection3D` | 3D 投影（`setProjection3D` 设置；必须设置）。 | `std::unique_ptr<QChartProjection3D>`（笛卡尔/柱/球/函数） | `nullptr` | `QChartProjection3D` 家族 |
| `QList<QChartLayer3D*>` | `m_layers3D` | 3D 图层登记列表（基类 m_layers 之外的类型化登记；`addLayer3D` 管理）。 | `QList<QChartLayer3D*>` | 空 | `QChartLayer3D` |
| `QChartWorldBox` | `m_worldBounds` | 世界包围盒缓存（fitWorld 链：resolveDataBox→computeWorldBounds 产物）。 | `QChartWorldBox` | 默认 | `QChartWorldBox` |
| `class GlHost*` | `m_glHost` | Phase 3 GL 宿主（内嵌 QOpenGLWidget，cpp 定义；GL 就绪才显示，否则隐藏回退纯 QPainter）。 | `std::unique_ptr<GlHost>` | 构造创建 | `QOpenGLChartRenderer` <br> `QOpenGLWidget` |
| `QOpenGLChartRenderer*` | `m_glRenderer` | GL 渲染器指针（GlHost 生命周期内，非持有）。 | `QOpenGLChartRenderer*` | GlHost 构造挂接 | `QOpenGLChartRenderer` |
| `RenderBackend` | `m_renderBackend` | 渲染后端开关（A9：GL 默认、QPainter 保底；环境变量 QCHART_GL=0 压制）。 | `OpenGL` <br> `QPainter` | `OpenGL` | — |
| `QVector3D` | `m_dataBounds3DMin/Max` | 视图→dataBounds 反算缓存（Numeric 范围；`recomputeDataBounds3D` 维护）。 | `QVector3D` | `{0,0,0}` | — |
| `bool` | `m_dataBounds3DValid` | 反算缓存有效性（false = A9 域盒兜底生效）。 | `true` <br> `false` | `false` | — |
| `std::pair<QVector3D,QVector3D>` | `m_anchorBox` | A9 锚定域盒（dataBounds3D 无效时轴/网格锚定此盒，静态）。 | `pair<QVector3D,QVector3D>` | `{(0,0,0),(0,0,0)}` | `QChartLayer3D` |
| `std::optional<QVector3D>` | `m_domainMin/Max` | 显式域盒（A3 优先级最高；`setDomainBox` 设置）。 | `std::optional<QVector3D>` | `nullopt` | — |
| `bool` | `m_orbitDrag` | 是否正在 orbit 拖拽（R6：仅 orbit，无平移手势）。 | `true` <br> `false` | `false` | — |
| `QPointF` | `m_pressPos` | 左键按下位置（点击 vs 拖拽判定）。 | `QPointF` | `QPointF()` | — |
| `QPointF` | `m_lastPos` | 上次鼠标位置（orbit 增量计算）。 | `QPointF` | `QPointF()` | — |
| `qreal` | `m_orbitSensitivity` | orbit 灵敏度（度/像素）。 | `qreal` | `0.3` | — |
| `qreal` | `m_dollySensitivity` | dolly 灵敏度（k：factor=exp(−notch·k)）。 | `qreal` | `0.1` | — |
| `bool` | `m_hoverActive` | 悬停激活状态。 | `true` <br> `false` | `false` | — |
| `QPointF` | `m_lastHoverUV` | 上次悬停 (u,v)（Numeric 空间；联动信号去重）。 | `QPointF` | `QPointF()` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartWidget3D` | 构造函数：基类流程 + 默认相机/投影 + GlHost 挂接 + 相机 viewChanged 连线。 | `QWidget* parent` | public | — | 用户/demo | `QChartWidget` |
| — | `~QChartWidget3D` | 析构（GlHost 需完整类型 → cpp 定义）。 | — | public | — | — | `GlHost` |
| `void` | `setRenderBackend` | 设置渲染后端（§2.2 统一后端：同决渲染与拾取；QCHART_GL=0 压制）。 | `RenderBackend b` | public | — | demo（setRenderBackend(OpenGL)）/用户 | — |
| `RenderBackend` | `renderBackend` | 后端访问器（内联）。 | 无 | public | `OpenGL`/`QPainter` | 拾取分支（updateHover） | — |
| `QChartCamera3D*` | `camera3D` | 3D 相机访问器（内联，非持有）。 | 无 | public | — | demo/测试/渲染 | `QChartCamera3D` |
| `void` | `setCamera3D` | 替换 3D 相机（重连 viewChanged）。 | `std::unique_ptr<QChartCamera3D> cam` | public | — | 用户 | `QChartCamera3D` |
| `void` | `setProjection3D` | 设置 3D 投影；自动按 defaultDataBounds fit 相机（fitWorld）。 | `std::unique_ptr<QChartProjection3D> proj` | public | — | 用户/demo（构造/切换） | `QChartProjection3D` |
| `const QChartProjection3D*` | `projection3D` | 3D 投影访问器（内联）。 | 无 | public | — | 测试/渲染 | `QChartProjection3D` |
| `void` | `addLayer3D` | 添加 3D 图层（内部走基类 addLayer 复用图例/主题/调色板 + 登记 m_layers3D）。 | `QChartLayer3D* g` | public | — | 用户/demo | `QChartLayer3D` |
| `QList<QChartLayer3D*>` | `layers3D` | 3D 图层列表访问器（内联）。 | 无 | public | — | 渲染/测试 | `QChartLayer3D` |
| `void` | `setDomainBox` | 显式域盒（A3 优先级最高）→ fitWorld。 | `const QVector3D& dataMin` <br> `const QVector3D& dataMax` | public | — | 用户 | — |
| `void` | `clearDomainBox` | 清除域盒（回退 数据包围盒 > defaultDataBounds）→ fitWorld。 | 无 | public | — | 用户 | — |
| `bool` | `hasDomainBox` | 域盒存在访问器。 | 无 | public | `true`/`false` | 测试 | — |
| `QVector3D` | `dataBounds3DMin/Max` | 反算缓存访问器（内联）。 | 无 | public | — | 测试/渲染 | — |
| `bool` | `dataBounds3DValid` | 反算有效性访问器（内联）。 | 无 | public | `true`/`false` | 测试/轴盒推送 | — |
| `QPointF` | `worldToPixel` | World→屏幕点（camera3D->project(...).screen）。 | `const QVector3D& w` | public | — | 测试/联动定位 | `QChartCamera3D` |
| `void` | `fitWorld` | **A3 全链**：resolveDataBox → computeWorldBounds → setViewCubeToFit → 反算 → 推轴盒 → 重绘。 | 无 | public | — | `setProjection3D`/`setDomainBox`/构造/用户 | `QChartProjection3D` <br> `QChartCamera3D` |
| `void` | `updateHover` | **3D 悬停（§8.3 修订）**：后端分支（GL→ID 帧拾取 / CPU→屏幕近邻）→ dataIndex → Data (u,v) → 发 uvHovered/uvHoveredEnd；不弹 tooltip（D-3D-13）。 | `const QPointF& pos` | private | — | `mouseMoveEvent` | `QChartHitTester` <br> `QOpenGLChartRenderer` |
| `void` | `layoutGlHost` | GlHost 几何跟随 plotArea（未就绪 → 全 widget）。 | 无 | private | — | `resizeEvent` | `GlHost` |
| `void` | `recomputeDataBounds3D` | viewCube 5³=125 点采样 → fromWorld → min/max（非有限跳过，全 NaN→Valid=false）；Cartesian3D 快速通道免采样。 | 无 | private | — | 相机 viewChanged 槽（×2） | `QChartProjection3D` <br> `QChartCamera3D` |
| `void` | `pushAxesDataBoxToLayers` | 更新各 layer3D 轴盒（dataBounds3D 有效用它，否则 A9 锚定域盒）。 | 无 | private | — | 相机 viewChanged 槽/`fitWorld` | `QChartLayer3D` |
| `std::pair<QVector3D,QVector3D>` | `computeSeriesDataBounds` | 数据包围盒：遍历 layers3D series3DList → points() → toNumeric×3 → min/max（一次性，非每帧）。 | 无 | private | — | `resolveDataBox` | `QChartSeries3D` |
| `std::pair<QVector3D,QVector3D>` | `resolveDataBox` | **A3 链**：显式域盒 > 数据包围盒 > defaultDataBounds。 | 无 | private | — | `fitWorld` | — |

Notes:
- 基类继承的全部 2D API（addLayer/addAxis/setViewRect/导出等）在 3D 语境语义不变（基类行为零变化红线）。
- 控制器职责（§2.2/§3）：反算/域盒链/轴盒推送全部在本类（"控制器"称谓），Layer3D 只消费。

## Overrided Qt Events:
| Name | Description | Parameters | Triggered By | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `mousePressEvent` | 左键：记录 m_pressPos/m_lastPos、置 m_orbitDrag；点击（无拖拽位移）→ 屏幕近邻命中 → 发 uvSelected(u,v)。 | `QMouseEvent*` | 用户鼠标左键按下 | `QChartHitTester` |
| `mouseMoveEvent` | orbit 拖拽：增量 yaw/pitch（m_orbitSensitivity 度/像素）→ camera3D->orbit；悬停：updateHover(pos)（GL/CPU 后端分支）。 | `QMouseEvent*` | 用户鼠标移动 | `QChartCamera3D` <br> `QChartHitTester` <br> `QOpenGLChartRenderer` |
| `mouseReleaseEvent` | 结束 orbit 拖拽。 | `QMouseEvent*` | 用户鼠标左键释放 | — |
| `wheelEvent` | dolly：factor=exp(−notch·m_dollySensitivity) → camera3D->dolly。 | `QWheelEvent*` | 用户滚轮 | `QChartCamera3D` |
| `leaveEvent` | 悬停结束：发 uvHoveredEnd。 | `QEvent*` | 鼠标离开 widget | — |
| `resizeEvent` | 基类布局逻辑保留 + `layoutGlHost()`（GlHost 跟随 plotArea）。 | `QResizeEvent*` | Qt 系统 resize | `GlHost` |
| `buildScreenScene`（protected virtual 覆写） | 填 3D 段：camera3D/投影3D/layers3D/worldBounds → QChartScene（2D 字段留默认）。 | — | `paintEvent`（基类） | `QChartScene` <br> `QChartLayer3D` |
| `buildExportScene`（protected virtual 覆写） | 导出场景填 3D 段（未验收，ROADMAP 遗留）。 | `scope, size, outDeviceSize` | `saveAs*`（基类） | `QChartScene` |

Notes:
- 交互链路（D-3D-4）：手势 → Camera 几何（orbit/dolly），Camera 不碰事件。
- 3D 悬停不弹 tooltip（D-3D-13 用户定案）；悬停/选中经 uvHovered/uvSelected 信号对外（D18 双 Widget 联动）。
- R6：无平移鼠标手势（m_orbitDrag 仅 orbit；panViewCube 仅 API/动画）。

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `uvHovered` | 悬停点 (u,v)（Numeric 空间；3D 侧 = 屏幕近邻/ID 帧命中）。 | `qreal u, qreal v` | NULL（内部未连线；双 Widget 联动/demo 按需连接，D18） | `QChartHitTester` <br> `QChartSeries3D` |
| `uvSelected` | 点击选中 (u,v)。 | `qreal u, qreal v` | NULL（联动/demo 按需连接） | `QChartHitTester` |
| `uvHoveredEnd` | 悬停结束。 | — | NULL（联动/demo 按需连接） | — |

Notes:
- D18 联动约定：两 Widget 同构数据副本 + 相同 Axis range → 同一 Numeric 空间；uvHovered/uvSelected **单向传值**，高亮标记只收不发防回环。
- 发射点：`updateHover`（uvHovered/uvHoveredEnd）、`mousePressEvent` 点击命中（uvSelected）。
