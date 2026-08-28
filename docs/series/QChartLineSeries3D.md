# QChartLineSeries3D Documentation

## Brief Introduction:
3D 折线系列（QChartSeries3D 派生）：**相邻有效点成 LineSegment**——任一端投影 screen 非有限 → 断段（延续 createPath 断路径语义）；`depth = 两端点深度均值`（裁决 a）；`dataIndex = 线段起点索引`（裁决 c）。Q_PROPERTY×2（lineWidth 默认 2.0 / cullingEnabled）。信号：lineWidthChanged/cullingChanged。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_lineWidth` | 线宽（px；Q_PROPERTY lineWidth）。 | `qreal` | `2.0` | — |
| `bool` | `m_cullingEnabled` | 粗筛剔除开关（Q_PROPERTY cullingEnabled）。 | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartLineSeries3D` | 构造函数。 | `name`, `parent` | public | — | 用户/demo（line3d） | — |
| `qreal` | `lineWidth` | 线宽访问器（内联）。 | 无 | public | `qreal` | 渲染/测试 | — |
| `void` | `setLineWidth` | 线宽 + emit lineWidthChanged。 | `qreal w` | public | — | 用户 | — |
| `void` | `setCullingEnabled` | 剔除开关 + emit cullingChanged。 | `bool v` | public | — | 用户 | — |
| `void` | `collectPrimitives` | **n−1 段规则**：全链闭包预投影全部点（{screen,depth} + valid 标记）；相邻对任一端 invalid → **断段跳过**；有效 → LineSegment{`a=p0.screen, b=p1.screen, depth=(d0+d1)/2, dataIndex=i（起点）, penWidth=lineWidth, color, worldA/B`}。 | `const ProjectFn3D& projectFn` <br> `QVector<QChartPrimitive>& out` | public override | — | Renderer collectScene / Layer3D | `QChartPrimitive` <br> `ProjectFn3D` |
| `void` | `draw` | 直绘（无排序）。 | `QPainter*` <br> `const ProjectFn3D&` <br> `const DrawContext3D*` | public override | — | 调试/demo | — |

Notes:
- 图元规则：**n 点 → n−1 条 LineSegment**（GL 路径 Line 批次 2 顶点/图元）；断段语义与 2D createPath 一致（NaN 断开）。
- 深度均值裁决：整段一个深度（排序键）——大跨段深度近似可接受（D16 排序语义）。
- 完整流程（断段/深度均值/dataIndex 起点）：docs/series/QChartLineSeries3D_collectPrimitives_flow.md。

## Overrided Qt Events:
None.

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `lineWidthChanged` | 线宽变化。 | — | 外部按需连接 | — |
| `cullingChanged` | 剔除开关变化。 | — | 外部按需连接 | — |
