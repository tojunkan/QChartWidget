# QChartRenderer Documentation

## Brief Introduction:
QChartRenderer 是**渲染器抽象基类**（非 QObject 的纯多态类），职责：把「场景快照 QChartScene」画到任意 QPaintDevice（QWidget/QImage/QPixmap/打印机）——`render()` 只依赖快照 + 目标 device，不反向依赖 QChartWidget。编排**四步流水线**：步骤 1（收集，由 Widget/Layer/测试在调用前完成，scene 已填 Numeric 数据）→ `render()` 内若 `m_viewDirty` 则执行 步骤 2a `transformNumericToCartesian`（纯虚）+ 2b `cullAndResolveLabels`（纯虚）并清脏 → 步骤 3 `drawPrimitives`（纯虚，按 m_visibilityCache）→ 步骤 4 `drawLabels`（纯虚）。共享保护级：静态 `drawLabel`（文字排版/避让/钳制统一实现）、`onRenderBegin/End` 可选钩子（默认空）。**dataDirty 判定归 Widget/buildScene（m_dataDirty 已注释移除）；viewDirty 由本类持有**（`invalidateView()` 置脏；数据变化→重建 scene 时 Renderer 需被通知或场景对象换代）。S0 派生：QPainterChartRenderer、QOpenGLChartRenderer。

## Constant Variables:
None.（头内 `kGridDepthBias` 等 3D 设计注释块已整段注释，非成员）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `bool` | `m_viewDirty` | （protected）视图脏标记：true → 下次 render 重算变换+裁剪 | `true`/`false` | `true` | — |
| `QVector<bool>` | `m_visibilityCache` | （protected）与 scene.primitives 一一对应的可见性缓存（步骤 2b 填充、步骤 3 消费） | `QVector<bool>` | 空 | `QChartScene` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `~QChartRenderer` | 虚析构（default） | 无 | public | — | — | — |
| `void` | `render` | 主渲染入口：device/camera/projection 任一空则直接返回；onRenderBegin → （m_viewDirty 时 transformNumericToCartesian + cullAndResolveLabels + 清脏）→ drawPrimitives(scene, device, m_visibilityCache) → drawLabels → onRenderEnd | `QChartScene& scene, QPaintDevice* device` | public | — | 测试 `renderer.render(f.scene, &img)`、Widget 阶段 paint | `QChartScene` |
| `void` | `invalidateView` | 视图变化（Camera/窗口 resize）→ 下次 render 重算变换与裁剪（内联置 m_viewDirty） | 无 | public | — | Widget/Camera viewChanged 链（S0 测试单帧未调用） | — |
| `virtual void` | `transformNumericToCartesian` | **纯虚**（protected）：Numeric → Cartesian 变换（CPU 做；GPU 跳过——shader 内完成） | `QChartScene& scene` | protected | — | `render` 内部 | — |
| `virtual void` | `cullAndResolveLabels` | **纯虚**（protected）：裁剪 + 标签解析（CPU 精确裁；GPU 粗裁/全可见） | `QChartScene& scene` | protected | — | `render` 内部 | — |
| `virtual void` | `drawPrimitives` | **纯虚**（protected）：绘制所有可见图元（visibility[i]==true） | `QChartScene& scene, QPaintDevice* device, const QVector<bool>& visibility` | protected | — | `render` 内部 | — |
| `virtual void` | `drawLabels` | **纯虚**（protected）：绘制所有可见标签（label.visible==true）。基类 .cpp 提供一份**纯虚定义**（camera->project + drawLabel 排版，含 plotArea 外剔除）——S0 子类各自实现覆盖，基类版本未被虚调用（限定调用保留为共享参考） | `QChartScene& scene, QPaintDevice* device` | protected | — | — | — |
| `static void` | `drawLabel` | 共享文字排版：空文本返回；设字号/笔色；测宽高+pad=3；AlignCenter → 选锚点四周最远侧放文字（四方避让），边对齐按位；越界钳制回 plotArea（过度挤压则居中）；`drawText(rect, AlignCenter, text)` | `QPainter& painter, const QRectF& plotArea, const QPointF& pixelAnchor, const QString& text, const QColor& color, qreal fontSize, Qt::Alignment alignment` | protected | — | `QPainterChartRenderer::drawLabels2D`、`QOpenGLChartRenderer::drawLabels` | — |
| `virtual void` | `onRenderBegin` | 可选钩子：渲染开始（默认空，Q_UNUSED） | `QPaintDevice* device` | protected | — | `render` 内部 | — |
| `virtual void` | `onRenderEnd` | 可选钩子：渲染结束（默认空） | `QPaintDevice* device` | protected | — | `render` 内部 | — |

Notes:
- 渲染入口前置条件实测：`render` 需要 scene.camera 与 scene.projection 均非空（S0 测试夹具两者齐备后才出像素）。
- m_viewDirty 默认 true：首帧 render 必执行步骤 2。
- 基类 drawLabels 的纯虚定义（QChartRenderer.cpp:110）与 CPU/GL 子类实现同构（project+clip+drawLabel），S0 实际执行路径为子类覆写。

## Overrided Qt Events:
无（非 QObject、非 QWidget，无事件覆写）。

## Signals:
None.（非 QObject，无信号）
