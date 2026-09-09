# QChartAbstractWidget Documentation

## Brief Introduction:
QChartAbstractWidget 是图表**纯容器抽象基类**（QWidget，Q_OBJECT；批次 A 容器化首改的目标形态）：不含相机、不含 buildScene、不含图例/导出/拾取/动画（旧成员/方法注释保留 + "待 <阶段> 恢复"标注，不编译引用）。职责：① 渲染后端选择（RenderBackend：QPainter/OpenGL——GL 后端创建 plotArea 对齐的 QOpenGLWidget 子控件 GlPlotWidget，CPU 后端不建）；② 图层容器（layers 非持有列表）；③ 布局/plotArea（relayout → 纯虚 calculatePlotArea 由子类算边距扣除，广播 plotAreaChanged）；④ 渲染编排（renderLayers/renderLayersGL/pushContextToLayers：每 layer collectPrimitives → renderer.render；GL 标签经透明 QImage 中间层合成）；⑤ 事件分发（paint/resize/mouse*/wheel 全部 final，转发到空虚拟钩子 onMouse*/onWheel——交互待交互阶段）。相机归 layer（2D/3D 各自层值成员）。GL 子控件交互事件置 WA_TransparentForMouseEvents，由外层统一分发。

## Constant Variables:
None.（嵌套枚举 `RenderBackend{QPainter, OpenGL}` 为类型定义非类常量）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QList<QChartLayer*>` | `m_layers` | （protected）图层列表（**非持有**，调用方保证生命周期） | `QList<QChartLayer*>` | 空 | `QChartLayer` |
| `QRectF` | `m_plotArea` | （protected）像素绘制区（relayout 计算并广播） | `QRectF` | 空 | — |
| `bool` | `m_layoutDirty` | （protected）布局脏标记（resize/margins/层变更置位，paintEvent 消费） | `true`/`false` | `true` | — |
| `qreal` | `m_marginLeft/Top/Right/Bottom` | （protected）四边距（setMargins 修改；边框轴 sizeHint 外边距基值） | `qreal` | 各 `20.0` | — |
| `std::unique_ptr<QPainterChartRenderer>` | `m_cpuRenderer` | （protected）CPU 渲染器（构造即建） | — | 构造创建 | `QPainterChartRenderer` |
| `std::unique_ptr<QOpenGLChartRenderer>` | `m_glRenderer` | （protected）GL 渲染器（OpenGL 后端激活时创建） | — | `nullptr` | `QOpenGLChartRenderer` |
| `std::unique_ptr<GlPlotWidget>` | `m_glHost` | （protected）plotArea 对齐 GL 子控件（OpenGL 后端激活时创建；定义于 .cpp） | — | `nullptr` | `QOpenGLWidget` |
| `QWidget*` | `m_glHostWidgetRaw` | （protected）宿主裸指针（测试取证；随宿主创建/销毁维护） | 指针/`nullptr` | `nullptr` | — |
| `RenderBackend` | `m_renderBackend` | （protected）当前渲染后端 | QPainter/OpenGL | `RenderBackend::QPainter` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartAbstractWidget` | 构造：setMouseTracking(true)、最小尺寸 200×150、建 m_cpuRenderer | `QWidget* parent=nullptr` | public | — | 子类构造 | — |
| — | `~QChartAbstractWidget` | 析构：GL 上下文仍存活时 makeCurrent + clearBatches | 无 | public | — | — | `QOpenGLChartRenderer` |
| `void` | `setRenderBackend` | 切渲染后端：GL→惰性建 m_glRenderer+m_glHost（m_glHostWidgetRaw 同步、置几何并 show）；CPU→makeCurrent+clearBatches 后销毁两者；scheduleRepaint | `RenderBackend backend` | public | — | demo（applyBackend）、GL 测试 | — |
| `RenderBackend` | `renderBackend` | 后端访问器（内联） | 无 | public | QPainter/OpenGL | 测试 | — |
| `QWidget*` | `glHostWidget` | GL 宿主控件访问器（内联；OpenGL 激活时非空；测试/宿主 FBO 取证） | 无 | public | 指针/`nullptr` | TestWidgetGl/TestWidget3DGl/TestAxes3DGl（grabFramebuffer/几何/FBO 断言）、demo shot | — |
| `void` | `addLayer` | 加层（去重；置布局脏 + scheduleRepaint） | `QChartLayer* layer` | public | — | QChartWidget/QChartWidget3D 便捷、用户/demo | `QChartLayer` |
| `void` | `removeLayer` | 移层 | `QChartLayer* layer` | public | — | 用户 | `QChartLayer` |
| `void` | `clearLayers` | 清层 | 无 | public | — | 用户 | — |
| `QList<QChartLayer*>` | `layers` | 层列表访问器（内联） | 无 | public | — | 子类（viewRect 遍历等） | — |
| `void` | `relayout` | calculatePlotArea → 写 m_plotArea（变化时广播 plotAreaChanged）+ layoutGlHost | 无 | public | — | layoutAxes（QChartWidget）、测试 | — |
| `QRectF` | `plotArea` | 绘制区访问器（内联） | 无 | public | `QRectF` | 测试断言、坐标转换 | — |
| `void` | `setMargins` | 设置四边距 → 布局脏 + scheduleRepaint | `qreal left, top, right, bottom` | public | — | 用户/测试 | — |
| `qreal` | `marginLeft/Top/Right/Bottom` | 各边距访问器（内联） | 无 | public | `qreal` | 测试（外带采样） | — |
| `virtual QRectF` | `calculatePlotArea` | **纯虚**（protected）：按 margins 与边框轴 sizeHint 外边距占用计算绘制区 | 无 | protected | `QRectF` | relayout 内部 | `QChartAxis` |
| `virtual void` | `drawExternalContent` | 画 plotArea 外内容（边框轴 drawAtEdge/标题；GPU 模式 plotArea 由 GL 子控件覆盖）；基类空 | `QPainter& painter` | protected | — | paintEvent | — |
| `virtual void` | `onBeforePaint` | 渲染前钩子（子类同步 viewRect/dataBounds 驱动链）；默认空 | 无 | protected | — | paintEvent | — |
| `virtual const QChartAbstractProjection*` | `projection` | 容器当前投影（默认 nullptr；pushContextToLayers 用） | 无 | protected | 指针 | pushContextToLayers | — |
| `virtual QColor` | `sceneBackgroundColor` | 场景背景色（默认白；主题阶段前子类/调用方覆盖） | 无 | protected | `QColor` | pushContextToLayers/paintEvent | — |
| `virtual void` | `onMousePress/onMouseMove/onMouseRelease` | 事件分发钩子（空；交互待交互阶段） | `QMouseEvent* e` | protected | — | 事件 final 转发 | — |
| `virtual void` | `onWheel` | 滚轮钩子（空） | `QWheelEvent* e` | protected | — | wheelEvent | — |
| `virtual void` | `renderLayers` | CPU 渲染编排：pushContextToLayers → 逐层 collectPrimitives + invalidateView + render(layer->scene(), device) | `QPaintDevice* device` | protected | — | paintEvent（CPU 路径与兜底） | `QChartLayer` |
| `virtual void` | `renderLayersGL` | GL 渲染编排：pushContextToLayers → 建 plotArea 尺寸**设备分辨率**透明 QImage（setDevicePixelRatio(dpr)）→ 逐层 collect + invalidateView + `m_glRenderer->render(scene, &labelDev)`（图元→当前 GL FBO、标签→QImage）→ QPainter SourceOver drawImage 合成上屏 | `QPaintDevice* device` | protected | — | GlPlotWidget::paintGL（经 owner->renderLayersGL(this)） | `QOpenGLChartRenderer` |
| `virtual void` | `pushContextToLayers` | 同步 plotArea/投影/背景到各层场景（渲染前调用，幂等） | 无 | protected | — | renderLayers/renderLayersGL/paintEvent | `QChartLayer` |
| `void` | `scheduleRepaint` | 请求重绘：GL 后端 = 宿主 update；否则自身 update | 无 | protected | — | 各变更入口 | — |

