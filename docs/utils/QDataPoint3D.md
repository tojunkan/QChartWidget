# QDataPoint3D Documentation

## Brief Introduction:
Data 空间 3D 点（值类型）：QVariant 三元组（任意 Axis 类型可用 u/v 可为 QDateTime 等）。**★定案（design_3d §6.1）：新建类，不扩展 QDataPoint**——扩展会改变 2D 数据类的内存/语义并触碰 2D 代码；本类与 QDataPoint 对称、零影响 2D。Data 层组织：Series 只存 Data（QVariant）；QVector3D 仅渲染时经投影产生（World 层）。非 QObject，header-only。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVariant` | `m_x` | dim0 数据（任意 Axis 类型）。 | `QVariant` | `{}`（空） | `QChartAxis` |
| `QVariant` | `m_y` | dim1 数据。 | `QVariant` | `{}`（空） | `QChartAxis` |
| `QVariant` | `m_z` | dim2 数据。 | `QVariant` | `{}`（空） | `QChartAxis` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QDataPoint3D` | 构造函数（QVariant 三元组默认空）。 | `QVariant x={}, y={}, z={}` | public | — | QChartSeries3D 数据/用户 | — |
| `QVariant` | `x`/`y`/`z` | 分量访问器（内联）。 | 无 | public | `QVariant` | QChartSeries3D::at/测试 | — |
| `void` | `setX`/`setY`/`setZ` | 分量设置（内联）。 | `QVariant v` | public | — | 用户 | — |

Notes:
- **值语义**：纯值类型；与 2D QDataPoint 对称（x/y 语义一致 + z）。
- **数据流**：Data（本类）→ 闭包（ProjectFn3D：axisX/Y/Z::toNumeric → projection3D::toWorld → camera.project）→ World/Screen——本类不参与任何映射（系列零耦合数据载体）。
- 曲面网格：setParametricGrid 生成 (u,v) 格点、z=QVariant()（空）——z 由投影 lambda 消费（2→3 嵌入，见 QChartSurfaceSeries.md）。
- 非类成员（无信号/事件）。

## Overrided Qt Events:
None.

## Signals:
None.（非 QObject）
