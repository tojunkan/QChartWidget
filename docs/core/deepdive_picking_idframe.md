# deepdive_picking_idframe.md —— GPU 拾取：ID 帧、三闸限流与解码

> t53 核心计算深挖 · core 模块（队长追加第 8 篇）
> 主题：统一后端（D26）下 GPU 拾取的全链路——ID 帧自包含、图元 ID 零内存分配、三闸限流、0xFFFFFF 哨兵与 `hitTestGPU` 解码、两后端交叉验证。

---

## 0. 问题背景

D26（统一后端原则，用户定案）：后端开关**同时决定渲染与拾取**——CPU 后端 = QPainter + CPU 近邻命中，GPU 后端 = GL + ID 帧拾取，禁止混搭。D27：拾取不是 renderer 职责，`hitTest` 统一进 `QChartHitTester`（2D 多边形 / 3D 屏幕近邻 / GPU 颜色编码三实现），`uvHovered/uvSelected` 信号零改动。

## 1. 拾取三实现总览（QChartHitTester，`include/core/QChartHitTester.h`）

| 实现 | 输入 | 算法 | 后端 |
|---|---|---|---|
| `hitTest(pixel, seriesList, toPixel, ctx)` | 2D 像素 | 顶层可见系列优先（自后向前）+ `series->hitTest`（像素 in 多边形） | CPU |
| `hitTest(pixel, primitives, maxDistPx=8)` | 3D 像素 | 只扫 `Layer==Series` 图元；点到点/点到线段距离 < 8px 取最近；dataIndex 透传 | CPU |
| `hitTestGPU(r,g,b, pickTable)` | 1×1 读回的 RGB24 | `id = r | g<<8 | b<<16` → 查表 → `HitResult`（纯函数，可单测） | GPU |

统一结果 `HitResult{series, index, dataIndex}`（2D 下 dataIndex==index）。

## 2. GPU 拾取架构：ID 帧自包含

`QOpenGLChartRenderer::pickIdAt(pos, scene)`（src/core/QOpenGLChartRenderer.cpp）——**ID 帧与主 pass 完全解耦**：

```
makeCurrent()                          # 必须先 makeCurrent（见 §6.1 陷阱）
if (m_batches.dirty) buildBatches(scene)   # 拾取表与批次同步重建
绑定默认 FBO；glViewport(0,0, plotArea.w, plotArea.h)
glClearColor(1,1,1,1); glClear(COLOR|DEPTH)      # 背景 = 0xFFFFFF 哨兵
drawPass(scene, ShaderKind::Pick)                # 全批次（含哨兵批次）重绘到 ID 帧
glReadPixels(pos.x, plotArea.h−1−pos.y, 1,1, RGBA)  # 1×1 读回，y 翻转（GL 行序底→顶）
```

**自包含三要素**：① 清 color（哨兵白背景 = 无图元 → 未命中）；② 清 depth + 写 depth——ID 帧内复用与主 pass **相同**的深度语义（Grid 偏置、Series 深度遮挡、Decor 关深度后画），保证"被遮挡的图元在 ID 帧也读不到"；③ 与主 pass 时序完全解耦（不依赖主 pass 是否已画过、何时画过）。

## 3. 图元 ID 零内存分配（§5.3 定案）

- **pickTable**：与批次同步构建（`buildBatches`），`PickRecord{series, dataIndex, layer}` 逐图元追加；`Q_ASSERT(pickTable 增量 == 图元增量)` 锁死 ID 与批次顶点一一对应。
- **ID = baseId + gl_VertexID/vertsPer**：每个分片批次记 `baseId`，顶点着色器按 `gl_VertexID / vertsPer` 得图元序号，加 baseId 即全局图元 ID——**零额外内存**（不写 ID 进顶点）。
- **哨兵批次**：`idSentinel = (layer != Series)`——轴/网格/Decor 片段在顶点着色器直接输出 0xFFFFFF（`u_sentinel` uniform），不入拾取表；Series 层片段输出真实 ID。
- 分片规则：同 (Layer, primitive, pointSize) 合并、每批次顶点 ≤ 64K（kMaxVertsPerBatch）、Point 批次按 markerSize 分键（混合点尺寸画错陷阱）。

