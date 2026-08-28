# render_flow.md —— 渲染分派：render / renderUncached（含导出路径）

> t55 核心函数 flow · core 模块（QChartRenderer 接口 + QPainterChartRenderer/QOpenGLChartRenderer 实现）

## 控制流（调用图）

```
屏显路径：
  QChartWidget::paintEvent
    └─ m_renderer->render(scene, this)
         ├─ QPainterChartRenderer::render   # CPU 后端（缓存/直绘）
         └─ QOpenGLChartRenderer::render    # GL 后端：device==宿主校验（GL 实际由 GlHost::paintGL 驱动）

导出路径（D13）：
  QChartWidget::saveAsPng/Svg/Pdf(path, [scope,] size)
    └─ buildExportScene(scope, size, outDeviceSize)   # 计算设备尺寸 + plotArea（3D 注入 3D 段）
    └─ m_renderer->renderUncached(scene, device)
         ├─ QPainterChartRenderer::renderUncached → drawDirect（无缓存直绘，真矢量）
         └─ QOpenGLChartRenderer::renderUncached → qWarning 拒绝（A4：导出一律走 QPainter）
```

## 数据流（入参/出参/状态变更）

### QPainterChartRenderer::render（缓存路径，src/core/QPainterChartRenderer.cpp）

| 步骤 | 入参/状态 | 出参/状态变更 |
|---|---|---|
| 1 | `scene` + `device`（QPaintDevice 任意：QWidget/QImage/…） | QPainter p(device)，Antialiasing on |
| 2 | `m_cachingEnabled==false`？ | → `drawDirect`（无缓存直绘）并返回 |
| 3 | `dpr = device->devicePixelRatioF()`；`devicePixels = device 逻辑尺寸 × dpr`（QWidget 逻辑尺寸 / QImage 像素尺寸统一换算） | — |
| 4 | `m_bgDirty \|\| m_bgCache.isNull() \|\| 尺寸≠devicePixels \|\| dpr≠缓存 dpr`？ | 重建背景缓存：QPixmap(devicePixels) + setDevicePixelRatio(dpr) + 透明填充 + drawBackground → m_bgDirty=false |
| 5 | — | `p.drawPixmap(0,0, m_bgCache)` blit |
| 6 | `m_fgDirty \|\| ...`（同 4 判据） | 重建前景缓存：drawForeground → m_fgDirty=false |
| 7 | — | `p.drawPixmap(0,0, m_fgCache)` blit（前景在上） |

### renderUncached → drawDirect

`drawBackground + drawForeground` 直接画到导出 device——**不读不写缓存**（避免矢量 device 被栅格化 + 不污染屏显缓存，D13）；PDF 恒填背景（忽略透明开关）。

### GL 后端差异（A4 边界）

- `render(scene, device)`：`device` 必须是宿主 QOpenGLWidget，否则 qWarning 返回（GL 无通用 QPaintDevice 目标）；实际绘制由宿主 `paintGL` 回调 → `m_glRenderer->paintGL(scene)`（buildBatches + 分层主 pass）。
- `renderUncached`：qWarning 提示改用 QPainterChartRenderer（GL 不参与导出）。
- `invalidateBackground/Foreground`：置 BatchPool.dirty（VBO 重建时机；**视图变化不触发**——顶点是 World 坐标，仅 u_viewProj uniform 变化，D30）；`setCachingEnabled` no-op、`isCachingEnabled` 恒 true。

## 时序（触发时机与先后）

1. **屏显**：paintEvent 每帧（update 合并）→ render；缓存命中（脏标记 false）→ 仅 blit（O(1)）；脏 → 先重建对应缓存再 blit（背景先于前景）。
2. **导出**：saveAs* 同步调用 → buildExportScene → renderUncached（无缓存路径，一次直绘）；导出不触碰屏显缓存状态（互不污染）。
3. **GL**：宿主 QOpenGLWidget 生命周期回调（initializeGL 首帧 → paintGL 每帧 → resizeGL）独立于 QWidget::paintEvent；GL 就绪前 GlHost 隐藏（回退纯 QPainter，§5.1 透明语义教训）。

## 关联

- Called By：QChartWidget::paintEvent / saveAsPng/Svg/Pdf（详见 docs/core/QChartWidget.md）。
- 相关决策：D2（渲染器分两层）、D13（导出 renderUncached）、D26（统一后端同决渲染与拾取）、D28（图元瞬态化）。
