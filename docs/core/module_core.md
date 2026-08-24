# module_core.md —— core 模块

> 属于 t53 文档套件；配套 deepdive：`docs/core/deepdive_viewRect.md`（viewRect→plotArea 与 viewCube 派生）、`docs/core/deepdive_picking_idframe.md`（GPU 拾取）。

## 1. 职责与边界

core = **宿主对象 + 相机家族 + 渲染管线 + 命中引擎 + 主题/图例**。是全库最重、被依赖最多的模块：

- 依赖：`projection`（QChartProjection/QChartProjection3D）、`layers`（QChartLayer/Layer3D）、`utils`（QChartMath/QChartDebug/QDataPoint）。
- 被依赖：`axes`（QChartAxis include QChartCamera）、`series`（QChartSeries3D include QChartCamera3D/QChartRenderer）、`layers`（QChartLayer3D include QChartRenderer）、`animation`（.cpp include QChartWidget）。
- 与 layers 存在头级小环（`QChartWidget.h` ↔ `QChartLayer.h`），`#pragma once` 下安全；逻辑层次上 core 是宿主层。

红线语义：**渲染器只依赖场景快照 + 目标 device**（`buildScreenScene()` 快照 + `render(scene, device)`），不反向依赖 Widget（D2/D13）；**拾取不进 renderer**（D27，归 QChartHitTester）。

## 2. 文件与类清单

| 文件（include/core/ + src/core/） | 类 | Q_OBJECT | 信号 | Q_PROPERTY |
|---|---|---|---|---|
| QChartWidget.h/.cpp | `QChartWidget` | ✓ | seriesHovered / viewChanged | panEnabled, zoomEnabled 等 3 |
| QChartWidget3D.h/.cpp | `QChartWidget3D : QChartWidget` | ✓ | uvHovered / uvSelected / uvHoveredEnd | — |
| QChartCamera.h/.cpp | `QChartCamera`（基类）+ `QChartCamera2D` | ✓✓ | viewChanged | viewRect / center / zoom |
| QChartCamera3D.h/.cpp | `QChartCamera3D : QChartCamera` | ✓ | viewChanged（继承） | viewCubeCenter/Size, yaw, pitch, fovY |
| QChartRenderer.h/.cpp | `QChartRenderer` + `QChartScene` + `QChartPrimitive` + `kGridDepthBias` | — | — | — |
| QPainterChartRenderer.h/.cpp | `QPainterChartRenderer : QChartRenderer` | — | — | — |
| QOpenGLChartRenderer.h/.cpp | `QOpenGLChartRenderer : QChartRenderer` | ✓ | — | — |
| QChartGL.h/.cpp | `QChartGL`（ShaderKind 程序池） | ✓ | — | — |
| QChartHitTester.h/.cpp | `QChartHitTester`（纯静态引擎） | — | — | — |
| QChartTheme.h/.cpp | `QChartTheme`（+ `QChartTheme::Preset`） | ✓ | — | — |
| QChartLegend.h/.cpp | `QChartLegend` | ✓ | visibleChanged / alignmentChanged / textColorChanged | visible, alignment 等 2 |

跨模块共享类型（定义于此）：`QChartScene`、`QChartPrimitive{Layer,Type,depth,dataIndex,worldA/worldB}`、`kGridDepthBias=1e-3`（QChartRenderer.h）；`QChartProjectedPoint{screen,depth,world}`（QChartCamera3D.h）。

## 3. 公共 API 一览

**QChartWidget**（2D 宿主）
- 视窗：`viewRect()/setViewRect()`、`plotArea()`、`panViewCartesian(dx,dy)`、`zoomViewCartesian(cx,cy,fx,fy)`、`dataBounds()` 语法糖（dim0/dim1）。
- 结构：`addLayer/addSeries/addAxis`、`setProjection`（唯一 Projection 持有者）、`setTheme`。
- 导出：`exportPng/Svg/Pdf(path, scope)`（走 `renderUncached`，D13）。
- 事件：paint/resize/mouse/wheel/leave 重写；`buildScreenScene()`/`buildExportScene()` 虚化钩子（D17 两处最小改动）。

**QChartWidget3D**（3D 宿主，D17 子类）
- `setProjection3D(unique_ptr<QChartProjection3D>)`（构造时序：先设默认 QCartesianProjection 满足基类流程，再切换）。
- 相机：`camera3D()`、`fitWorld()`（A3 全链：defaultDataBounds→computeWorldBounds→setViewCubeToFit）。
- 反算：`dataBoundsFromViewCube()`（5³=125 点采样，全 NaN→Valid=false；§2.2 快速通道）。
- 交互（D-3D-4：手势→Camera 几何；R6 平移无手势）：orbit（左键拖拽）/ dolly（滚轮）/ panViewCube（仅 API）。
- 联动（D18）：`uvHovered(u,v)/uvSelected(u,v)/uvHoveredEnd()` 信号；高亮标记只收不发防回环。

