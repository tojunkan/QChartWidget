# deepdive_viewCube.md —— viewCube→dataBounds 5³ 反算与笛卡尔快速通道

> t53 核心计算深挖 · projection 模块（关联 core 的 deepdive_viewRect：viewCube 派生链见该文）
> 主题：R6 用户定案（D23）的反算策略——为什么通用坐标系必须采样、5×5×5=125 点网格如何聚合、恒等快速通道为何免采样。

---

## 0. 问题背景

3D 视图变化（orbit/dolly/panViewCube/设 viewCube）后，轴范围（dataBounds，Numeric 空间）必须随 viewCube（World 空间）反算。R5 定案**无逆矩阵/unproject**（R5 移除逆映射方案），R6 定案采样法。本 deepdive 推导"反算为什么不能直接取角点"、125 点采样算法、与 `isIdentityMapping` 快速通道（`src/core/QChartWidget3D.cpp` `recomputeDataBounds3D`）。

## 1. 为什么必须采样：极值不在角上

`fromWorld` 是 `toWorld` 的逆。对通用投影（柱/球），Numeric 空间的 axis-aligned 极值在 World 空间**不是**盒的角点：

- 球坐标 `toWorld(r,θ,φ)`：r 的极值（max r）出现在赤道面（sinφ=1 的 θ 任意方向），World 盒角点（如 (+x,+y,+z)）未必对应 r 最大——**World 盒的角点映射回 Numeric 并不等于 Numeric 极值**。
- 柱坐标同理：r=√(x²+y²) 的极值在 World 盒侧面的中点附近，不在角。

因此反算 = **在 viewCube 内采样 World 点 → fromWorld → 聚合 Numeric min/max**。

## 2. 算法：5³=125 点网格采样（`QChartWidget3D::recomputeDataBounds3D`）

```
输入：viewCube = {min, max}（World 轴对齐盒）
levels = {0.0, 0.25, 0.5, 0.75, 1.0}          # 每轴 5 档：min/25%/50%/75%/max
for i,j,k ∈ [0,5)³:                            # 5×5×5 = 125 个 World 采样点
    p = min + (levels[i], levels[j], levels[k]) ⊙ (max−min)
    num = projection3D->fromWorld(p)
    if !finite(num): continue                  # 奇点 NaN/Inf 跳过
    聚合 num.x/y/z 的 min/max
全 NaN → Valid=false（dataBounds3D 失效；A9 兜底：轴/网格锚定域盒 m_anchorBox）
否则 dataBounds3D = {min, max}, Valid=true
```

要点：
- **每轴 5 档**（含两端 min/max）——D23 用户拍板，5³=125 点（非 16³=4913 点；反向反算的精度/成本权衡）。
- **非有限跳过**：奇点（柱/球 r=0、atan2 退化）产 NaN 的点不参与聚合。
- **全 NaN → Valid=false**：不猜测、不产出垃圾范围；调用方（轴/网格）用锚定域盒兜底。

## 3. 快速通道：isIdentityMapping（0 次 fromWorld）

`QChartCartesianProjection3D::isIdentityMapping() == true`（Numeric ≡ World）。此时：

```
dataBounds = viewCube（直接赋值，0 次采样）
```

同一标志还驱动（D23 全链）：
- **图元生成免 toWorld**：Layer3D 恒等路径 Numeric≡World 直通；
- **段数 = 2**：`samplingSegmentsHint()` 返回 2（两点直线），弯曲投影 32。

## 4. 伪代码 vs 源码位置

| 步骤 | 源码 |
|---|---|
| 反算入口（视图变化槽，每帧不重算，§9） | src/core/QChartWidget3D.cpp `recomputeDataBounds3D` |
| 快速通道判定 | include/projection/QChartCartesianProjection3D.h `isIdentityMapping` |
| 通用 5³ 采样 | src/core/QChartWidget3D.cpp（levels[5] 三重循环） |
| 反向映射（奇点 NaN） | include/projection/QChartCylindricalProjection3D.h / QChartSphericalProjection3D.h `fromWorld` |
| 正向包围盒（对照：16³ 采样） | include/projection/QChartProjection3D.h `computeWorldBounds` |

## 5. 边界与陷阱

1. **角点陷阱**：直接取 viewCube 8 角点反算在柱/球投影下会**低估/错估** Numeric 范围（§1）——必须 125 点采样；这是 R6 定案的原因。
2. **采样粒度权衡**：5 档是用户拍板的精度点；更密（如 9³=729）更准但每帧反算成本更高——视图变化槽"每帧不重算"（只在实际视图变化时反算一次）。
3. **全 NaN 失效语义**：Valid=false 时 dataBounds3D 不可用——下游必须走 A9 锚定盒，不能读零值（初始化为 (0,0,0) 是"无效"标记而非真实范围）。
4. **快速通道前提**：只有 Cartesian3D 恒等才免采样；`isIdentityMapping()` 默认 false，新增投影若不重写会被安全地当成通用路径。
5. **与 2D 一致性**：正交俯视 + 恒等时反算结果 ≡ 2D `computeDataBounds`（viewRect 对标物一致性，D-3D-2）。

## 6. 单测对照

| 锚点 | 断言 |
|---|---|
| TestQChartProjection / TestQChartMath | fromWorld 奇点 NaN（柱/球 r=0）、computeWorldBounds 采样与全 NaN 回退 |
| TestQChartCamera3D | orbit 后 viewCube 不变 → 反算输入稳定；正交俯视 ≡ 2D |
| TestQChartWidget3D（或 axes3d 相关） | 视图变化 → dataBounds3D 反算；快速通道 0 采样路径正确性 |

> 回归口径：ctest 180 PASS + 2 SKIP 全绿；任何改动涉及反算算法（levels 档位/采样密度/快速通道判定）必须重跑上述测试类。
