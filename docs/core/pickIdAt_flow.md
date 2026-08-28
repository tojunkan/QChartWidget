# pickIdAt_flow.md —— GPU 拾取：ID 帧自包含 + 三闸限流 + 解码

> t55 核心函数 flow · core 模块（src/core/QOpenGLChartRenderer.cpp `pickIdAt`）
> 深挖版：docs/core/deepdive_picking_idframe.md（算法与边界全推导）；本文聚焦调用/数据/时序。

## 控制流（调用图）

```
QChartWidget3D::updateHover（GL 分支）
  └─ pickIdAt(pos, scene)
       ├─ 【闸③】!m_glReady → 返回哨兵 qRgb(255,255,255)      # 惰性初始化前跳过
       ├─ 【闸②a】pos == m_lastPickPos → 返回 m_lastPickResult # 位移 <1px 保持
       ├─ 【闸②b】now − m_lastPickMs < 16 → 返回 m_lastPickResult  # <16ms 保持
       ├─ 宿主校验：qobject_cast<QOpenGLWidget*>（无/上下文无效 → 哨兵）
       ├─ host->makeCurrent()                                   # ★ 先 makeCurrent 再建批次（t46 教训）
       ├─ m_batches.dirty ? buildBatches(scene)                 # 拾取表与批次同步重建
       │    └─ collectScene → pickTable 增量对齐 Q_ASSERT
       │    └─ uploadBatches（分片/16B 顶点/baseId/idSentinel）
       ├─ ID 帧（★自包含，与主 pass 时序完全解耦）:
       │    ├─ glBindFramebuffer(默认 FBO)；glViewport(0,0, plotArea.w, plotArea.h)
       │    ├─ glClearColor(1,1,1,1)；glClear(COLOR|DEPTH)       # 背景=0xFFFFFF 哨兵 + 清 depth + 写 depth
       │    └─ drawPass(scene, ShaderKind::Pick)                 # 全批次（含哨兵批次），复用主 pass 深度语义
       ├─ 1×1 readback：glFinish → glReadPixels(x, h−1−y, 1,1, RGBA)（y 翻转；越界→哨兵）
       ├─ 缓存限流状态：m_lastPickPos/Ms/Result/Valid 更新
       └─ 返回 qRgb(r,g,b) → updateHover → hitTestGPU 解码
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `pos`（**宿主局部坐标**——调用方已把外层坐标换算到 ID 帧视口 plotArea）；`scene`（快照） |
| 中间量 | pickTable（与批次同步：PickRecord{series,dataIndex,layer}）；GLBatch{baseId, idSentinel, depthTest, depthBias, pointSize}；ID 帧像素 px[4] |
| 出参 | `QRgb`（RGB24；0xFFFFFF = 背景/轴网格 Decor 哨兵 = 未命中） |
| 状态变更 | `m_lastPickPos/Ms/Result/Valid`（限流缓存）；`m_batches.dirty`（若重建）→ false；批次缓存（m_cachedPrimitives 上传后 clear+squeeze，D28） |
| 解码（下游） | `hitTestGPU(r,g,b, pickTable)`：`id = r\|g<<8\|b<<16`；0xFFFFFF 或越界 → 空 HitResult；否则 pickTable[id] → {series, dataIndex} |

## 时序（触发时机与先后）

1. **鼠标移动触发**（updateHover），非定时器；【闸①】Qt 事件循环合并连续 move（外部）。
2. **三闸顺序**：③m_glReady（最快路径）→ ②位移/16ms（缓存命中）→ 放行执行 ID 帧。
3. **makeCurrent 先于 buildBatches**：uploadBatches 是 GL 调用——先建批次后 makeCurrent 会空转上传 → 批次为空 → ID 帧全哨兵（t46 实测教训）。
4. **ID 帧与主 pass 解耦**：清 color（哨兵白）+ 清 depth + 写 depth，与主 pass 深度/时序完全独立——首帧前直接拾取也正确（t47 独立 probe 验证）。
5. **readback 确定性**：glFinish 保证绘制完成（软渲染异步管线否则读到未完成帧）；y 翻转（GL 行序底→顶）。
6. **16ms 闸与软渲染**：llvmpipe 单次 ID 帧 ~31-59ms（超过 16ms 闸）——闸是防御重复昂贵 ID 帧，不是帧率保证（真实 GPU 上 ID 帧为 O(1) readback + 小视口重绘）。

## 关联

- 深挖推导：docs/core/deepdive_picking_idframe.md；解码纯函数：QChartHitTester::hitTestGPU。
- 相关决策：A6（GPU 拾取）、D26（统一后端）、D27（拾取归 QChartHitTester）、§5.3（三闸限流与哨兵定案）。
