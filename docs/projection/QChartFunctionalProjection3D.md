# QChartFunctionalProjection3D Documentation

## Brief Introduction:
用户自定义 3D 坐标投影（免子类化，D-3D-5 家族）：lambda 定义 Numeric ↔ World 映射。统一支持两类用法：**2→3 参数曲面嵌入**（forward 忽略 n2，如莫比乌斯环 u∈[0,360)、v∈[−0.5,0.5]）与 **3→3 坐标变换**（forward 使用全部三 Numeric 分量）。`backward` 可不提供：nullptr 时 `fromWorld` 返回 NaN + qWarning。包围盒默认走基类 16³ 采样；可传 `boundsFn` 覆盖。header-only，无 Q_OBJECT。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `std::function<QVector3D(qreal,qreal,qreal)>` | `m_forward` | Numeric → World 映射（必传）。 | lambda / 空 | 构造传入 | — |
| `std::function<QVector3D(qreal,qreal,qreal)>` | `m_backward` | World → Numeric（可选；null → fromWorld NaN + qWarning）。 | lambda / `nullptr` | `nullptr` | — |
| `QVector3D` | `m_defaultDataMin` | 默认 Numeric 范围下限（Widget3D 首次 fit 用）。 | `QVector3D` | `(0,0,0)` | `QChartWidget3D` |
| `QVector3D` | `m_defaultDataMax` | 默认 Numeric 范围上限。 | `QVector3D` | `(1,1,1)` | `QChartWidget3D` |
| `std::function<QChartWorldBox(const QVector3D&, const QVector3D&)>` | `m_boundsFn` | 自定义包围盒计算（可选；null → 基类 16³ 采样）。 | lambda / `nullptr` | `nullptr` | `QChartWorldBox` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartFunctionalProjection3D` | 构造函数：forward 必传；backward/defaultDataMin/Max/boundsFn/name0~2 可选默认。 | `forward` <br> `backward=nullptr` <br> `defaultDataMin=(0,0,0)` <br> `defaultDataMax=(1,1,1)` <br> `boundsFn=nullptr` <br> `name0="u", name1="v", name2="w"` | public | — | 用户/demo（surface3d 球面/莫比乌斯环 lambda） | — |
| `QVector3D` | `toWorld` | 委托 m_forward；forward 空 → NaN + qWarning。 | `qreal n0, qreal n1, qreal n2=0.0` | public override | `QVector3D` | Layer3D 闭包（makeProjectFn）/computeWorldBounds 采样 | `QChartLayer3D` |
| `QVector3D` | `fromWorld` | 委托 m_backward；空 → NaN + qWarning。 | `const QVector3D& w` | public override | `QVector3D` | Widget3D 5³ 反算（若提供 backward） | `QChartWidget3D` |
| `QChartWorldBox` | `computeWorldBounds` | `m_boundsFn` 优先；否则基类 16³ 采样。 | `dataMin, dataMax` | public override | `QChartWorldBox` | `QChartWidget3D::fitWorld`（A3 链） | `QChartWorldBox` <br> `QChartWidget3D` |
| `std::pair<QVector3D,QVector3D>` | `defaultDataBounds` | 返回 m_defaultDataMin/Max。 | 无 | public override | `pair<QVector3D,QVector3D>` | `QChartWidget3D::fitWorld`（resolveDataBox） | `QChartWidget3D` |

Notes:
- 2→3 嵌入约定：n2 默认 0.0——`toWorld(u,v)` 即参数曲面点；曲面 lambda 直接可落地（demo_surface3d：球面/莫比乌斯）。
- 未覆盖 samplingSegmentsHint/isIdentityMapping（默认 32/false）；boundsFn 是包围盒精度的自定义点（如解析极值场景）。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
