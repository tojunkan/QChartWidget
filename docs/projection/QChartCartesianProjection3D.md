# QChartCartesianProjection3D Documentation

## Brief Introduction:
3D 笛卡尔坐标投影（恒等）：`toWorld/fromWorld` 恒等（Numeric 空间 ≡ World 空间）——2D QCartesianProjection 的 3D 对应。**恒等快速通道**（design_3d_axes §5.4，D23）：`samplingSegmentsHint()=2`（直线两点）、`isIdentityMapping()=true`（反算 dataBounds 免采样、图元生成免 toWorld）。header-only，无状态，无 Q_OBJECT。2D/3D 双重恒等 → **正交俯视 ≡ 2D 映射**（D-3D-2 硬验收）成立的基础。

## Constant Variables:
None.

## Member Variables:
None.（无状态）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartCartesianProjection3D` | 构造函数（轴名 "x"/"y"/"z"）。 | 无 | public | — | `QChartWidget3D`（默认 3D 投影占位，构造时序）/用户 | `QChartWidget3D` |
| `QVector3D` | `toWorld` | 恒等：Numeric → World。 | `qreal n0, qreal n1, qreal n2=0.0` | public override | `QVector3D(n0,n1,n2)` | Layer3D 闭包/Widget3D 反算 | — |
| `QVector3D` | `fromWorld` | 恒等：World → Numeric。 | `const QVector3D& w` | public override | `w` | Widget3D 5³ 反算（recomputeDataBounds3D 快速通道） | `QChartWidget3D` |
| `int` | `samplingSegmentsHint` | 段数提示（恒等 → 2 点直线）。 | 无 | public override | `2` | `QChartLayer3D::emitLine`（免分段） | `QChartLayer3D` |
| `bool` | `isIdentityMapping` | 恒等快速通道（true）。 | 无 | public override | `true` | `QChartWidget3D::recomputeDataBounds3D`（免采样直取盒）/Layer3D emitLine（免 toWorld） | `QChartWidget3D` <br> `QChartLayer3D` |

Notes:
- computeWorldBounds 未覆盖：基类 16³ 采样恒等也正确（恒等映射下采样结果 == 直接取盒），性能上可由消费方快速通道跳过（fitWorld 链仍调用基类采样——恒等时结果精确）。
- 默认 3D 投影：QChartWidget3D 构造先 setProjection3D(QChartCartesianProjection3D) 占位满足基类流程（详见 QChartWidget3D.md 构造时序）。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