Notes:
- **事件均为 final 覆写**（protected 段声明后 private 实现）：paintEvent/resizeEvent/mousePressEvent/mouseMoveEvent/mouseReleaseEvent/wheelEvent——子类不可再覆写（交互行为经钩子扩展）。
- paintEvent 语义：布局脏→relayout；onBeforePaint；CPU 后端：填背景→push+renderLayers→drawExternalContent；GPU 后端（宿主可见）：外层只填背景+push+drawExternalContent（plotArea 内由宿主 paintGL）；宿主未就绪兜底退化 CPU 全量。
- GlPlotWidget（.cpp 实现细节）：plotArea 对齐；paintGL 视口=设备像素 `qRound(w×dpr)`（t13 HiDPI 修复）、清白底、`qchartSetGLPixelRatio(dpr)`（t14）、owner->renderLayersGL(this)；WA_TransparentForMouseEvents；hide 至 OpenGL 激活。
- 旧 API 注释保留（恢复点）：`invalidateBackground()/invalidateForeground()`（语义已被 renderer viewDirty/layer dataDirty 取代）、`saveAsPng/saveAsSvg/saveAsPdf`（导出阶段）、`legend()`（图例阶段）、`buildScene()`（由「layer 快照 + renderer 管线」取代，见 QChartLayer::scene/snapshotScene 语义）。

## Overrided Qt Events:

| Event | Override | Behavior |
| :--- | :--- | :--- |
| `paintEvent` | final | 布局/渲染编排（CPU 全量 / GPU 外带 + 宿主内区） |
| `resizeEvent` | final | 置布局脏 + scheduleRepaint |
| `mousePressEvent/mouseMoveEvent/mouseReleaseEvent` | final | 转发 `onMousePress/onMouseMove/onMouseRelease` 钩子 |
| `wheelEvent` | final | 转发 `onWheel` 钩子 |

## Signals:

| Name | Description | Parameters | Emitted By | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `plotAreaChanged` | plotArea 重算广播 | `const QRectF& newPlotArea` | `relayout`（值变化时） | — |
| `projectionChanged` | 容器投影变更广播 | `const QChartAbstractProjection* newProjection` | 子类 setProjection（QChartWidget） | `QChartAbstractProjection` |
