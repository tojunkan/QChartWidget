# QChartProjection3D Documentation

## Brief Introduction:
3D 坐标投影基类（header-only，D-3D-5）：Numeric 空间 ↔ World 空间双向映射 + World 包围盒（采样法）+ 3D 路径生成。3D 链路：`Data ─[Axis::toNumeric]→ Numeric ─[Projection3D::toWorld]→ World(x,y,z) ─[Camera::viewProjectionMatrix]→ Clip ─[QChartMath]→ Screen`。与 2D QChartProjection 同构（2D 家族零改动）。本头还定义共享类型 **`QChartWorldBox{min,max}`**（World 轴对齐盒；归属 QChartCamera3D.h 但此处先行定义，禁止重复定义）。子类：QChartCartesianProjection3D（恒等+快速通道）/QChartCylindricalProjection3D/QChartSphericalProjection3D/QChartFunctionalProjection3D。

## Constant Variables:
None.（采样常数 grid=16、segments=64 为函数内局部）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QString` | `m_name0` | dim0 轴名（"x"/"r"/"u" 等，dimensionName 返回）。 | `QString` | 子类构造传入 | — |
| `QString` | `m_name1` | dim1 轴名。 | `QString` | 子类构造传入 | — |
| `QString` | `m_name2` | dim2 轴名。 | `QString` | 子类构造传入 | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartProjection3D` | 构造函数（三轴名，默认 "x"/"y"/"z"）。 | `QString name0="x"` <br> `QString name1="y"` <br> `QString name2="z"` | public | — | 子类构造 | — |
| `QString` | `dimensionName` | 维度名访问器（越界返回空）。 | `int dim` | public | `QString`/空 | 轴标题/QChartAxes3D | — |
| `QVector3D` | `toWorld` | **纯虚**：Numeric (n0,n1,n2) → World；NaN/Inf 自然传播，调用方跳过。 | `qreal n0, qreal n1, qreal n2=0.0` | public pure virtual | `QVector3D` | Layer3D 闭包（makeProjectFn）/computeWorldBounds 采样 | `QChartLayer3D` |
| `QVector3D` | `fromWorld` | **纯虚**：World → Numeric（反向；奇点 NaN 策略延续）。 | `const QVector3D& w` | public pure virtual | `QVector3D` | Widget3D 5³ 反算（recomputeDataBounds3D） | `QChartWidget3D` |
| `QChartWorldBox` | `computeWorldBounds` | **16³=4913 点采样** toWorld → 有限点 min/max；**全 NaN → 回退 {dataMin,dataMax}**。子类可覆盖（Functional3D 传 boundsFn；Cartesian3D 未覆盖——恒等采样仍正确但可走快速通道）。 | `const QVector3D& dataMin` <br> `const QVector3D& dataMax` | public virtual | `QChartWorldBox` | `QChartWidget3D::fitWorld`（A3 链，QChartWidget3D.cpp:236） | `QChartWorldBox` <br> `QChartWidget3D` |
| `std::pair<QVector3D,QVector3D>` | `defaultDataBounds` | 默认 Numeric 范围（与 2D QRectF(0,0,10,10) 对齐）。 | 无 | public virtual | `{(0,0,0),(10,10,10)}` | `QChartWidget3D::fitWorld`（A3 链 resolveDataBox） | `QChartWidget3D` |
| `int` | `samplingSegmentsHint` | 直线采样段数提示（弯曲投影 32；Cartesian3D 覆盖为 2）。 | 无 | public virtual | `32`（默认）/`2`（Cartesian3D） | `QChartLayer3D::emitLine` | `QChartLayer3D` |
| `bool` | `isIdentityMapping` | 恒等映射快速通道开关（默认 false；Cartesian3D 覆盖为 true）。 | 无 | public virtual | `true`/`false` | `QChartWidget3D::recomputeDataBounds3D`（免采样）/Layer3D emitLine（免 toWorld） | `QChartWidget3D` <br> `QChartLayer3D` |
| `QVector<QVector<QVector3D>>` | `createPath3D` | 数据曲线 → World 折线（**NaN 断路径**：t∈[0,1] 采样 → toWorld → 非有限断开重开子路径）。 | `std::function<QVector3D(qreal t)> dataCurve` <br> `int segments=64` | public | 子路径列表 | 库内无调用（测试/外部消费方；与 2D createPath 同构） | — |

Notes:
- **共享类型 QChartWorldBox**：`{QVector3D min, max}`——fit/反算/批次用（归属说明：design_3d §4.2 指定 QChartCamera3D.h；本头先行定义，Camera3D include 复用，禁止重复定义避免 ODR）。
- 快速通道定位：`isIdentityMapping()` 不在 computeWorldBounds 内部判定（基类恒采样 16³）——由消费方（Widget3D 反算/Layer3D emitLine）读取并跳采（见 deepdive_viewCube / QChartProjection3D_computeWorldBounds_flow.md）。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