## 4. 解码：hitTestGPU

```
id = r | (g << 8) | (b << 16)
id == 0xFFFFFF（哨兵：背景 / 轴网格 Decor 片段）或越界 → 空 HitResult（dataIndex == -1）
否则 pickTable[id] → HitResult{series, index=dataIndex, dataIndex}
```
纯函数（无 GL 依赖）——单测可直接喂 RGB 三元组验证解码与哨兵语义（`TestQChartHitTester`）。

## 5. 三闸限流（§5.3）与软渲染实测

`pickIdAt` 的调用节流：

| 闸 | 条件 | 动作 |
|---|---|---|
| ①（外部） | Qt 事件循环合并连续 mouse move | 高频移动天然降频 |
| ② | 位移 <1px（pos 未变）**或** 距上次 <16ms | 直接返回上次结果（`m_lastPickResult` 缓存） |
| ③ | `!m_glReady`（惰性初始化前 / 上下文不可用） | 返回哨兵（未命中） |

**llvmpipe 实测 ~31–59ms vs 16ms 闸**：软渲染下单次 ID 帧（makeCurrent + 批次上传 + 全 pass 重绘 + glFinish + readback）可达数十毫秒，远超 16ms 闸——16ms 闸不是"保证帧率"，而是**避免同一光标位置重复触发昂贵的 ID 帧**；真实 GPU 上 ID 帧是 O(1) readback + 小视口重绘，16ms 闸仅为防御。验收口径（D31）：软渲染只冒烟、不作性能基准。

## 6. 边界与陷阱

1. **makeCurrent 顺序**（t46 实测教训）：必须先 `makeCurrent` 再 `buildBatches`——`uploadBatches` 是 GL 调用；先建批次后 makeCurrent → 上传空转 → 批次为空 → ID 帧全哨兵 → 永远未命中。
2. **readback y 翻转**：GL 行序底→顶，光标 y 需 `plotArea.height()−1−pos.y` 翻转；越界（pos 不在 plotArea）→ 保持哨兵。
3. **glFinish 确定性**：软渲染下必须 glFinish 保证绘制完成再 readback（异步管线会读到未完成的帧）。
4. **viewport = plotArea 而非全 widget**：ID 帧只覆盖 plotArea；plotArea 外未命中（哨兵）。
5. **哨兵白与真实 ID 冲突**：0xFFFFFF 保留为哨兵，Series 图元 ID 从 0 递增、永不达 0xFFFFFF（表长受内存预算约束，D28）。
6. **两后端交叉验证**：同一 (scene, pos) 下 CPU 近邻命中与 GPU ID 命中结果应一致（除 CPU 8px 阈值 vs GPU 像素精度的边界差异），demo/测试双跑对比（设计 §8.2）。

## 7. 源码位置与单测对照

| 符号 | 文件 |
|---|---|
| `pickIdAt(pos, scene)`（ID 帧 + 三闸） | src/core/QOpenGLChartRenderer.cpp |
| `buildBatches`（pickTable 同步 + 对齐断言）/ `uploadBatches`（分片/16B 顶点/baseId/idSentinel） | src/core/QOpenGLChartRenderer.cpp |
| `drawPass(scene, ShaderKind::Pick)`（u_baseId/u_sentinel uniform） | src/core/QOpenGLChartRenderer.cpp |
| `QChartHitTester::hitTestGPU` + `PickRecord` | src/core/QChartHitTester.cpp / include/core/QChartHitTester.h |
| 三实现调度（后端分支） | QChartWidget3D 事件层 → 渲染器 → hitTester |

- 单测：`hitTestGPU` 解码/哨兵/越界（纯函数，offscreen 可跑）；`TestQChartGL`/`TestQOpenGLRenderer` 在 offscreen 平台 QSKIP（无真实 GL），wayland/xcb 实跑验证上下文与 ID 帧；`uvHovered` 信号语义零改动（D18 联动回归）。
- 回归：ctest 180 PASS + 2 SKIP 全绿；GL 改动另跑 wayland/xcb 冒烟（3D demo + 悬停）。
