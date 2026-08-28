# QChartCylindricalProjection3D Documentation

## Brief Introduction:
3D 柱坐标投影：dim0 = r（半径）、dim1 = θ（方位角，度）、dim2 = z（高度）。`toWorld(r,θ°,z)` = `(r·cosθ, r·sinθ, z)`；`fromWorld(x,y,z)`：`r=√(x²+y²)`、`θ=atan2(y,x)∈[0°,360°)`、`z=z`；**奇点 r=0 处 θ 无定义 → (NaN, 0, z)**（与 2D Polar 极点一致，z 保留）。header-only，无状态，无 Q_OBJECT。

## Constant Variables:
None.

## Member Variables:
None.（无状态）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartCylindricalProjection3D` | 构造函数（轴名 "r"/"θ"/"z"）。 | 无 | public | — | 用户/demo（line3d/scatter3d/surface3d） | — |
| `QVector3D` | `toWorld` | `(r·cosθ, r·sinθ, z)`（θ 度→弧度）。 | `qreal n0`（r） <br> `qreal n1`（θ°，度） <br> `qreal n2=0.0`（z） | public override | `QVector3D` | Layer3D 闭包（makeProjectFn）/computeWorldBounds 采样 | `QChartLayer3D` |
| `QVector3D` | `fromWorld` | `r=√(x²+y²)`；`θ=atan2∈[0°,360°)`；**r≈0 → (NaN, 0, z)**（z 保留）。 | `const QVector3D& w` | public override | `QVector3D(r,θ°,z)` <br> `(NaN,0,z)`（奇点） | Widget3D 5³ 反算（recomputeDataBounds3D） | `QChartWidget3D` |

Notes:
- 奇点语义：r=0（z 轴上任意点）θ 无定义 → NaN；下游（采样/反算）跳过非有限点——全 NaN 时 Widget3D 反算 Valid=false（A9 锚定盒兜底）。
- 未覆盖 samplingSegmentsHint/isIdentityMapping（默认 32/false——弯曲投影按 32 段采样）。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
