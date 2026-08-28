# QOpenGLChartRenderer Documentation

## Brief Introduction:
GL 后端渲染器（design_phase3 §2.1；D25/D26 统一后端 GPU 侧，: QChartRenderer 与 QPainter 后端并列）。职责边界（A4）：**GL 只做 3D 屏显**；导出（PNG/SVG/PDF）与 2D 路径一律走 QPainterChartRenderer（本类 `renderUncached` → qWarning 拒绝）。生命周期（A3 惰性）：宿主 QOpenGLWidget 回调驱动——`initializeGL`（上下文可用性 + 格式核对 + shader 编译）、`paintGL`（buildBatches + 分层主 pass）、`resizeGL`（视口）。**非 Q_OBJECT**（无信号/槽）。核心机制：图元→VBO 批次（16B interleaved 顶点，A5）+ ID 帧拾取（A6/D27，解码归 QChartHitTester）+ 三闸限流（§5.3）。

## Constant Variables:
None.（`GLVertex` static_assert 16B、`kMaxVertsPerBatch=65536` 为 .cpp 内局部常量；`ShaderKind` 枚举在 QChartGL.h）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QWidget*` | `m_host` | 宿主 = 内嵌 QOpenGLWidget（qobject_cast 需完整类型；GL 函数/上下文/FBO 来源）。 | `QWidget*` | `nullptr` | `QOpenGLWidget` <br> `GlHost` |
| `QVector<QChartPrimitive>` | `m_cachedPrimitives` | 收集缓存（dirty 才重建，§3.2；uploadBatches 后 clear+squeeze 瞬态化，D28）。 | `QVector<QChartPrimitive>` | 空 | `QChartPrimitive` |
| `QVector<QChartTextLabel>` | `m_cachedLabels` | billboard 标签（§6 overlay 绘制源；dirty 时随 collectScene 重建）。 | `QVector<QChartTextLabel>` | 空 | `QChartTextLabel` |
| `BatchPool` | `m_batches` | 批次池：`GLBatch{vao,vbo,primitive,vertexCount,baseId,layer,depthTest,depthBias,ebo,pointSize,idSentinel}` + `pickTable` + `dirty`。 | `BatchPool` | dirty=true | `GLBatch` <br> `QChartHitTester::PickRecord` |
| `QSize` | `m_viewportSize` | 视口尺寸（resizeGL 更新）。 | `QSize` | 空 | — |
| `bool` | `m_glReady` | GL 就绪（上下文可用 + initializeGL 成功）——GlHost 显隐判定（§5.1 透明语义教训）。 | `true`/`false` | `false` | `GlHost` |
| `bool` | `m_initAttempted` | initializeGL 已尝试（防每帧重试刷 qWarning）。 | `true`/`false` | `false` | — |
| `QElapsedTimer` | `m_pickClock` | 拾取限流计时（第二道闸 16ms）。 | `QElapsedTimer` | — | — |
| `QPoint` | `m_lastPickPos` | 上次拾取位置（位移 <1px 闸）。 | `QPoint` | — | — |
| `qint64` | `m_lastPickMs` | 上次拾取时刻（首帧前=-1000000 远过去）。 | `qint64` | `-1000000` | — |
| `QRgb` | `m_lastPickResult` | 上次拾取结果（缓存；哨兵 0xFFFFFF=无命中）。 | `QRgb` | `qRgb(255,255,255)` | — |
| `bool` | `m_lastPickValid` | 上次拾取有效标记（限流保持用）。 | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QOpenGLChartRenderer` | 构造函数（宿主指针挂接）。 | `QWidget* host` | public | — | `GlHost`（QChartWidget3D） | `GlHost` |
| — | `~QOpenGLChartRenderer` | 析构（GL 资源清理）。 | — | public | — | — | — |
| `void` | `render` | ⚠ device 必须是宿主 QOpenGLWidget；否则 qWarning 返回（GL 无通用 QPaintDevice 目标）。 | `const QChartScene& scene` <br> `QPaintDevice* device` | public override | — | `QChartWidget3D::paintGL`（GlHost） | `QChartScene` |
| `void` | `renderUncached` | ⚠ GL 不参与导出：qWarning 提示改用 QPainterChartRenderer（A4）。 | `scene, device` | public override | — | （不应被调用；导出走 QPainter） | — |
| `void` | `invalidateBackground` | 场景/数据脏标记（触发 VBO 重建；**视图变化不触发**，A5）。 | 无 | public override | — | `QChartWidget3D`（轴/数据变化） | — |
| `void` | `invalidateForeground` | 同（GL 无 QPixmap 缓存；数据/样式变化 → VBO 重建）。 | 无 | public override | — | `QChartWidget3D`（系列变化） | — |
| `void` | `setCachingEnabled` | GL 无 QPixmap 缓存：no-op（接口一致保留）。 | `bool` | public override | — | `QChartWidget::setCachingEnabled` | — |
| `bool` | `isCachingEnabled` | 恒 true（GL 无缓存语义）。 | 无 | public override | `true` | 测试 | — |
| `void` | `initializeGL` | 首帧/首 show：上下文可用性检查 + 格式核对（3.3 Core+depth24）+ 编译 3 shader 程序（§4；失败一次性告警降级 A9）。 | 无 | public | — | 宿主 `initializeGL` 回调 | `QChartGL` |
| `void` | `paintGL` | 主渲染入口：buildBatches（dirty）→ 分层主 pass（不透明清屏 + Grid/Series/Decor，§5.1）。 | `const QChartScene& scene` | public | — | 宿主 `paintGL` 回调 | `QChartScene` |
| `void` | `resizeGL` | 视口随尺寸（深度缓冲由 QOpenGLWidget 默认 FBO 按尺寸重建，A3）。 | `int w, int h` | public | — | 宿主 `resizeGL` 回调 | — |
| `bool` | `isReady` | GL 就绪访问器（内联）——GlHost 显隐判定。 | 无 | public | `true`/`false` | `GlHost`/`updateHover`（后端分支） | `GlHost` |
| `const QVector<QChartHitTester::PickRecord>&` | `pickTable` | 拾取表访问器（与批次同步构建；updateHover 解码用）。 | 无 | public | — | `QChartWidget3D::updateHover`（GL 分支） | `QChartHitTester` |
| `const QVector<QChartTextLabel>&` | `labels` | billboard 标签访问器（GlHost paintGL 绘制 overlay）。 | 无 | public | — | `GlHost::paintGL`（QPainter overlay） | `QChartTextLabel` |
| `QRgb` | `pickIdAt` | **GPU 拾取**：ID 帧自包含（清色哨兵白+清/写 depth）+ 三闸限流（①事件合并②≥1px+≥16ms③m_glReady）→ 1×1 readback → RGB24（全流程见 deepdive_picking_idframe）。 | `const QPoint& pos` <br> `const QChartScene& scene` | public | `QRgb`（哨兵 0xFFFFFF=无命中） | `QChartWidget3D::updateHover`（GL 分支） | `QChartHitTester` |
| `void` | `buildBatches` | 图元列表 → VBO 批次（数据/样式变化才重建）：collectScene + uploadBatches。 | `const QChartScene& scene` | private | — | `paintGL`/`pickIdAt`（dirty 时） | `QChartScene` |
| `void` | `collectScene` | 收集图元 + 构建 pickTable（纯 CPU 无 GL 依赖；对齐校验 Q_ASSERT pickTable 增量==图元增量）。 | `const QChartScene& scene` | private | — | `buildBatches` | `QChartLayer3D` <br> `QChartSeries3D` |
| `void` | `uploadBatches` | 同 (Layer,primitive,pointSize) 合并、64K 分片 → 16B 顶点打包 → glBufferData 上传（**需上下文 current**，t46 教训）。 | 无 | private | — | `buildBatches` | `GLVertex` <br> `GLBatch` |
| `void` | `drawPass` | 主 pass / ID pass 共用批次遍历（分层 Grid→Series→Decor；u_depthBias/u_baseId/u_sentinel uniform）。 | `const QChartScene& scene` <br> `ShaderKind kind` | private | — | `paintGL`/`pickIdAt`（ID 帧） | `QChartGL` <br> `ShaderKind` |
| `QOpenGLFunctions_3_3_Core*` | `glFuncs` | 当前上下文 GL 函数集（3.3 Core；null 安全）。 | 无 | private | — | 全部 GL 调用 | `QOpenGLFunctions_3_3_Core` |

Notes:
- friend `TestQOpenGLRenderer`：单测直查批次结构（图元→顶点数/合并/baseId，§13.2）。
- 惰性语义（A3）：m_glReady 前 paintGL 空绘制（GlHost 隐藏回退纯 QPainter）；拾取第三道闸返回哨兵。
- 视图变化（相机）不触发 VBO 重建——顶点是 World 坐标、仅 u_viewProj uniform 变化（D30 GPU 投影路径）。

## Overrided Qt Events:
None.（非 QWidget；生命周期由宿主回调驱动，非事件覆写）

## Signals:
None.（非 QObject）
