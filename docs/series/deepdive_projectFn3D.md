# deepdive_projectFn3D.md —— ProjectFn3D 全链闭包与 float3 权威存储

> t53 核心计算深挖 · series 模块
> 主题：D15 全链闭包如何让 3D 系列零耦合（Data→Numeric→World→Screen 在哪组装、如何注入），以及 D28/t51 的 float3 双存储（12B/点）与失效语义。

---

## 0. 问题背景

2D 系列通过 `toPixel(QVariant,QVariant)` 闭包与映射解耦；3D 必须延续同构设计（D15 红线）。同时 Phase 3 内存预算（A3：1M 点 ≤70MB）要求数值型系列不再以 QVariant 存储（50~70MB）而是 **float3 权威存储（12B/点）**。本 deepdive 拆解闭包的组装/注入链与双存储的切换语义。

## 1. 闭包定义与组装（谁组装、注入到哪）

```cpp
// include/series/3d/QChartSeries3D.h
using ProjectFn3D = std::function<QChartProjectedPoint(const QDataPoint3D&)>;
// 返回 QChartProjectedPoint{screen, depth, world}（world 原样回传，GL 路径 VBO 源）
```

**组装点 = QChartLayer3D**（`makeProjectFn`，src/layers/3d/QChartLayer3D.cpp）——全链闭包在层内完成：

```
QDataPoint3D ─[axisX/Y/Z::toNumeric]→ qreal×3 ─[projection3D::toWorld]→ QVector3D(World)
            ─[camera3D::project（viewProjectionMatrix·clip→clipToScreen+viewDepth）]→ QChartProjectedPoint
```

**注入点**：`QChartLayer3D::collectPrimitives` / GL `buildBatches`（src/core/QOpenGLChartRenderer.cpp）对每个系列调用：

```cpp
const ProjectFn3D fn = g->makeProjectFn(scene.camera3D, scene.plotArea);
for (QChartSeries3D* s : g->series3DList())
    s->collectPrimitives(fn, sp);          // 系列只消费 fn，不持 Axis/Projection/相机
```

**系列循环内**（QChartSeries3D 头注释约定的伪代码）：
```
for i in 0..count():
    p = projectFn(m_points[i])
    if !finite(p.screen): continue         # NaN 哨兵跳过（相机后方 w≤0 等）
    primitive{ depth = p.depth, dataIndex = i, ... }   # dataIndex 供悬停/拾取定位
```

## 2. 为什么这样设计

| 方案 | 问题 |
|---|---|
| 系列持 Axis/Projection 引用（×） | 系列耦合映射对象；D15/design_3d.md §6.2 红线：系列只存 Data |
| 系列自组装闭包（×） | 需要 Axis 数组 + Projection + 相机指针——同一信息 Layer 已有，重复持有破坏零耦合 |
| **Layer 组装、系列消费（✓）** | 映射知识集中在层（与 2D `toPixel` 同构）；系列可单测（喂假闭包）；多 Widget 共享系列数据副本（D18）时闭包各自组装 |

## 3. float3 权威存储（t51，D28 内存预算必要条件）

```
数值型路径：append(qreal×3)  → m_numericCache（QVector<QVector3D>，12B/点）权威
QVariant 路径：append(QVariant×3)/append(QDataPoint3D)/setPoints/insert/replace
                              → m_points（QDataPoint3D 列表权威）
```

**激活与回退语义**：
- 全部数值型 append → `numericCacheActive()==true`（float3 权威；GL 渲染直读，免 QVariant 解包）。
- **首个 QVariant 路径 append** → `numericCacheActive()==false`：此前 float3 **一次性物化**为 QDataPoint3D 回退（Phase 2 边界不变）。
- `replace/insert`（QVariant 参与）→ 失效回退；`remove` → 增量维护（保持数值型）；`clear` → 复位（numericCacheActive 回 true）。
- API 兼容：`points()/at()/count()` 语义与 Phase 2 完全一致——numeric-only 时 QVariant **按需物化**（`points()` 物化视图 / `at()` 单点物化）。

## 4. World 缓存（VBO 源，Layer3D 填充）

```
数值型系列：worldCache = toWorld(numericCache)      # QVector<QVector3D>
曲面系列：   worldCache = toWorld(toNumeric(grid))   # 行主序网格
混合系列：   空
```
- series **零耦合红线**：本类不持 Axis/Projection（`worldCache()` 可变引用由 Layer3D 填充，内部入口）。
- 失效：数据变化 → 本类自清 + Layer3D 置脏（`m_worldCacheDirty`）；投影/轴/相机变化 → Layer3D 置脏；collectPrimitives 重建后复位（src/layers/3d/QChartLayer3D.cpp）。

## 5. 边界与陷阱

1. **重载隐藏红线**（§6.2）：3D `draw(painter, projectFn, ctx3D)` 与基类 `draw(painter, toPixel, ctx)` 签名不同 → **隐藏而非覆盖**；基类桩 = qWarning + no-op——调用方必须持 `QChartSeries3D*`（Layer3D 经 `m_series3D` 遍历），误经 `QChartSeries*` 会响亮失败而非静默画错。
2. **NaN 哨兵**：`!finite(p.screen)` 跳过（相机后方 w≤0 → clipToScreen NaN）；绝不把负 w 当正常点。
3. **dataIndex 语义**：图元带 `dataIndex=i`（hover 定位 (u,v) 与 GPU 拾取 PickRecord 的索引源，D18/D27）。
4. **混合存储的代价**：QVariant 路径激活后 float3 物化是一次性 O(N) 开销；性能敏感场景应全数值型 append（QValueAxis 场景便捷重载）。
5. **双存储一致性**：`points()/at()/count()` 在任何激活状态下语义一致（按需物化），测试锁定（t41 审查项）。

## 6. 单测对照

| 锚点 | 断言 |
|---|---|
| TestQChartSurfaceSeries / TestQChartRenderer3D | collectPrimitives 图元字段（depth/dataIndex/类型）、NaN 段跳过、闭包注入后系列无映射引用 |
| TestQChartSeries3D（t51 相关） | float3 激活/回退（数值型→true；首个 QVariant→false 物化；remove 增量；clear 复位）；points()/at()/count() 语义一致 |
| TestQChartMath / TestQChartCamera3D | 闭包全链（toNumeric→toWorld→project）与逐点投影一致；正交俯视 ≡ 2D |
| GL 相关（TestQOpenGLRenderer） | worldCache → VBO 16B 顶点（World float3 + 颜色）；offscreen QSKIP，wayland/xcb 实跑 |

> 回归口径：ctest 180 PASS + 2 SKIP 全绿；修改闭包组装/双存储切换/物化语义必须重跑上述测试类（t41 审查曾逐项核验）。
