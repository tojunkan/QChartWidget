# QChartRenderer Documentation

## Brief Introduction:
渲染器抽象接口 + 场景快照定义（D2 定案）。职责：把「场景快照」画到任意 `QPaintDevice`（QWidget/QImage/QPixmap/打印机等）；`render()` 只依赖快照 + 目标 device，**不反向依赖 QChartWidget**。接口分派：`render`（屏显，可缓存）/ `renderUncached`（导出专用，无缓存直绘，避免矢量 device 栅格化与污染屏显缓存，D13）。本头还定义 3D 图元 `QChartPrimitive`、billboard 标签 `QChartTextLabel`、场景快照 `QChartScene`、网格深度偏置 `kGridDepthBias=1e-3`（跨模块共享类型，见 architecture_overview §8）。Phase 3 的 `QOpenGLChartRenderer` 与本接口并列（统一后端 D26）。

## Constant Variables:

| Type | Name | Description | Available Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `constexpr qreal` | `kGridDepthBias` | 网格深度偏置（painter 版 polygon offset）：Grid 图元 depth −= 1e-3，保证同深度处系列优先（z-fighting 时系列赢；GL 路径 glPolygonOffset 等价语义，D29）。 | `1e-3` | `QChartPrimitive` <br> `QPainterChartRenderer` <br> `QOpenGLChartRenderer` |

Notes:
- 本类为纯抽象接口：无成员变量、无 Q_OBJECT（非 QObject）。

## Member Variables:
None.（抽象接口；具体状态在子类）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `~QChartRenderer` | 虚析构（default 实现，src/core/QChartRenderer.cpp）。 | — | public virtual | — | 子类析构 | — |
| `void` | `render` | 将场景渲染到目标设备（不得假设 device 是 QWidget；可读写缓存）。 | `const QChartScene& scene` <br> `QPaintDevice* device` | public pure virtual | — | `QChartWidget::paintEvent`（m_renderer->render(scene, this)） | `QChartScene` <br> `QPaintDevice` |
| `void` | `renderUncached` | 直接绘制到 device、不读写内部缓存（导出专用：PNG/SVG/PDF；避免矢量 device 被栅格化 + 不污染屏显缓存，D13）。 | `const QChartScene& scene` <br> `QPaintDevice* device` | public pure virtual | — | `QChartWidget::saveAsPng/Svg/Pdf` | `QChartScene` |
| `void` | `invalidateBackground` | 置脏背景缓存（下次 render 重建；无缓存后端可为空操作）。 | 无 | public pure virtual | — | `QChartWidget::invalidateBackground`（轴/网格变化链路） | — |
| `void` | `invalidateForeground` | 置脏前景缓存。 | 无 | public pure virtual | — | `QChartWidget::invalidateForeground`（系列/图例变化链路） | — |
| `void` | `setCachingEnabled` | 缓存开关。 | `bool enabled` | public pure virtual | — | `QChartWidget::setCachingEnabled` | — |
| `bool` | `isCachingEnabled` | 缓存状态访问器。 | 无 | public pure virtual | `true`/`false` | `QChartWidget::isCachingEnabled`/测试 | — |

Notes:
- **本头定义的共享类型**（非成员函数，供全库引用）：`QChartPrimitive{Type{Point,LineSegment}, Layer{Grid,Series,ForegroundDecor}, a/b/depth/dataIndex/markerSize/color/penWidth/worldA/worldB}`（depth=−viewZ 越大越远，降序绘制；worldA/B 为 Phase 3 GL 的 VBO 顶点源）；`QChartTextLabel{screenPos,text,anchor,fontSize,color,isTitle}`；`QChartScene{plotArea,dataBounds,viewRect,projection,axes,layers,backgroundColor,legend,legendItems,exportMode,camera3D,layers3D,worldBounds}`（`is3D() = camera3D!=nullptr`；exportMode 导出跳过调试黄框）。
- 场景组装方 = QChartWidget::buildScreenScene/buildExportScene（3D 子类重写注入 3D 段）。

## Overrided Qt Events:
None.（非 QWidget）

## Signals:
None.（非 QObject）

Notes:
- 接口契约：实现方（QPainterChartRenderer/QOpenGLChartRenderer）必须同时实现 render 与 renderUncached（GL 后端 renderUncached 拒绝导出并 qWarning，A4：导出一律走 QPainter 后端）。
