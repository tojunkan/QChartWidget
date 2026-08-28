# QChartSurfaceSeries_setGrid_flow.md —— 行主序校验与参数域生成

> t55 核心函数 flow · series 模块（src/series/3d/QChartSurfaceSeries.cpp）

## 控制流（调用图）

```
用户/demo（surface3d 球面/莫比乌斯）
  ├─ setParametricGrid(rows, cols, u0, u1, v0, v1)     # 便捷生成 (u,v) 格点
  │    ├─ pts.reserve(rows·cols)
  │    ├─ for r ∈ [0,rows):  v = rows>1 ? v0+(v1−v0)·r/(rows−1) : v0      # 参数域行扫描
  │    │     for c ∈ [0,cols): u = cols>1 ? u0+(u1−u0)·c/(cols−1) : u0    # 参数域列扫描
  │    │           pts.append(QDataPoint3D(u, v, QVariant()))              # z 未用（2→3 嵌入）
  │    └─ setGrid(rows, cols, pts)
  └─ setGrid(rows, cols, pts)
       ├─ 校验：rows<0 || cols<0 || pts.size() != rows·cols
       │    → qWarning + return（★ 原子性：不改变任何状态）
       ├─ m_rows = rows; m_cols = cols
       ├─ setPointsInternal(pts)      # QVariant 路径：m_points 权威、numericCache 失效、worldCache 失效（不发信号）
       ├─ emit gridChanged()
       └─ emit dataChanged()          # → Layer3D worldCache 置脏 → 重建
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| setParametricGrid 入参 | rows/cols（网格尺寸）+ u0,u1,v0,v1（参数域 [u0,u1]×[v0,v1]——通常 0..360 × −0.5..0.5 等） |
| setGrid 入参 | rows/cols + pts（**行主序**：r 行 c 列 → pts[r·cols+c]） |
| 校验 | `pts.size() == rows·cols`（严格相等；失败原子忽略） |
| 状态变更 | m_rows/m_cols；m_points（QVariant 权威）；numericCache 失效（setPointsInternal 置 inactive）；worldCache 失效；gridChanged + dataChanged |
| 出参（下游） | collectPrimitives：u 向 rows·(cols−1) 段（r·cols+c ↔ r·cols+c+1）+ v 向 cols·(rows−1) 段（r·cols+c ↔ (r+1)·cols+c） |

**行主序索引约定（一致性契约）**：`idx(r,c) = r·cols + c`——setGrid 的 pts 布局、collectPrimitives 的段端点、worldCache 的填充顺序（Layer3D）必须同用此约定（改动必须同步）。

## 时序（触发时机与先后）

1. **一次性设置**：网格数据通常构造/初始化时设置一次（静态曲面）；动态更新（如参数动画）每次 setGrid/setParametricGrid 全量替换。
2. **失效链**：setGrid → gridChanged + dataChanged → Layer3D worldCache 置脏 → collectPrimitives 重建（u/v 向段端点索引基于新 rows/cols）。
3. **参数域生成即用**：setParametricGrid 是纯便捷（生成 pts 后走 setGrid）——不绕开校验。

## 边界与陷阱

1. **校验原子性**：尺寸不符 → 完全忽略（rows/cols/数据均不变）——调用方重试安全（防半更新状态）。
2. **rows/cols = 1 退化**：参数域生成时 rows>1/cols>1 才插值（单行/单列取端点值）——网格退化时 collectPrimitives 段数公式仍成立（0 段）。
3. **z 未用**：setParametricGrid 生成 z=QVariant()（空）——2→3 曲面嵌入由投影 lambda 消费 (u,v)；若投影需要 z（3→3 用法）应直接用 setGrid 提供全三元组。
4. **QVariant 路径固定**：曲面数据总是走 QVariant 路径（setPointsInternal 强制）——numericCache 不参与曲面（worldCache 由 toWorld(toNumeric(grid)) 填充，见 deepdive_projectFn3D §4）。

## 关联

- Called By：用户/demo（setGrid/setParametricGrid）；collectPrimitives（段端点）。
- 段数公式与图元字段：docs/series/QChartSurfaceSeries.md；行主序 worldCache：docs/series/deepdive_projectFn3D.md §4。
- 相关决策：design_3d §6.5（曲面线框，行主序网格）、D28（worldCache VBO 源）。
