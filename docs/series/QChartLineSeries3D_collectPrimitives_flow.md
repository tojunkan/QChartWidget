# QChartLineSeries3D_collectPrimitives_flow.md —— 断段语义 + 深度均值 + dataIndex 起点

> t55 核心函数 flow · series 模块（src/series/3d/QChartLineSeries3D.cpp）

## 控制流（调用图）

```
Renderer collectScene / QChartLayer3D collectPrimitives
  └─ s->collectPrimitives(projectFn, out)          # projectFn = Layer3D 组装的 ProjectFn3D 全链闭包
       ├─ n = count()（双存储统一访问）；n < 2 → 返回
       ├─ 预投影阶段：for i ∈ [0,n): proj[i] = projectFn(at(i))     # Data→{screen,depth,world}
       │      valid[i] = isfinite(screen.x) && isfinite(screen.y)   # 投影 NaN（w≤0 等）标记
       ├─ 成段阶段：for i ∈ [0, n−1):
       │      p0=proj[i], p1=proj[i+1]
       │      !p0.valid || !p1.valid → continue        # ★ 断段（任一端 NaN）
       │      else → 追加 LineSegment{ a=p0.screen, b=p1.screen,
       │                               depth=(p0.depth+p1.depth)/2,   # 裁决 a：两端深度均值
       │                               dataIndex=i,                    # 裁决 c：线段起点索引
       │                               penWidth=m_lineWidth, color=color(),
       │                               worldA=p0.world, worldB=p1.world }  # GL 顶点源
       └─ out 追加全部有效段
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `projectFn`（Layer3D 组装：toNumeric×3 → toWorld → camera3D.project）；本系列状态（m_lineWidth/color/数据） |
| 中间量 | `proj[]`（预投影数组：{screen,depth,world} + valid 标记——**两阶段**：先全投影再成段，保证断段判定基于投影结果而非输入域） |
| 出参 | LineSegment 图元列表（≤ n−1 条）；字段全填（type/a/b/depth/dataIndex/penWidth/color/worldA/B） |
| 状态变更 | 无（纯收集；worldCache 由 Layer3D 另行填充） |

**关键裁决**：
- **断段**：投影 screen 非有限（相机后方 w≤0 等）→ 该端点所在两段都断开（延续 createPath NaN 断路径语义——跨奇点不画假连线）。
- **depth 均值**：整段一个深度作排序键——大跨段（两端深度差大）取均值近似；painter's algorithm 排序下段整体落位。
- **dataIndex = 起点 i**：hover/拾取定位到段起点数据（t47 拾取 dataIndex 语义同源）。

## 时序（触发时机与先后）

1. **重收集驱动**：数据变化（dataChanged → worldCache 置脏）或轴/投影/相机变化 → collectPrimitives 重建（非每帧——worldCache 机制）。
2. **投影先行**：全点预投影后才成段——两阶段保证 valid 判定一致（不存在"前点有效后点 NaN"的漏判）。
3. **图元下游**：QPainter 路径 → 排序（depth 降序）→ 绘制；GL 路径 → buildBatches（Line 批次 2 顶点/段，baseId 连续）。

## 边界与陷阱

1. **n<2 早退**：单点/空不产生段（无退化图元）。
2. **断段不合并**：跳过即缺口——不尝试跨 NaN 连接（视觉正确优先）。
3. **浮点 depth 均值**：两端深度一个 +Inf/NaN 时均值非有限——但 valid 判定已排除（screen NaN 与 depth NaN 同源 w≤0）。
4. **GL 一致性**：worldA/worldB 与 a/b 一一对应（VBO 顶点 = World 坐标，仅 u_viewProj 变换——D30）。

## 关联

- Called By：Renderer collectScene（src/core/QOpenGLChartRenderer.cpp）/QChartLayer3D。
- 曲面同款规则：docs/series/QChartSurfaceSeries.md（段数公式）；深挖：docs/series/deepdive_projectFn3D.md。
- 相关决策：D15（全链闭包）、D16（深度排序）、t42/t44（GL 顶点）。
