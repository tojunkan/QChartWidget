# QChartAxes3D Documentation

## Brief Introduction:
QChartAxes3D 是 **3D 轴参照系编排器**（**非 Q_OBJECT** 的轻量编排/值容器形态，无事件/信号；本阶段由 QChartLayer3D 以 unique_ptr 持有）：职责仅两件——① 提供**盒几何**（8 角/12 边/主轴 spine 边索引的纯静态工具）；② 作为**轴配置容器**（AxisConfig[3]：axis 指针、visible、markerSizePx、labelOffsetPx、axisTitleVisible、axisTitle + 公开 `dataBounds: QCube`）。**刻度生成不做**——刻度委托给各轴 QChartAxis::tickValues/tickLabels（QChartLayer3D::collectPrimitives 内调用）。轴配置与三轴绑定经 QChartLayer3D::setAxisX/Y/Z 自动同步（axis(0..2) 指向 X/Y/Z）。`markerSizePx/labelOffsetPx/axisTitleVisible/axisTitle` 字段随标题/刻度点排版后续批次消费（当前 spine 绘制仅用 axis 指针与轴色/tickValues）。静态几何：boxCorners 按位序（x:bit0/y:bit1/z:bit2）生成 8 角；boxEdges 返回 12 条边（4 u∥、4 v∥、4 w∥）；spineEdgeIndices={0,4,8}（从角 0 出发的三条主轴边）。

## Constant Variables:
None.（`AxisConfig` 嵌套结构为类型定义非类常量：`{QChartAxis* axis=nullptr; bool visible=true; qreal markerSizePx=4.0; QPointF labelOffsetPx{0,0}; bool axisTitleVisible=true; QString axisTitle;}`）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QCube` | `dataBounds` | （public）轴/网格数据盒（QChartLayer3D::setDataBounds 写入；fitWorld 回退读取） | `QCube` | 默认构造无效盒（ctor 由 layer3D 初始化为默认盒） | `QCube` |
| `AxisConfig` | `m_cfg[3]` | （private）三轴配置（dim0/1/2 = X/Y/Z） | `AxisConfig` | 默认构造 | `QChartAxis` |
| `bool` | `m_visible` | （private）轴参照系整体可见 | `true`/`false` | `true` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartAxes3D` | 构造（default） | 无 | public | — | QChartLayer3D 构造（unique_ptr） | — |
| `AxisConfig&` | `axis` | 维度配置访问器（非 const/const 重载） | `int dim` | public | 引用 | QChartLayer3D（重绑/collect 读取） | — |
| `bool` | `visible` | 可见访问器（内联） | 无 | public | `true`/`false` | collect 守卫 | — |
| `void` | `setVisible` | 设置整体可见（内联） | `bool v` | public | — | 用户（经 layer3D 未转发，直接 axes3D()） | — |
| `static QVector<QVector3D>` | `boxCorners` | 8 角点：按位序（bit0=x→dataMax.x 否则 min；bit1=y；bit2=z）生成（cpp） | `const QVector3D& dataMin, const QVector3D& dataMax` | public | `QVector<QVector3D>` | `QChartLayer3D::collectPrimitives` | — |
| `static QVector<QPair<int,int>>` | `boxEdges` | 12 条边索引对：{0,1}{2,3}{4,5}{6,7}（u∥）、{0,2}{1,3}{4,6}{5,7}（v∥）、{0,4}{1,5}{2,6}{3,7}（w∥） | 无 | public | `QVector<QPair<int,int>>` | `QChartLayer3D::collectPrimitives` | — |
| `static QVector<int>` | `spineEdgeIndices` | 主轴边索引 {0,4,8}（从角 0 出发的三条边，与 boxEdges 顺序对应） | 无 | public | `QVector<int>` | `QChartLayer3D::collectPrimitives`（spine 着色/线宽） | — |

Notes:
- 旧 docs/axes/QChartAxes3D.md 描述重构前"自绘刻度/流"形态已作废；本类现为编排器容器（几何+配置），绘制与刻度委托 layer3D/QChartAxis。
- 12 边维度归属（layer3D 推断）：边序号 <4 → dim0、<8 → dim1、否则 dim2（供轴色/spine 判定）。
- 无 Q_OBJECT → 无 moc、无信号（QCHART_SOURCES 含其 cpp）。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
