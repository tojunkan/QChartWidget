# QChartAxes3D_ticks_flow.md —— 委托链与盒几何（ticks / tickAnchor / boxCorners）

> t55 核心函数 flow · axes 模块
> 主题：QChartAxes3D 的 Numeric 几何产出——刻度委托链（复用 2D 算法）与盒/边/spine/锚点的角索引约定。

## 控制流（调用图）

```
QChartLayer3D::collectPrimitives（axesDataBox 有效时）
  ├─ dimTicks(dim)                         # 取 dim 分量 [lo,hi]（m_axesDataMin/Max）
  │    └─ QChartAxes3D::ticks(dim, lo, hi)
  │         ├─ dim 越界（<0||>2）→ 空
  │         ├─ axis(dim).axis == nullptr → 空（该维无刻度）
  │         └─ axis->tickValues(lo, hi)    # ★ 直接复用 2D 算法（同一刻度体系）
  ├─ tickLabelTexts(dim, lo, hi)
  │    └─ axis->tickLabels(ticks)          # 复用 2D 格式化
  ├─ 网格/刻度图元：
  │    ├─ Box 模式：盒底面 tick 对齐网格（emitLine Grid 层）
  │    ├─ Lattice 模式：晶格三族（emitLine）
  │    └─ 刻度点：tickAnchor(dim, tickValue, dataMin) → 投影 → 刻度点图元
  └─ 盒/边/spine：
       ├─ boxCorners(dataMin, dataMax)     # 8 角：index = u|(v<<1)|(w<<2)，bit 置位取 max
       ├─ boxEdges()                       # 12 边角对（u/v/w 各 4）
       └─ spineEdgeIndices()               # 3 条强调 spine（min 角出发）
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `dim ∈ {0,1,2}`；`dimMin/dimMax`（该维 Numeric 范围——Layer3D 轴盒注入，dataBounds3D 或 A9 锚定盒）；`dataMin/dataMax`（盒几何） |
| ticks 出参 | `QVector<qreal>`（Numeric 刻度值——委托 axis->tickValues，axis null/越界 → 空） |
| tickAnchor 出参 | `QVector3D`（dataMin 的 dim 分量替换为 tickValue——min 角 spine 边上的 Numeric 锚点） |
| 盒几何出参 | 8 角（bit 约定）/12 边（角对）/3 spine（索引） |
| 状态变更 | 无（纯查询/静态）；下游 Layer3D worldCache 置脏由轴重绑驱动 |

**角索引约定（一致性契约）**：`corner(i) = (bit0(i)?dataMax.x:dataMin.x, bit1(i)?dataMax.y:dataMin.y, bit2(i)?dataMax.z:dataMin.z)`——boxEdges/spine 的角对必须与此约定一致（改动必须同步，deepdive_axes3d §5）。

## 时序（触发时机与先后）

1. **重收集驱动**：轴重绑（setAxisX/Y/Z → axes3D 配置槽同步 + worldCacheDirty）或数据盒变化 → collectPrimitives 重建。
2. **先 ticks 后锚点**：ticks（Numeric 值）→ tickAnchor（锚点）→ 投影（Layer3D emitLine/刻度点）——锚点与刻度一一对应。
3. **每 collect 一次**：与 2D 每帧重算不同，3D 只在脏时重建（worldCache 机制，见 QChartLayer3D.md）。

## 边界与陷阱

1. **null 轴语义**：axis(dim).axis==nullptr → 该维不生成刻度/标签（非崩溃）；其余样式字段（markerSizePx/labelOffsetPx）仍生效。
2. **无效数据盒**：hasValidAxesDataBox() 失败 → 不生成任何轴/网格图元（现有直接组装场景零影响）。
3. **三层分离**：本链只产 Numeric 几何——toWorld/投影在 Layer3D，绘制在 Renderer（reviewer grep 验证点：无 QPainter/Camera3D/Projection3D 引用）。
4. **委托的隐藏成本**：3D 刻度 = 2D 算法（含 niceStep/chooseStep 分支）——同一体系保证刻度风格一致（D24 复用动机）。

## 关联

- Called By：QChartLayer3D::collectPrimitives（dimTicks/刻度锚点/盒边 spine）。
- 深度与偏置：docs/layers/deepdive_layerDepth.md；几何约定：docs/axes/deepdive_axes3d.md。
- 相关决策：D24（3D 参照系分层与编排）、design_3d_axes §8.2（编排器定案）、§5.4（快速通道）。
