# QChartSphericalProjection3D Documentation

## Brief Introduction:
3D 球坐标投影：dim0 = r（半径）、dim1 = θ（方位角，度）、dim2 = φ（仰角，度）。`toWorld(r,θ°,φ°)` = `(r·cosφ·cosθ, r·cosφ·sinθ, r·sinφ)`；`fromWorld(x,y,z)`：`r=√(x²+y²+z²)`、`θ=atan2(y,x)∈[0°,360°)`、`φ=asin(z/r)∈[-90°,90°]`（z/r clamp 防域外）；**奇点 r=0 处 θ/φ 均无定义 → (NaN, NaN, 0)**。header-only，无状态，无 Q_OBJECT。

## Constant Variables:
None.

## Member Variables:
None.（无状态）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartSphericalProjection3D` | 构造函数（轴名 "r"/"θ"/"φ"）。 | 无 | public | — | 用户/demo（scatter3d 球面采样） | — |
| `QVector3D` | `toWorld` | `(r·cosφ·cosθ, r·cosφ·sinθ, r·sinφ)`（θ/φ 度→弧度）。 | `qreal n0`（r） <br> `qreal n1`（θ°，度） <br> `qreal n2=0.0`（φ°，度） | public override | `QVector3D` | Layer3D 闭包（makeProjectFn）/computeWorldBounds 采样 | `QChartLayer3D` |
| `QVector3D` | `fromWorld` | `r=√(x²+y²+z²)`；`θ=atan2∈[0°,360°)`；`φ=asin(clamp(z/r,−1,1))`；**r≈0 → (NaN, NaN, 0)**。 | `const QVector3D& w` | public override | `QVector3D(r,θ°,φ°)` <br> `(NaN,NaN,0)`（奇点） | Widget3D 5³ 反算（recomputeDataBounds3D） | `QChartWidget3D` |

Notes:
- **φ 的 clamp**：`qBound(-1, z/r, 1)` 防浮点越界（|z/r| 略超 1 时 asin 域外）——数值稳定性细节。
- 奇点：r=0（原点）θ/φ 均无定义 → 双 NaN；全 NaN 反算 → Valid=false（A9 兜底）。
- 未覆盖快速通道（默认 32 段采样 / 非恒等）。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
