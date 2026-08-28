# QChartAxis_tickValues_flow.md —— 刻度生成：niceStep（数据域）vs 对数域 vs 单位自适应

> t55 核心函数 flow · axes 模块（★全库最常调用函数之一：2D 边框轴每帧 + 3D 委托每 collect）
> 主题：`QValueAxis::tickValues`（含私有 `niceStep`）、`QLogAxis::tickValues`、`QDateTimeAxis::tickValues`、`QBarCategoryAxis::tickValues` 的域差异与全分支。

## 控制流（调用图）

```
2D：QPainterChartRenderer::drawBackground → 轴遍历 → drawAtEdge/drawAtPosition → tickValues + tickLabels
3D：QChartLayer3D::dimTicks(dim) → QChartAxes3D::ticks(dim, lo, hi) → axis->tickValues(lo, hi)   （委托链）
    └─ 消费：网格线（emitLine）、刻度锚点（tickAnchor）、刻度点

子类实现：
QValueAxis::tickValues(min, max)
  ├─ 退化区间（qFuzzyCompare）→ 单刻度 [min]
  ├─ m_tickInterval>0 ? 固定步长 : niceStep(range)（非法 → range/(tickCount−1) 兜底）
  ├─ first=ceil(min/step)·step；last=floor(max/step)·step
  ├─ first>last（对齐后空）→ 等分退化（count=max(2,tickCount)）
  ├─ 填充：v ∈ [first, last+0.0001·step]（v>max+0.0001·step break）
  └─ 保障 ≥2 刻度（<2 → [min, max]）

QLogAxis::tickValues(min, max)        # Numeric 已是 log 空间
  ├─ min>=max → [min]
  ├─ range<0.5 → step=0.25；<1.5 → step=0.5；否则 step=1（每数量级）
  ├─ first=ceil(min/step)·step；逐 step 填充（0.001·step 容差）
  └─ <2 → [min,max]

QDateTimeAxis::tickValues(min, max)   # Numeric = epoch 毫秒
  ├─ chooseStep(rangeMs) → stepMs（年→…→秒阶梯）
  ├─ start=(minMs/stepMs)·stepMs（<minMs 则 +stepMs——从 epoch 0 对齐）
  └─ 逐 stepMs 填充；<2 → [min,max]

QBarCategoryAxis::tickValues(_, _)    # 忽略区间：每类整数索引 0..size−1
```

## 数据流（入参/出参/状态变更）

| 子类 | 域 | 步长来源 | 对齐基准 | 出参 |
|---|---|---|---|---|
| QValueAxis | 数据域（qreal） | `m_tickInterval` 或 **niceStep**（range/targetTicks → 10 幂 × 漂亮因子 {1,1.5,2,3,4,5,8,10} 最近者） | ceil/floor 到 step 倍数 | `QVector<qreal>` |
| QLogAxis | log10 数量级域 | range 阶梯（1/0.5/0.25） | ceil 到 step 倍数 | `QVector<qreal>`（log 值） |
| QDateTimeAxis | epoch 毫秒 | chooseStep 单位阶梯（年/月/日/时/分/秒） | epoch 0 整数倍 | `QVector<qreal>`（毫秒） |
| QBarCategoryAxis | 类别索引 | 固定 1（每类） | 0 起 | `QVector<qreal>`（0..size−1） |

状态变更：无（纯查询；m_tickInterval/m_tickCount 为只读输入）。

## 时序（触发时机与先后）

1. **2D**：绘制帧内（drawAtEdge/drawAtPosition 调用时）——数据范围变化（viewRect/dataBounds 变）→ 重绘 → 重算刻度；**每帧可能多次**（每轴每帧）——"全库最常调用"成立。
2. **3D**：collectPrimitives 内 dimTicks（axesDataBox 变化/重收集时）——委托链 `Layer3D::dimTicks → QChartAxes3D::ticks → axis->tickValues`（同一 2D 算法，跨坐标系复用，D24）。
3. **先刻度后标签**：tickValues → tickLabels（格式化管道，见 QChartAxis_tickLabels_flow.md）；对齐/退化分支保证任何范围都有 ≥1 刻度输出。

## 边界与陷阱

1. **退化区间**（min≈max）：单刻度（不崩溃）；对齐后空区间 → 等分退化（保证 ≥2）。
2. **浮点容差**：`0.0001·step`（QValue）/`0.001·step`（QLog）防最后刻度因浮点被截断。
3. **对数域负/零**：Data≤0 的对数语义由 toNumeric 处理（NaN）；tickValues 只吃 log 值。
4. **niceStep 非法兜底**：range≤0/非有限 → step=1；计算后非法 → range/(tickCount−1)。
5. **QBarCategoryAxis 忽略入参**：刻度与区间无关（每类固定）——离散域特性。

## 关联

- Called By：QPainterChartRenderer::drawBackground（2D）/QChartLayer3D::dimTicks（3D 委托）。
- 标签管道：docs/axes/QChartAxis_tickLabels_flow.md；3D 委托链：docs/axes/QChartAxes3D_ticks_flow.md。
- 相关决策：design_notes §Axis（三件事：数值化/刻度/格式化）。
