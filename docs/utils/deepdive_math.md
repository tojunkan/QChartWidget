# deepdive_math.md —— QChartMath 纯函数集：Clip→NDC→Screen 管线

> t53 核心计算深挖 · utils 模块
> 主题：3D 数学纯函数的完整推导（Clip→NDC→Screen 拆分、y 翻转、深度语义、批量投影与反投影），以及为什么 Phase 3 GPU 路径复用同一套函数。

---

## 0. 问题背景

3D 链路终点是屏幕坐标，但 Clip 空间的语义（齐次坐标、相机后方）必须显式处理；同时 Phase 3 的 GPU 路径（D30）要求"CPU 与 GPU 用同一套数学约定"。design_3d.md §3 定案：`QChartMath` = header-only inline 纯函数，显式拆分 **Clip → NDC → Screen**（Phase 3 GL 顶点着色器同用），并提供深度辅助与批量投影/反投影入口。

## 1. 推导：Clip → NDC → Screen

### 1.1 Clip → NDC（`clipToNdc`）

```
ndc = clip.xyz / clip.w        # 透视除法
clip.w ≤ 0（相机背后/近平面外）→ NaN 哨兵（调用方跳过，绝不把负 w 当正常点）
```

### 1.2 NDC → Screen（`ndcToScreen`）

NDC 范围 [−1,1]²，屏幕 y 向下：

```
x = plotArea.left() + (ndc.x + 1) · 0.5 · plotArea.width()
y = plotArea.bottom() − (ndc.y + 1) · 0.5 · plotArea.height()    # ★ y 翻转
```

**y 翻转与 2D 一致性**：2D `cartesianToPixel` 的 y 是 `bottom − ny·h`（View 上 → 像素上）；此处 NDC y 向上（OpenGL 约定）→ 同样翻转——**两种映射在"正交俯视 ≡ 2D"下逐点一致**（D-3D-2 硬验收，单测双向锁定）。

### 1.3 Clip → Screen 组合（`clipToScreen`）

`w≤0 → NaN`；否则 `ndcToScreen(clipToNdc(clip), plotArea)`。

## 2. 深度语义（`viewDepth`）

```
depth = −(viewMatrix · worldPoint).z    # 视图空间 z 取负
```

相机前方（负 z）→ 正 depth；**数值越大越远**（排序键：painter's algorithm 降序 = 远先画，D16）。与相机 dolly/orbit 联动时深度随视图矩阵变化——图元 depth 在收集时即时计算（非缓存），保证排序正确。

## 3. 批量投影与反投影（Phase 3 预留）

### 3.1 `projectBatch`（World 批量 → 屏幕 + 深度）

```
for i: clip = viewProj · (world[i], 1)
       outScreen[i] = clipToScreen(clip, plotArea)     # w≤0 → NaN 槽位
       outDepth[i]  = viewDepth(view, world[i])
```
两数组**逐元素对齐**（NaN 槽位保留，不压缩）——Phase 2 已实现并单测；Phase 3 换 GPU 批量/预转换时**签名不变**（D-3D-10）。

### 3.2 `unproject`（Clip → World，Phase 3 射线拾取预留）

```
clipPos.w ≤ 0 → NaN；viewProj 不可逆 → NaN
worldH = inv(viewProj) · clipPos；worldH.w ≤ 0 → NaN
world = worldH.xyz / worldH.w
```

## 4. 源码位置

| 符号 | 位置 |
|---|---|
| `clipToNdc / ndcToScreen / clipToScreen` | include/utils/QChartMath.h |
| `perspectiveMatrix / orthographicMatrix` | include/utils/QChartMath.h（Qt 封装，集中约定 frustum 参数） |
| `viewDepth` | include/utils/QChartMath.h |
| `projectBatch / unproject` | include/utils/QChartMath.h |
| 消费方：`QChartCamera3D::project` / `viewProjectionMatrix` | src/core/QChartCamera3D.cpp |

## 5. 边界与陷阱

1. **w≤0 的双重保护**：`clipToNdc` 与 `clipToScreen` 都查 `w<=0`（clipToScreen 组合时先查）；`unproject` 查 `clipPos.w` 与 `worldH.w` 两处——漏一处就会把相机后方的点画出来。
2. **y 翻转方向**：翻转写错（+ 变 −）会让 3D 上下颠倒且与 2D 不一致——TestQChartMath 的对照断言（clipToScreen vs cartesianToPixel）锁死。
3. **NaN 槽位语义**：projectBatch 不压缩数组（索引对齐是契约）；调用方按槽位消费，遇 NaN 跳过（不画/不参与包围盒）。
4. **不可逆矩阵**：unproject 必须查 `inverted(&ok)`——奇异矩阵（如零视角）返回 NaN 而非垃圾。
5. **header-only 约束**：新增函数必须 inline（#pragma once + inline），否则 CMake QCHART_SOURCES 需加 .cpp（红线：当前 utils 无 .cpp）。

## 6. 单测对照

| 锚点 | 断言 |
|---|---|
| TestQChartMath | clipToNdc w≤0→NaN；ndcToScreen y 翻转与 `QChartCamera2D::cartesianToPixel` 一致；clipToScreen 往返；透视/正交矩阵性质（near/far 投影后 z 单调）；viewDepth 方向（−viewZ 越大越远）；projectBatch 与逐点投影一致且两数组对齐；unproject 不可逆/负 w → NaN |
| TestQChartCamera3D | `viewProjectionMatrix` 与 `project` 走 QChartMath 全链（正交俯视 ≡ 2D） |

> 回归口径：ctest 180 PASS + 2 SKIP 全绿；任何数学公式改动（y 翻转、w≤0 判定、深度方向）必须重跑 TestQChartMath + TestQChartCamera3D（像素断言双锁）。
