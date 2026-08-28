# QChartLayer3D_collectPrimitives_flow.md —— 分层收集四段链

> t55 核心函数 flow · layers 模块（src/layers/3d/QChartLayer3D.cpp `collectPrimitives`）★3D 渲染主路径（QPainter 3D 子路径 + GL collectScene 共用）

## 控制流（调用图）

```
Renderer（QPainterChartRenderer::drawForeground3D / QOpenGLChartRenderer::collectScene）
  └─ layer3D->collectPrimitives(cam, plotArea, out, labels=nullptr)
       ├─ ① worldCache 重建（m_worldCacheDirty 时，§9 失效策略）:
       │    数值型系列：worldCache = toWorld(numericCache)      # 数值已预转换（免 QVariant）
       │    曲面系列：   worldCache = toWorld(toNumeric(grid))   # 行主序 rows·cols
       │    混合（QVariant）非曲面：不填（Phase 2 边界，走全链闭包）
       │    重建后 m_worldCacheDirty = false
       ├─ ② 轴/网格图元（hasValidAxesDataBox 守卫）:
       │    Grid 层：Box 模式（盒底面 tick 对齐网格：u/v 向 emitLine）
       │              Lattice 模式（晶格三族：族 U/V/W 全交叉 emitLine）
       │    ForegroundDecor 层：盒边（boxEdges）/spine（spineEdgeIndices）/刻度点（tickAnchor → 投影）
       │    labels 出参：tickLabelTexts → QChartTextLabel（tick 标签 + 轴标题）
       ├─ ③ 系列图元（Series 层）:
       │    fn = makeProjectFn(cam, plotArea)
       │    for s ∈ m_series3D（可见）: s->collectPrimitives(fn, out)     # 散点/线/曲面各自规则
       └─ ④ labels（可选出参，billboard 源；GL overlay / QPainter drawLabels 同源）
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `cam`（相机，投影/深度来源）、`plotArea`、`labels`（可选出参） |
| 中间量 | worldCache（VBO 源，重建于①）；dimTicks（axes3D 委托）；tickAnchor（Numeric 锚点 → 投影） |
| 出参 | `out`（图元列表：Grid/Series/ForegroundDecor 分层，depth/dataIndex 已填）；`labels`（billboard） |
| 状态变更 | `m_worldCacheDirty` true→false（重建后）；各系列 worldCache 填充 |

**分层深度语义**（与 renderer 配合）：Grid 图元 depth 减 kGridDepthBias（Renderer 应用，QChartLayer3D 不自行减——分层在 emitLine 的 layer 字段）；Series 原始 depth；ForegroundDecor 恒后画（renderer 分桶）。详见 docs/layers/deepdive_layerDepth.md。

## 时序（触发时机与先后）

1. **脏驱动**：投影变化（setProjection3D）/轴重绑（setAxisX/Y/Z）/数据变化（hookSeriesDirty → dataChanged）/系列增减 → `m_worldCacheDirty=true` → 下次 collectPrimitives 重建（**非每帧**——相机变化不触发，A5 静态 World 几何）。
2. **顺序固定**：①worldCache → ②轴/网格 → ③系列 → ④labels——系列图元依赖 makeProjectFn（与 worldCache 无关，闭包路径）。
3. **消费方**：QPainter 路径 → drawForeground3D 分桶排序绘制；GL 路径 → collectScene 缓存图元 + pickTable 对齐（Q_ASSERT 增量校验）→ uploadBatches。

## 边界与陷阱

1. **无效数据盒**：hasValidAxesDataBox() false → ②段整体跳过（轴/网格零图元；系列仍收集——直接组装场景零影响）。
2. **快速通道**：emitLine 内 identity 判定（isIdentityMapping → Numeric≡World 直通 + 段数=samplingSegmentsHint）——Cartesian3D 网格/刻度免 toWorld。
3. **worldCache 与闭包并存**：worldCache 供 VBO（GL），collectPrimitives 仍走全链闭包（QPainter/拾取）——两路径同源（design_phase3 §9 裁决 b）。
4. **标签出参**：labels 指针非空才填（GL overlay 源）；nullptr 调用方（如仅拾取收集）跳过。

## 关联

- Called By：QPainterChartRenderer::drawForeground3D（src/core/QPainterChartRenderer.cpp:188-192）/QOpenGLChartRenderer::collectScene。
- 深挖：docs/layers/deepdive_layerDepth.md（深度/偏置）；闭包：docs/layers/QChartLayer3D_makeProjectFn_flow.md；失效策略：design_phase3 §9。
- 相关决策：D15/D16/D24（分层编排）、A5（静态 World 几何免每帧收集）。
