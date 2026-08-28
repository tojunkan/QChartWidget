# QChartAxes3D Documentation

## Brief Introduction:
3D 轴参照系编排器（D24 定案，**非 Q_OBJECT**）：组合持有 `QChartAxis*`（复用 2D 刻度生成/标签格式化/样式——非继承，2D drawAtEdge/drawAtPosition 语义与 3D 不兼容），只产 **Numeric 空间几何**（盒 8 角/12 边/spine/刻度锚点）。**三层分离红线**：本类不 toWorld/投影（Layer3D 做）、不数值化、不绘制——无 QPainter / QChartCamera3D / QChartProjection3D 引用（reviewer grep 验证点）。每维配置槽 `AxisConfig{axis*, visible, markerSizePx, labelOffsetPx, axisTitleVisible, axisTitle}`（dim∈{0,1,2}；axis=null → 该维不生成刻度/标签）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `AxisConfig` | `m_cfg[3]` | 每维配置槽（axis 组合复用非持有；其余样式字段）。 | `AxisConfig[3]` | `{axis=nullptr, visible=true, markerSizePx=4.0, labelOffsetPx={0,0}, axisTitleVisible=true}` | `QChartAxis` |
| `bool` | `m_visible` | 总开关（demo 'A' 键）。 | `true`/`false` | `true` | — |

Notes:
- `AxisConfig` 为类内 struct（axis/visible/markerSizePx/labelOffsetPx/axisTitleVisible/axisTitle）。
- 拥有权：QChartAxes3D 由 QChartLayer3D 持有（unique_ptr）；AxisConfig.axis 为非持有指针（Layer3D 重绑时同步）。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartAxes3D` | 构造函数（默认三配置槽空轴）。 | 无 | public | — | `QChartLayer3D` 构造（make_unique） | `QChartLayer3D` |
| `AxisConfig&` | `axis` | 每维配置槽访问器（内联；const 版）。 | `int dim` | public | `AxisConfig&` | `QChartLayer3D::setAxisX/Y/Z`（重绑同步）/用户（demo 样式） | `QChartLayer3D` |
| `bool` | `visible` | 总开关访问器（内联）。 | 无 | public | `true`/`false` | demo（'A' 键） | — |
| `void` | `setVisible` | 总开关设置（内联）。 | `bool v` | public | — | demo | — |
| `QVector<QVector3D>` | `boxCorners` | **静态**：盒 8 角（Numeric 空间）；`index = u\|(v<<1)\|(w<<2)`，bit 置位取 dataMax 分量。 | `const QVector3D& dataMin` <br> `const QVector3D& dataMax` | public static | 8 角列表 | `QChartLayer3D::collectPrimitives`（盒/边） | `QChartLayer3D` |
| `QVector<QPair<int,int>>` | `boxEdges` | **静态**：12 边角索引对（u∥(0,1)(2,3)(4,5)(6,7)；v∥(0,2)(1,3)(4,6)(5,7)；w∥(0,4)(1,5)(2,6)(3,7)）。 | 无 | public static | 12 边 | `QChartLayer3D::collectPrimitives` | `QChartLayer3D` |
| `QVector<int>` | `spineEdgeIndices` | **静态**：3 条强调 spine（min 角出发的 u/v/w 边）。 | 无 | public static | 3 索引 | `QChartLayer3D::collectPrimitives`（spine） | `QChartLayer3D` |
| `QVector<qreal>` | `ticks` | **委托**：`axis(dim)->tickValues(dimMin, dimMax)`（axis 为 null/dim 越界 → 空）。 | `int dim` <br> `qreal dimMin, qreal dimMax` | public | `QVector<qreal>` | `QChartLayer3D::dimTicks`（网格/刻度图元） | `QChartLayer3D` <br> `QChartAxis` |
| `QStringList` | `tickLabelTexts` | **委托**：`axis(dim)->tickLabels(ticks)`。 | `int dim` <br> `qreal dimMin, qreal dimMax` | public | `QStringList` | `QChartLayer3D::collectPrimitives`（标签） | `QChartLayer3D` <br> `QChartAxis` |
| `QVector3D` | `tickAnchor` | **静态**：dataMin 的 dim 分量替换为 tickValue（min 角 spine 边上的刻度锚点，Numeric）。 | `int dim` <br> `qreal tickValue` <br> `const QVector3D& dataMin` | public static | `QVector3D` | `QChartLayer3D::collectPrimitives`（刻度点） | `QChartLayer3D` |

Notes:
- 委托链（ticks/tickAnchor）与盒几何全流程：docs/axes/QChartAxes3D_ticks_flow.md。
- 角索引约定（bit 置位=max）是 Layer3D 边生成的一致性契约（改动必须同步，见 deepdive_axes3d §5）。
- 无信号（非 QObject）；轴/刻度变化经 Layer3D worldCache 置脏传播。

## Overrided Qt Events:
None.（非 QWidget）

## Signals:
None.（**非 Q_OBJECT**）
