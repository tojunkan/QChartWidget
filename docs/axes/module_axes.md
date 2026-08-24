# module_axes.md —— axes 模块

> 属于 t53 文档套件；配套 deepdive：`docs/axes/deepdive_axes3d.md`（QChartAxes3D 编排与 tick 复用）。
> 2D 与 3D 同属本模块（对仗代码目录 `include/axes/{2d,3d}`），单篇分节。

## 1. 职责与边界

axes = **坐标轴家族**（2D 轴基类 + 四个子类 + 3D 参照系编排器）。核心分工（design_notes §Axis）：

1. **数值化**：`toNumeric(data)→qreal` / `fromNumeric(qreal)→data`（跨类型统一）；
2. **刻度生成**：`tickValues(min,max)`；
3. **标签格式化**：`tickLabels(ticks)`。

Axis **不管坐标映射**（没有 valueToNormalized/pan/zoom/coordinateSystem）；数据映射归投影（projection），视窗几何归相机（core）。3D 侧 `QChartAxes3D` 是**非 Q_OBJECT 编排器**：组合复用 2D Axis 的刻度/标签/样式，只产 Numeric 空间几何（三层分离红线：不 toWorld、不数值化、不绘制）。

- 依赖：`core`（QChartCamera 的 viewChanged；DrawContext 引用）、`projection`（QChartProjection）。
- 被依赖：`layers`（QChartLayer include QChartAxis；QChartLayer3D 持有 QChartAxes3D）、`core`（QChartWidget include 各轴）。

## 2. 文件与类清单

| 文件 | 类 | Q_OBJECT | 信号 | Q_PROPERTY |
|---|---|---|---|---|
| include/axes/QChartAxis.h + src/axes/QChartAxis.cpp | `QChartAxis`（基类）+ `DrawContext`（struct） | ✓ | rangeChanged / visibleChanged / styleChanged / tickCountChanged / subTickCountChanged | 6（visible/title/color/tickCount/subTickCount/alignment） |
| include/axes/2d/QValueAxis.h + src/axes/2d/QValueAxis.cpp | `QValueAxis` | ✓ | —（继承） | 3 |
| include/axes/2d/QLogAxis.h + src/axes/2d/QLogAxis.cpp | `QLogAxis` | ✓ | — | 1 |
| include/axes/2d/QDateTimeAxis.h + src/axes/2d/QDateTimeAxis.cpp | `QDateTimeAxis` | ✓ | — | 1 |
| include/axes/2d/QBarCategoryAxis.h + src/axes/2d/QBarCategoryAxis.cpp | `QBarCategoryAxis` | ✓ | — | 1 |
| include/axes/3d/QChartAxes3D.h + src/axes/3d/QChartAxes3D.cpp | `QChartAxes3D`（非 Q_OBJECT 编排器） | — | — | — |

## 3. 公共 API 一览

**QChartAxis（基类，2D/3D 共用）**
- 数值化：`toNumeric(QVariant)` / `fromNumeric(qreal)`（纯虚）；`toNumeric0/toNumeric1`（std::function 成员，DrawContext 组装用）。
- 刻度：`tickValues(min,max)`（纯虚）、`tickLabels(ticks)`（纯虚）、`subTickValues`（默认实现）。
- 绘制：`drawAtEdge(painter, ctx, dim)`（边框轴，Numeric 线性插值到 plotArea 边缘）、`drawAtPosition(painter, ctx, offset, ...)`、`sizeHint(font)`。
- 样式：`setRange(min,max)`（语法糖，仅 Cartesian）、`setVisible/setTitle/setColor/setThemeColor/clearColor`（D12 双槽模型）、`setTickCount/setSubTickCount`、`setAlignment`；`isInteractive()`。

**2D 子类（Data↔Numeric 语义表，design_notes §Data ≠ Numeric）**

| 子类 | Data 类型 | toNumeric | fromNumeric |
|---|---|---|---|
| QValueAxis | qreal | 恒等 | 恒等 |
| QLogAxis | qreal（>0） | log10(v) | pow(10, v) |
| QDateTimeAxis | QDateTime | toMSecsSinceEpoch() | fromMSecsSinceEpoch(v) |
| QBarCategoryAxis | QString | 类别索引 | categories[index] |

