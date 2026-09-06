# QChartProjection Documentation

## Brief Introduction:
QChartProjection 是 **2D 坐标投影基类**（继承 QChartAbstractProjection；Numeric 空间维度 2 (x,y)）：在统一 vec3 接口之上保留 2D 双精度标量纯虚接口 `toCartesian(qreal num0, qreal num1)/fromCartesian(qreal x, qreal y)`（子类实现），并以 **final** 包装实现统一基类接口（z=0 折叠）、`dimension()=2`。扩展 2D 独有的**包络转换**（Pan/Zoom 用）：`computeDataBounds(viewRect)`（像素/视图 → Numeric 反算）、`computeViewRect(dataBounds)`（Numeric → View Cartesian 正算）纯虚与 `defaultDataBounds()`（默认 (0,0,10,10)，可覆写）。2D 派生：QCartesianProjection、QPolarProjection、QFunctionalProjection、QInterpolatedProjection（其余 3D 投影族派生 QChartProjection3D）。

## Constant Variables:
None.

## Member Variables:
None.（无新增成员；继承 m_dimNames）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartProjection` | 构造：传默认维度名 | `QString name0="x", QString name1="y"` | public | — | 派生类构造（QCartesianProjection 传 "x","y"；QPolarProjection 传 "θ","r"） | — |
| — | `~QChartProjection` | 虚析构（default） | 无 | public | — | — | — |
| `virtual QPointF` | `toCartesian` | **纯虚**：双精度标量版正向映射 | `qreal num0, qreal num1` | public | `QPointF` | 经 final vec3 包装被 CPU 渲染器调用 | — |
| `virtual QPointF` | `fromCartesian` | **纯虚**：双精度标量版反向映射 | `qreal x, qreal y` | public | `QPointF` | 经 final vec3 包装（反算/拾取阶段） | — |
| `virtual QRectF` | `computeDataBounds` | **纯虚**：viewRect → dataBounds（Numeric 范围反算；Polar 网格采样/Functional 采样实现） | `const QRectF& viewRect` | public | `QRectF` | Widget 阶段（dataBounds 反算）；S0 无调用方 | — |
| `virtual QRectF` | `computeViewRect` | **纯虚**：dataBounds → viewRect（正算；Polar 扇形包围盒等） | `const QRectF& dataBounds` | public | `QRectF` | Widget 阶段（setDataRange）；S0 无调用方 | — |
| `virtual QRectF` | `defaultDataBounds` | 默认 Numeric 范围（(0,0,10,10)；子类可覆写——Cartesian 同默认、Polar 覆写 (0,0,360,10)） | 无 | public | `QRectF` | Widget 首次初始化 viewRect | — |
| `QVector3D` | `toCartesian` | **final**：vec3 包装（取 x/y → QPointF 版 → z=0） | `const QVector3D& num` | public | `QVector3D` | CPU 渲染器（经基类指针虚调用） | — |
| `QVector3D` | `fromCartesian` | **final**：vec3 反向包装（z=0） | `const QVector3D& cart` | public | `QVector3D` | 反算路径 | — |
| `int` | `dimension` | **final**：返回 2 | 无 | public | `2` | QChartGL 哈希键 | — |

Notes:
- glslToCartesian/glslFromCartesian 仍为纯虚（继承自 QChartAbstractProjection 未在此实现），由 2D 子类各自提供表达式。
- final 包装防子类重载 vec3 接口（统一走标量版）；投影族 header-only（唯一例外 QInterpolatedProjection 有 cpp）。
- S0 测试以 `std::unique_ptr<QChartProjection>`（实际 Cartesian/Polar）经本基类指针装配 scene.projection。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
