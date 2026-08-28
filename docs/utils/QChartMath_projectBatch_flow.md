# QChartMath_projectBatch_flow.md —— 批量投影入口与 depth 对齐（Phase 3 GPU 预留）

> t55 核心函数 flow · utils 模块（include/utils/QChartMath.h `projectBatch`）★Phase 3 GPU 批量/预转换的 CPU 侧契约

## 控制流（调用图）

```
当前：单测覆盖（TestQChartMath：与逐点投影一致 + 两数组对齐）
Phase 3（D-3D-10 预留）：数值预转换缓存/GPU 批量路径——签名不变
  └─ projectBatch(viewProj, view, plotArea, world[], outScreen[], outDepth[])
       ├─ outScreen->resize(world.size()); outDepth->resize(world.size())   # 预分配，槽位保留
       └─ for i ∈ [0, world.size()):
            clip = viewProj · [world[i], 1]
            outScreen[i] = clipToScreen(clip, plotArea)      # w≤0 → NaN 槽位
            outDepth[i]  = viewDepth(view, world[i])         # −viewZ（排序键）
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `viewProj`（World→Clip 合并矩阵）、`view`（视图矩阵——depth 用，与 viewProj 分离传入）、`plotArea`、`world`（World 点数组） |
| 出参 | `outScreen`（像素点数组）+ `outDepth`（深度数组）——**逐元素对齐**（同一索引 = 同一点；NaN 槽位保留，**不压缩**） |
| 状态变更 | 无（纯函数；两出参 resize 后填充） |

**对齐契约**：`outScreen[i] ↔ outDepth[i] ↔ world[i]` 一一对应——消费方按槽位遍历、遇 NaN 跳过（不画/不参与排序）；压缩数组会破坏索引对齐（红线）。

## 时序（触发时机与先后）

1. **CPU 路径**：批量逐点（等价于逐点调用 clipToScreen/viewDepth——单测断言一致性）；Phase 2 已实现并单测。
2. **Phase 3 GPU 替换**：换 GPU 批量/预转换时**签名不变**（D-3D-10）——CPU 侧 worldCache/numericCache 预转换后批量喂给 VBO（顶点着色器仅 u_viewProj，D30）。
3. **depth 与 screen 同源**：两者独立计算（depth 用 view 矩阵、screen 用 viewProj）——viewProj 含投影（含 near/far 压缩）不能用于 depth（排序键必须视图空间 −viewZ）。

## 边界与陷阱

1. **空数组**：world 空 → 两出参 resize(0)（安全）。
2. **出参为 null**：`!outScreen || !outDepth → return`（防御）。
3. **NaN 槽位语义**：w≤0 点 screen=NaN 但 depth 仍计算（viewDepth 无 w 依赖）——消费方必须同时查 screen 有限性（t45 审查：depth 可能有限而 screen NaN，跳过判定以 screen 为准）。
4. **矩阵分离**：view 与 viewProj 都要传——传错（只用 viewProj 算 depth）会得到压缩深度（排序错误）。

## 关联

- Called By：当前无库内调用（单测覆盖）；Phase 3 预转换/GPU 路径预留。
- 单点版：docs/utils/QChartMath_clipToScreen_flow.md；深挖：docs/utils/deepdive_math.md。
- 相关决策：D-3D-10（Phase 3 GPU 批量/预转换签名不变）、D30（GPU 投影路径）、design_phase3 §9（数值预转换缓存）。
