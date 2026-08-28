# QChartSurfaceSeries Documentation

## Brief Introduction:
3D 曲面线框系列（QChartSeries3D 派生）：Data 层 = 网格 `QVector<QDataPoint3D>`（**rows×cols 行主序**）；`setParametricGrid` 便捷生成 (u,v) 格点（z 未用，参数域 [u0,u1]×[v0,v1]）。World 层缓存 m_worldCache（基类，行主序 rows·cols，Layer3D 填充 = VBO 源）；collectPrimitives 仍走全链闭包 ProjectFn3D。**图元规则**：线框 `rows·(cols−1) + cols·(rows−1)` 条 LineSegment（u 方向行内相邻列 + v 方向列内相邻行），任一端投影非有限 → 跳过。信号：gridChanged（+ dataChanged）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `int` | `m_rows` | 网格行数（行主序）。 | `int ≥ 0` | `0` | — |
| `int` | `m_cols` | 网格列数。 | `int ≥ 0` | `0` | — |

Notes:
- 网格数据本身存于基类双存储（setGrid 走 QVariant 路径 → m_points 权威、numericCache 失效、worldCache 失效）。
- 有效判定：m_rows≤0 或 m_cols≤0 → collectPrimitives 空。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartSurfaceSeries` | 构造函数。 | `name`, `parent` | public | — | 用户/demo（surface3d） | — |
| `void` | `setGrid` | **行主序校验**：`pts.size() != rows·cols` → qWarning + **忽略（不改变状态）**；否则置 m_rows/m_cols + setPointsInternal（QVariant 路径，不发数据信号）+ emit gridChanged + dataChanged。 | `int rows, int cols` <br> `const QVector<QDataPoint3D>& pts` | public | — | 用户/demo | `QDataPoint3D` |
| `int` | `rows` / `cols` | 网格尺寸访问器（内联）。 | 无 | public | `int` | 渲染/测试 | — |
| `void` | `setParametricGrid` | 便捷生成：`v = v0+(v1−v0)·r/(rows−1)`（rows>1）、`u = u0+(u1−u0)·c/(cols−1)`（cols>1）；z=QVariant() 未用 → setGrid。 | `int rows, int cols` <br> `qreal u0, u1, v0, v1` | public | — | 用户/demo（球面/莫比乌斯 lambda 前驱） | — |
| `void` | `collectPrimitives` | **线框规则**：全链闭包预投影全部网格点（valid 标记）；u 方向 `rows·(cols−1)` 条（行内 r·cols+c ↔ r·cols+c+1）+ v 方向 `cols·(rows−1)` 条（列内 r·cols+c ↔ (r+1)·cols+c）；任一端 invalid → 跳过；depth=两端均值、dataIndex=起点、penWidth=1.0。 | `const ProjectFn3D& projectFn` <br> `QVector<QChartPrimitive>& out` | public override | — | Renderer collectScene / Layer3D | `QChartPrimitive` <br> `ProjectFn3D` |
| `void` | `draw` | 直绘（线框）。 | `QPainter*` <br> `const ProjectFn3D&` <br> `const DrawContext3D*` | public override | — | 调试/demo | — |

Notes:
- **段数公式**：rows·(cols−1)（u 向）+ cols·(rows−1)（v 向）——与网格拓扑一一对应（GL 路径 Line 批次 2 顶点/段）。
- setGrid 校验失败**原子性**：不改变任何状态（含 rows/cols）——调用方重试安全。
- 完整流程（校验/参数域生成）：docs/series/QChartSurfaceSeries_setGrid_flow.md。

## Overrided Qt Events:
None.

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `gridChanged` | 网格结构变化（rows/cols/数据）。 | — | 外部按需连接（配合 dataChanged → Layer3D worldCache 置脏） | — |
