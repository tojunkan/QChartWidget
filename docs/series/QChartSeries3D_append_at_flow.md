# QChartSeries3D_append_at_flow.md —— 双存储分发与物化回退

> t55 核心函数 flow · series 模块（src/series/3d/QChartSeries3D.cpp）★用户内存关切核心（A3 预算：float3 12B/点 vs QVariant 50~70MB）

## 控制流（调用图）

```
数据写入（append/insert/replace/remove/clear/setPoints）:
  append(qreal×3)  ── numericCacheActive? ── true  → m_numericCache.append(float3)  # ★ 增量维护，保持数值型
  │                                    └─ false → append(QDataPoint3D(x,y,z))      # 混合：QVariant 路径
  append(QDataPoint3D) ── numericCacheActive? ── true → materializeToQVariant()    # ★ 回退（顺序语义保持）
  │                                        └─ false → m_points.append(pt)
  append(QVariant×3)  ── 同 QDataPoint3D 版（首个 QVariant → 回退物化）
  insert(index, pt)   ── numericCacheActive → materializeToQVariant() 再 m_points.insert  # 与 replace 一致
  replace(index, pt)  ── numericCacheActive → materializeToQVariant() 再 m_points.replace  # 失效定案
  remove(index)       ── numericCacheActive → m_numericCache.remove(index)  # ★ 增量维护免回退
  clear()             ── 双缓存清空 + numericCacheActive 复位 true
  setPoints(pts)      ── QVariant 路径（m_points 整批替换 + 缓存失效）

数据读取（count/at/points）:
  count()      = numericCacheActive ? m_numericCache.size() : m_points.size()
  at(i)        = numericCacheActive ? float3→QDataPoint3D 单点物化 : m_points.at(i)
  points()     = numericCacheActive ? 按需物化视图（m_pointsView 懒构建，m_pointsViewValid 缓存）
                                    : m_points（权威）
```

## 数据流（入参/出参/状态变更）

| 操作 | 触发回退? | 状态变更 |
|---|---|---|
| append(qreal×3)（激活时） | 否（保持数值型） | `m_numericCache` append；`m_pointsViewValid=false`；`m_worldCache.clear()`；emit dataChanged |
| append(QDataPoint3D/QVariant×3)（激活时） | **是**（materializeToQVariant） | float3 一次性物化为 QDataPoint3D；`m_numericCacheActive=false`；m_points append |
| insert/replace（激活时） | **是**（失效定案） | 物化回退后再操作 m_points |
| remove（激活时） | 否（**增量维护**） | m_numericCache.remove(index)（O(n) 移动）；`m_worldCache.clear()`；emit dataChanged |
| clear | 否 | 双缓存清空；`numericCacheActive=true`（复位） |
| at(i)（激活时） | 否（**单点物化**） | 无持久状态（仅临时 QDataPoint3D） |
| points()（激活时） | 否（**按需物化视图**） | `m_pointsView` 懒重建；`m_pointsViewValid=true`（数据变更置 false） |

**materializeToQVariant 内部**：m_points.clear+reserve → float3 逐点转 QDataPoint3D(qreal) → m_numericCache.clear → numericCacheActive=false → 视图失效。一次性 O(N)。

## 时序（触发时机与先后）

1. **写入即分发**：每次 append 按当前激活态决定路径——**首个 QVariant 路径 append 是激活→回退的转折点**（此后永久 QVariant 直至 clear）。
2. **渲染读取**：GL 路径直读 `numericCache`（免物化）；QPainter 路径经 `at()`（单点物化）/`points()`（视图物化）——物化成本与访问粒度对应。
3. **失效传播**：数据变化 → `m_worldCache.clear()` + emit dataChanged → Layer3D worldCache 置脏 → collectPrimitives 重建。
4. **性能语义**（D28/A3）：全数值型 append（QValueAxis 场景）全程 12B/点、零物化；混合后回退是**一次性** O(N)（此前数据物化），后续走 QVariant。

## 边界与陷阱

1. **顺序语义**：append(QDataPoint3D) 在激活态先物化再追加——保证 m_points 顺序 = 追加顺序（回退不破坏既有数据序）。
2. **float3 精度**：qreal→float 舍入（O1 观察：非精确可表示 double 如 0.1 → 0.10000000149…）——设计取舍，QVariant 路径精确。
3. **remove 增量 vs replace 失效**：不对称是有意为之（remove 可保持数值型；replace 语义复杂故回退）——t41 审查逐项核验。
4. **points() 视图生命周期**：m_pointsView 仅在 numericCacheActive 且变更时重建——外部持有引用需注意失效（返回 const ref）。

## 关联

- Called By：用户/demo（写入）；collectPrimitives 各子类（at 读取）；GL buildBatches（numericCache/worldCache 直读）。
- 深挖：docs/series/deepdive_projectFn3D.md（闭包+双存储）；内存预算：design_phase3 §7.1/§9（audit B3 分列两缓存）。
- 相关决策：D28（float3 权威存储）、t51（实现）/t41（审查）。