**QChartCamera2D**（viewRect 几何唯一实现）
- `setViewRect`（绝对设置，不做 fit）、`center()/setCenter`、`zoom()/setZoom`（zoom=宽度度量）。
- `fitViewRectToPlotArea(plotArea, strategy)`：Stretch/Fit/Crop/Fixed × KeepWidth/KeepHeight/KeepCenter。
- 静态纯函数 `cartesianToPixel(viewRect, plotArea, cx, cy)` / `pixelToCartesian(...)`（全推导见 deepdive_viewRect）。

**QChartCamera3D**（viewCube 主状态，R5/D21）
- 主状态：`viewCube()/setViewCube`、`viewCubeCenter/Size`、`yaw/pitch`（clamp ±89°）、`fovY`（(1,179]）。
- 派生只读：`position()/lookAt()/up()/nearPlane()/farPlane()`（公式见 deepdive_viewRect）。
- 矩阵：`viewMatrix()`、`projectionMatrix(aspect)`（透视/正交）、`viewProjectionMatrix(aspect)`（World→Clip 合并，D-3D-10 预留）。
- 几何：`orbit(dyaw,dpitch)`（viewCube 不动）、`dolly(factor)`（缩放盒）、`panViewCube(dx,dy)`（仅 API/动画）。
- 投影：`project(world, plotArea)` → `QChartProjectedPoint{screen,depth,world}`。

**渲染器家族**（统一后端，D2/D26）
- `QChartRenderer::render(scene, device)`（参数化 QPaintDevice）；`QPainterChartRenderer`（2D 双缓存 + 3D 子路径）；`QOpenGLChartRenderer`（GL 3.3 Core 主 pass + ID 帧拾取，细节见 deepdive_picking_idframe）。

**QChartHitTester**（D27 统一反向映射）
- `hitTest(pixel, seriesList, toPixel, ctx)`（2D：顶层可见优先 + 像素 in 多边形）。
- `hitTest(pixel, primitives, maxDistPx=8)`（3D：屏幕近邻，只扫 Series 层）。
- `hitTestGPU(r,g,b, pickTable)`（GPU 颜色解码，id=r|g<<8|b<<16，0xFFFFFF 哨兵→空）。
- `PickRecord{series, dataIndex, layer}`（与 GL 批次同步构建）。

**QChartTheme / QChartLegend**（D12 双槽颜色模型）
- 主题只当默认值；axis/layer/series/legend 显式 override 优先，`clear*()` 回退主题默认。
- Legend：`setVisible/setAlignment/setTextColor/clearTextColor` + 三个通知信号。

## 4. 信号槽表（谁连谁，QChartWidget 内建线）

| 发送方 | 信号 | 接收方 | 槽/λ 动作 |
|---|---|---|---|
| QChartLegend | visibleChanged / alignmentChanged / textColorChanged | QChartWidget | → `invalidateForeground()` |
| QChartLayer | seriesAdded(s) | QChartWidget | 入列 + 连 s 的 colorChanged/opacityChanged/visibleChanged/nameChanged → invalidateForeground；QXYSeries/QBarSeries 的 renderOverrideChanged → invalidateForeground |
| QChartLayer | seriesRemoved(s) | QChartWidget | 出列 + invalidateForeground |
| QChartAxis | rangeChanged(min,max) | QChartWidget | 重算 dataBounds → fit → invalidate（语法糖链路） |
| QChartAxis | visibleChanged / styleChanged / tickCountChanged | QChartWidget | → `invalidateBackground()` |
| QChartCamera（2D/3D） | viewChanged | QChartWidget / QChartWidget3D | 反算 dataBounds + 推轴盒 + 重绘（3D：§9 每帧不重算） |
| QChartWidget | seriesHovered(s,index,hover) | 外部（demo/联动） | 悬停通知 |
| QChartWidget3D | uvHovered(u,v) / uvSelected(u,v) / uvHoveredEnd() | 外部（双 Widget 联动，D18） | 单向传值 |

**moc 红线**：本模块所有 Q_OBJECT 类的 moc 由库 AUTOMOC 生成（消费方 OFF）；`QChartHitTester/QChartRenderer/QPainterChartRenderer` 无 Q_OBJECT 不产 moc。

## 5. 核心机制

