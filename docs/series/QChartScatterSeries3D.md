# QChartScatterSeries3D Documentation

## Brief Introduction:
3D 散点系列（QChartSeries3D 派生）：**每点一个 Point 图元**（dataIndex=点索引；投影 screen 非有限 → 跳过）。`draw` 无排序直绘（收集后逐点画圆）。Q_PROPERTY×1（markerSize，默认 4.0px）。信号：markerSizeChanged。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_markerSize` | 标记半径（px；Q_PROPERTY markerSize）。 | `qreal` | `4.0` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartScatterSeries3D` | 构造函数。 | `name`, `parent` | public | — | 用户/demo（scatter3d） | — |
| `qreal` | `markerSize` | 标记尺寸访问器（内联）。 | 无 | public | `qreal` | 渲染/测试 | — |
| `void` | `setMarkerSize` | 标记尺寸 + emit markerSizeChanged。 | `qreal s` | public | — | 用户 | — |
| `void` | `collectPrimitives` | **每点一个 Point 图元**：`projectFn(at(i))`（双存储统一访问）→ screen 非有限跳过；填 `{a=screen, depth, dataIndex=i, markerSize, color, worldA}`。 | `const ProjectFn3D& projectFn` <br> `QVector<QChartPrimitive>& out` | public override | — | Renderer collectScene / Layer3D | `QChartPrimitive` <br> `ProjectFn3D` |
| `void` | `draw` | 无排序直绘（收集后逐点画圆）。 | `QPainter*` <br> `const ProjectFn3D&` <br> `const DrawContext3D*` | public override | — | 调试/demo | — |

Notes:
- 图元规则：**n 点 → n 个 Point 图元**（1 顶点/点）；GL 路径 Point 批次 1 顶点（gl_VertexID/1 = 图元 ID）。
- markerSizeChanged 触发重绘：经基类属性信号机制（3D 系列由 Layer3D 消费 + Widget invalidate）。

## Overrided Qt Events:
None.

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `markerSizeChanged` | 标记尺寸变化。 | — | 外部/demo 按需连接（3D 系列信号由 Widget3D/Layer3D 链路消费） | — |
