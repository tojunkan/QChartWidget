# deepdive_axes3d.md —— QChartAxes3D 编排与 tick 复用

> t53 核心计算深挖 · axes 模块
> 主题：3D 参照系编排器（D24）如何"组合复用 2D Axis"——Numeric 几何产出的全推导、tick 委托链、Box/Lattice 网格模式。

---

## 0. 问题背景

3D 需要"轴参照系"（盒 8 角/12 边/spine/刻度锚点），但不能把 2D Axis 的绘制接口搬进 3D（`drawAtEdge/drawAtPosition` 语义不兼容）。D24 定案：**QChartAxes3D = 非 Q_OBJECT 编排器**，组合持有 `QChartAxis*`，复用其刻度生成/标签格式化/样式，只产 Numeric 空间几何；toWorld/投影（Layer3D）与绘制（Renderer）分层外包。

## 1. 几何推导：盒/边/spine/锚点

约定 Numeric 数据盒 `{dataMin, dataMax}`（QVector3D）。

- **8 角**（`boxCorners`）：`index = u | (v<<1) | (w<<2)`，bit0=u、bit1=v、bit2=w，**置位取 dataMax 分量、清零取 dataMin 分量**：
  ```
  corner(i) = (bit0(i) ? dataMax.x : dataMin.x,
               bit1(i) ? dataMax.y : dataMin.y,
               bit2(i) ? dataMax.z : dataMin.z)
  ```
- **12 边**（`boxEdges`，角索引对）：u∥ `(0,1)(2,3)(4,5)(6,7)`；v∥ `(0,2)(1,3)(4,6)(5,7)`；w∥ `(0,4)(1,5)(2,6)(3,7)`。
- **3 条强调 spine**（`spineEdgeIndices`）：从 min 角 (0) 出发的 `{u∥边0, v∥边4, w∥边8}`。
- **刻度锚点**（`tickAnchor(dim, tickValue, dataMin)`）：dataMin 的 dim 分量替换为 tickValue（落在 min 角 spine 边上）——刻度点在 Numeric 空间定位，后续由 Layer3D 统一 toWorld。

## 2. tick 复用链（委托，非复制）

```
Layer3D::ticksForDim(dim)                      # src/layers/3d/QChartLayer3D.cpp
  └─ 校验 hasValidAxesDataBox()（min≤max 且 min≠max；默认 (0,0,0)=(0,0,0) → 无效 → 空）
  └─ 取 dim 分量范围 [lo, hi]（m_axesDataMin/Max，Widget3D 注入：dataBounds3D 或 A9 锚定域盒）
  └─ QChartAxes3D::ticks(dim, lo, hi)
       └─ axis(dim).axis->tickValues(lo, hi)   # ★ 直接复用 2D Axis 的刻度算法
       └─ axis 为 null → 返回空（该维不生成刻度）
QChartAxes3D::tickLabelTexts(dim, ...)
  └─ axis->tickLabels(ticks)                   # 复用 2D 标签格式化（QValueAxis "%.2f" 等）
```

收益：3D 刻度与 2D **同一算法、同一样式体系**（tickCount 语义、子刻度、格式化一致），编排器零复制、零漂移。

## 3. 网格模式：Box / Lattice（`QChartLayer3D::GridMode`，默认 Box）

| 模式 | 网格语义 | 图元来源 |
|---|---|---|
| Box | 完整盒式网格（6 面 12 边 + 三轴刻度线） | boxEdges + 每轴刻度锚点连线 |
| Lattice | 简化格栅（每轴刻度点连线成格） | 刻度锚点对连 |

网格图元与系列图元统一深度排序；Grid 图元 `depth -= kGridDepthBias`（=1e-3，src/core/QChartRenderer.h）保证**同深度系列优先**（D16/D24；GL 路径等价 glPolygonOffset，D29）。

## 4. 三层分离红线（reviewer grep 验证点）

`QChartAxes3D` 内**不允许**出现：`QPainter`、`QChartCamera3D`、`QChartProjection3D`（头文件无这些 include）——只产 Numeric 几何。职责边界：

```
Axis（2D）:  数值化 + 刻度 + 标签
QChartAxes3D: 盒/边/spine/刻度锚点（Numeric）    ← 本 deepdive
QChartLayer3D: toWorld + 投影 + 图元收集（含深度）
Renderer:    绘制 / 拾取
```

## 5. 边界与陷阱

1. **无效数据盒**：`hasValidAxesDataBox()` 失败（min==max 或 min>max）→ 不生成任何轴/网格图元（现有直接组装场景零影响）；Widget3D 用 A9 锚定域盒兜底（dataBounds3D 无效时）。
2. **轴重绑同步**：`setAxisX/Y/Z` 重绑时同步 `m_axes3D->axis(dim).axis`，并置 worldCache 脏（投影/轴/数据变化 → collectPrimitives 重建）。
3. **null 轴语义**：`axis(dim).axis == nullptr` → 该维不生成刻度/标签（非崩溃），AxisConfig 其余字段（markerSizePx/labelOffsetPx）仍生效。
4. **角索引约定**：bit 置位取 max——调用方（Layer3D 边生成）必须与 `boxCorners` 的索引约定一致，否则边会连错角。
5. **tick 范围**：ticks 委托的是"该维 Numeric 范围"（lo,hi），不是 dataBounds3D 全盒——每维独立范围（Z 轴仅 toNumeric、X/Y 复用基类）。

## 6. 单测对照

| 锚点 | 断言 |
|---|---|
| TestQChartAxes3D | boxCorners 索引约定（bit 置位=max）、12 边角对、spine 索引、tickAnchor 分量替换、null 轴空刻度 |
| TestQChartMath / TestQChartCamera3D | 锚点经 toWorld+投影后落位正确（正交俯视对照 2D） |
| TestQChartRenderer3D | 网格/刻度图元深度（Grid 偏置后系列优先）、Box/Lattice 两模式图元集合 |

> 回归口径：ctest 180 PASS + 2 SKIP 全绿；改动几何约定（角索引/边对/spine）必须重跑 TestQChartAxes3D 与渲染对照测试。