1. **场景快照 + 参数化渲染**（D2/D13）：`paintEvent → buildScreenScene()（快照）→ renderer->render(scene, device)`；导出走同一入口（无缓存直绘，真矢量）。
2. **相机 = 纯映射器**（D1/D21）：2D 只做 viewRect 几何、不反算 dataBounds（归 Widget）；3D viewCube 主状态、位置类派生只读。
3. **统一后端**（D26）：后端开关同时决定渲染与拾取；GlHost（QOpenGLWidget 内嵌）GL 就绪才显示，否则隐藏回退纯 QPainter（§5.1 透明像素教训）。
4. **图元瞬态化**（D28）：collectPrimitives 输出瞬态缓存，VBO 上传后 clear+squeeze。
5. **双 Widget 联动防回环**（D18）：Series 单归属硬约束，不共享对象；同构副本 + 相同 Axis range → 同一 Numeric 空间；高亮标记只收不发。

## 6. 核心函数一览表（含文件位置）

| 函数 | 职责 | 调用方 | 文件 |
|---|---|---|---|
| `QChartCamera2D::cartesianToPixel(viewRect, plotArea, cx, cy)` | View Cartesian→Pixel 线性映射（纯函数） | DrawContext/Layer/Widget | src/core/QChartCamera.cpp |
| `QChartCamera2D::pixelToCartesian(viewRect, plotArea, p)` | Pixel→View 逆映射（纯函数） | Widget 交互/Layer hitTest | src/core/QChartCamera.cpp |
| `QChartCamera2D::fitViewRectToPlotArea(plotArea, strategy)` | fit 几何（KeepWidth/Height/Center，Fit/Crop） | Widget 布局/设 dataBounds | src/core/QChartCamera.cpp |
| `QChartCamera3D::viewProjectionMatrix(aspect)` | World→Clip 合并矩阵 | Layer3D 闭包/GL 渲染器 | src/core/QChartCamera3D.cpp |
| `QChartCamera3D::project(world, plotArea)` | World→{screen,depth,world} | Layer3D emitLine/collect | src/core/QChartCamera3D.cpp |
| `QChartCamera3D::orbit/dolly/panViewCube` | 交互几何（viewCube 状态运算） | QChartWidget3D 事件层 | src/core/QChartCamera3D.cpp |
| `QChartWidget::buildScreenScene()` | 组装 2D 场景快照（虚化钩子） | paintEvent | src/core/QChartWidget.cpp |
| `QChartWidget::exportPng/Svg/Pdf` | 导出（renderUncached，D13） | 用户/demo | src/core/QChartWidget.cpp |
| `QChartWidget3D::fitWorld()` | A3 全链 fit（defaultDataBounds→bounds→viewCube） | setProjection3D/构造 | src/core/QChartWidget3D.cpp |
| `QChartWidget3D::dataBoundsFromViewCube()` | viewCube→dataBounds 5³ 采样反算（快速通道免采样） | 视图变化槽 | src/core/QChartWidget3D.cpp |
| `QChartRenderer::render(scene, device)` | 参数化渲染入口（统一后端） | Widget paint/export | src/core/QChartRenderer.cpp |
| `QPainterChartRenderer::render` | QPainter 后端（2D 双缓存 + 3D 子路径） | QChartRenderer 分派 | src/core/QPainterChartRenderer.cpp |
| `QOpenGLChartRenderer::paintGL` | GL 主 pass（不透明清屏 + 分层 + ID 帧） | QOpenGLWidget | src/core/QOpenGLChartRenderer.cpp |
| `QChartGL::program(ShaderKind)` | 程序池（引用计数/惰性编译） | GL 渲染器 | src/core/QChartGL.cpp |
| `QChartHitTester::hitTest`（2D/3D 重载） | 统一反向映射（CPU） | Widget 悬停/点击 | src/core/QChartHitTester.cpp |
| `QChartHitTester::hitTestGPU(r,g,b,table)` | ID 解码（纯函数，可单测） | GL 拾取槽 | src/core/QChartHitTester.cpp |

## 7. 设计文档对应

- 五空间/相机职责：`docs/design/design_notes.md`（§五空间模型、§viewRect 与 dataBounds、§Pan/Zoom）。
- 3D 相机/World/图元：`docs/design/design_3d.md`（§2 链路、§4 相机家族、§7 图元、§8 Widget 形态、§9 联动）。
- 统一后端/GL/拾取/内存：`docs/design/design_phase3.md`（§2 宿主与渲染器、§5 z-buffer 与 ID 帧、§7 上下文、§8 拾取、§9 缓存）。
- 决策：D1/D2/D11/D12/D13/D16/D17/D18/D21/D22/D26/D27/D28/D29/D30（速查见 overview §7）。
