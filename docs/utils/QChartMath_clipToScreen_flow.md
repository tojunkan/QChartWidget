# QChartMath_clipToScreen_flow.md —— Clip→NDC→Screen 全链（w≤0 哨兵 + y 翻转）

> t55 核心函数 flow · utils 模块（include/utils/QChartMath.h `clipToScreen`）★全库 3D 像素落点（每个 3D 点都经此）

## 控制流（调用图）

```
3D 链路终点（每个 World 点）:
  QChartCamera3D::project(world, plotArea)
    └─ viewProj·world → QVector4D clip
    └─ QChartMath::clipToScreen(clip, plotArea)
         ├─ clip.w <= 0.0 → NaN 哨兵（相机背后/近平面外）       # 第一道闸
         └─ ndc = clipToNdc(clip) = clip.xyz / clip.w
              └─ ndcToScreen(ndc, plotArea):
                   x = plotArea.left() + (ndc.x + 1)·0.5·plotArea.width()
                   y = plotArea.bottom() − (ndc.y + 1)·0.5·plotArea.height()   # ★ y 翻转

下游消费（NaN → 跳过）:
  Layer3D emitLine/collectPrimitives（断段/跳过点）
  QChartLineSeries3D/Surface collectPrimitives（valid 标记）
  projectBatch（批量逐点）
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `clip`（QVector4D 齐次坐标：viewProj·[world,1]）、`plotArea`（像素区） |
| 中间量 | `ndc`（透视除法后 [−1,1]³；w≤0 → NaN 三元组） |
| 出参 | `QPointF`（plotArea 内像素坐标）或 `(NaN, NaN)`（哨兵） |
| 状态变更 | 无（纯函数） |

**两处 w≤0 检查**：`clipToScreen` 先查 `clip.w<=0`，`clipToNdc` 内部再查（防御双保险）——漏一处就会把相机后方的点画出来（t45/t47 审查重点）。

**y 翻转语义**：NDC y 向上（OpenGL 约定）→ 屏幕 y 向下：`bottom − (ndc.y+1)/2·h`——与 2D `QChartCamera2D::cartesianToPixel` 的 `bottom − ny·h` 同向（"View 上 → 像素上"）——**正交俯视 ≡ 2D 的逐点一致性基础**（D-3D-2 硬验收，TestQChartMath 双向锁定）。

## 时序（触发时机与先后）

1. **逐点即时调用**：闭包（makeProjectFn）/emitLine 对每个 World 点调用——无缓存（O(1)）。
2. **批量路径**：projectBatch 内逐点同函数（两数组对齐，NaN 槽位保留不压缩）。
3. **GL 差异**：GPU 路径不在 CPU 调 clipToScreen——顶点着色器内做透视除法（同数学约定，GLSL 与 CPU 双实现一致性由 t45 等价断言锁死）。

## 边界与陷阱

1. **w≤0 三重场景**：相机后方、近平面外、退化投影——统一 NaN 哨兵。
2. **NaN 传播**：clip 含 NaN（上游 toWorld 奇点）→ 除法 NaN → 哨兵（不崩溃）。
3. **plotArea 边界**：点可落在 plotArea 外（不裁剪——可见性由 DrawContext::pixelVisible/rectVisible 粗筛，见 QChartAxis.md）。
4. **与 2D 一致性断言**：y 翻转方向写反 → 3D 上下颠倒且与 2D 不一致——TestQChartMath 对照断言锁死（回归口径）。

## 关联

- Called By：QChartCamera3D::project（src/core/QChartCamera3D.cpp）/projectBatch/Layer3D。
- 全推导：docs/utils/deepdive_math.md；批量入口：docs/utils/QChartMath_projectBatch_flow.md。
- 相关决策：D-3D-1（QChartMath）、D-3D-2（退化一致性硬验收）、design_3d §3 ⚠（w≤0 NaN 与 y 翻转约定）。