**QChartAxes3D（3D 编排器）**
- 每维配置槽：`axis(dim)` → `AxisConfig{axis*, visible, markerSizePx, labelOffsetPx, axisTitleVisible, axisTitle}`（dim∈{0,1,2}；组合持有 QChartAxis*，非持有所有权）。
- 总开关：`setVisible`（demo 'A' 键）。
- Numeric 几何（静态/委托）：`boxCorners(dataMin,dataMax)`（8 角，index=u|(v<<1)|(w<<2)）、`boxEdges()`（12 边）、`spineEdgeIndices()`（3 条强调 spine）、`ticks(dim,min,max)`（委托 axis->tickValues）、`tickLabelTexts`、`tickAnchor(dim, value, dataMin)`（min 角 spine 边上的刻度锚点）。

## 4. 信号槽表（谁连谁）

| 发送方 | 信号 | 接收方 | 动作 |
|---|---|---|---|
| QChartAxis | rangeChanged(min,max) | QChartWidget | 重算 dataBounds → fit → invalidate（2D 语法糖链路） |
| QChartAxis | visibleChanged / styleChanged / tickCountChanged / subTickCountChanged | QChartWidget | invalidateBackground()（轴变化重绘） |
| QChartCamera2D | viewChanged | QChartWidget | 反算 dataBounds + invalidate（轴范围联动） |

3D 侧：QChartAxes3D 非 QObject 无信号；轴/刻度变化经 Layer3D 的 worldCache 置脏（轴重绑 → `m_worldCacheDirty=true` → collectPrimitives 重建）。

## 5. 核心机制

1. **Axis 三件事**：数值化/刻度/格式化；不持有 min/max 作为映射基准（`setRange` 只是语法糖，触发 rangeChanged 让 Widget 重算）。
2. **边框轴捷径**（仅 Cartesian）：Numeric 直接线性插值到 plotArea 边缘 → Pixel，不经 Projection（design_notes §边框轴）。
3. **D12 双槽颜色模型**：显式 override（setColor）优先于主题默认（setThemeColor），`clearColor()` 回退主题；styleChanged 在 override 变化时发出。
4. **3D 三层分离**（D24）：QChartAxes3D 只产 Numeric 几何（盒/边/spine/tick 锚点），Layer3D 做 toWorld+投影，Renderer 绘制；编排器无 QPainter/QChartCamera3D/QChartProjection3D 引用（reviewer grep 验证点）。
5. **tick 复用**：3D 刻度值/标签直接委托 2D Axis 的 `tickValues/tickLabels`（同一算法、同一样式体系），见 deepdive_axes3d。

## 6. 核心函数一览表（含文件位置）

| 函数 | 职责 | 调用方 | 文件 |
|---|---|---|---|
| `QChartAxis::toNumeric/fromNumeric` | 跨类型数值化（每子类定义语义） | DrawContext/Layer/Widget | src/axes/QChartAxis.cpp + src/axes/2d/*.cpp |
| `QChartAxis::tickValues/tickLabels` | 刻度生成/标签格式化 | Widget（2D 边框轴）/ QChartAxes3D（3D 委托） | src/axes/QChartAxis.cpp + src/axes/2d/*.cpp |
| `QChartAxis::drawAtEdge/drawAtPosition` | 2D 边框轴/定位轴绘制 | QPainterChartRenderer | src/axes/QChartAxis.cpp |
| `QChartAxes3D::boxCorners/boxEdges/spineEdgeIndices` | 盒/边/spine 的 Numeric 几何 | QChartLayer3D::collectPrimitives | src/axes/3d/QChartAxes3D.cpp |
| `QChartAxes3D::ticks/tickLabelTexts/tickAnchor` | 刻度委托与锚点（Numeric） | QChartLayer3D（网格/刻度图元） | src/axes/3d/QChartAxes3D.cpp |

## 7. 设计文档对应

- 2D 轴职责/Data≠Numeric/边框轴：`docs/design/design_notes.md`（§Axis、§边框轴 vs 数据主脊、§特殊值处理）。
- 3D 参照系/编排器/tick 复用/Box-Lattice：`docs/design/design_3d_axes.md`（A1~A10、§5.4 快速通道、§8.2 编排器定案）。
- 决策：D12（颜色双槽）、D24（3D 参照系分层与编排）、D-3D-13（3D 悬停简化）。
