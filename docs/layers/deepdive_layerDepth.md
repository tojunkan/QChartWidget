# deepdive_layerDepth.md —— 分层收集与深度偏置

> t53 核心计算深挖 · layers 模块
> 主题：QChartLayer3D 的图元分层收集（Grid/ForegroundDecor/Series）、painter's algorithm 深度降序（D16）与 kGridDepthBias 两后端等价（D29）。

---

## 0. 问题背景

3D 无硬件深度缓冲（QPainter 路径）→ 用 painter's algorithm：图元按深度排序、远先画近后画。但网格与系列同深度时谁赢？前景层（spine/刻度/标签）必须恒后画。D16/D24/D29 定案：**Grid/Series 统一深度排序 + kGridDepthBias=1e-3（同深度系列赢）+ ForegroundDecor 恒后画**；GL 路径用 z-buffer + glPolygonOffset 达到**像素级等价**。

## 1. 深度定义与排序

```
depth = −viewZ（viewMatrix 变换后 z 取负：越大越远）   # QChartMath::viewDepth
```

- `viewDepth(view, worldPoint)` = 视图空间 z 的负值（src/utils/QChartMath.h）——**方向与视觉一致：近处 depth 小、远处 depth 大**。
- 排序：图元按 `depth` **降序**（远先画）→ 近者后画覆盖（D16；nearCoversFar 像素断言锁死）。

## 2. 分层（QChartPrimitive::Layer）与深度偏置

`QChartPrimitive::Layer` 三档（src/core/QChartRenderer.h）：

| 层 | 深度处理 | 语义 |
|---|---|---|
| Grid | `depth -= kGridDepthBias`（=1e-3） | 网格与系列统一排序，但网格整体"退后" 1e-3——**同深度系列优先**（网格不遮挡系列） |
| Series | 原始 depth | 真实深度遮挡 |
| ForegroundDecor | 恒后画（不参与深度排序，最后画） | spine/刻度点/标签等前景装饰永远在最上层 |

`kGridDepthBias = 1e-3`（`static constexpr qreal`，QChartRenderer.h）——painter 版的"polygon offset"。

## 3. 两后端等价（D29）

| 路径 | 机制 | 等价性 |
|---|---|---|
| QPainter | 深度降序排序 + Grid 减 kGridDepthBias + Decor 最后画 | 基准 |
| OpenGL | Grid/Series 批次开深度测试 + `glPolygonOffset(1, 1)` 等价 kGridDepthBias 语义；ForegroundDecor `depthTest=false` 后画（src/core/QOpenGLChartRenderer.cpp `b.depthTest/depthBias`） | 与 QPainter **像素级等价断言**锁死 |

GL 批次字段：`depthTest = (layer != ForegroundDecor)`；`depthBias = (layer == Grid) ? kGridDepthBias : 0`（§5.2）。

## 4. 图元收集流程（collectPrimitives，src/layers/3d/QChartLayer3D.cpp）

```
collectPrimitives(cam, plotArea, out, labels?)
  1. axesDataBox 有效？→ 轴/网格图元：
       boxCorners/boxEdges/spine（Grid 层，depth 减 bias）
       每轴刻度锚点 → emitLine（Grid 层）
       刻度点/标签（ForegroundDecor 层）
  2. 系列图元（Series 层）：每个 series3D 经 makeProjectFn 闭包 collectPrimitives
       （depth = 投影深度，dataIndex 携带）
  3. worldCache 直算填充（数值型/曲面；混合系列跳过）
  4. labels 可选出参（billboard）
  5. 排序（深度降序，跨系列全局视野——排序归 Renderer 3D 子路径，D16）
```

## 5. 边界与陷阱

1. **同深度歧义**：不处理"同深度图元互相遮挡"的细分规则（如折线自交同深度）——bias 只解决网格 vs 系列；系列间同深度按排序稳定性（后加入者先画）。
2. **bias 数值**：1e-3 是经验值——过大 → 网格浮空；过小 → 浮点下同深度失效。修改必须重跑 nearCoversFar 像素断言（D16 锁死）。
3. **GL 等价**：glPolygonOffset 的量纲与 kGridDepthBias 不同（深度缓冲单位），等价断言以**像素结果**为准，不要求数值相等（§5.2 语义等价）。
4. **Decor 无深度**：ForegroundDecor 关深度后画——若未来加"前景遮挡"需求，需新层语义（当前设计恒可见，A4）。
5. **排序成本**：O(N log N)（N=图元数）；Phase 3 改 z-buffer 后排序仅用于 CPU 兜底路径（D30 GPU 投影路径消灭每帧 CPU 投影瓶颈）。

## 6. 单测对照

| 锚点 | 断言 |
|---|---|
| TestQChartRenderer3D | 深度降序正确（远先画）、Grid bias 后同深度系列赢（nearCoversFar 像素断言） |
| TestQChartMath | `viewDepth` 方向（−viewZ 越大越远）与公式一致 |
| TestQOpenGLRenderer | GL 深度测试/glPolygonOffset 与 QPainter 像素级等价（offscreen QSKIP；wayland/xcb 实跑） |
| TestQChartAxes3D | 轴/网格图元 Layer 归类（Grid vs ForegroundDecor） |

> 回归口径：ctest 180 PASS + 2 SKIP 全绿；bias 值/分层规则/排序方向改动必须重跑渲染对照测试（像素断言）。
