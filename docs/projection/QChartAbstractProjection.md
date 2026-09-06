# QChartAbstractProjection Documentation

## Brief Introduction:
QChartAbstractProjection 是 **2D/3D 投影统一抽象基类**（非 QObject；重构后已清除 Q_OBJECT——投影为纯值语义多态，无信号）：定义 Numeric ↔ Cartesian 的核心映射接口（`toCartesian/fromCartesian`，统一 `QVector3D` 入参出参）与 **GPU GLSL 表达式契约**（`glslToCartesian/glslFromCartesian` 返回以 `vec3 num`/`vec3 cart` 为输入的表达式字符串，QChartGL 注入顶点 Shader），并提供通用辅助 `createPath`（参数曲线采样 → 连续 Path 图元列表，NaN/Inf 自动断链）、维度信息（`dimension/dimensionName`）与采样提示（`samplingSegmentsHint`，默认 32；恒等映射快速通道 `isIdentityMapping` 默认 false）。派生：QChartProjection（2D）、QChartProjection3D（3D）。五空间链中投影掌管 **Numeric → Cartesian（View Cartesian）** 一环。

## Constant Variables:
None.（`CoordinateSystem{Cartesian, Polar, Functional, Cartesian3D, Spherical, Cylindrical, Functional3D}` 为嵌套枚举类型定义非类常量）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QStringList` | `m_dimNames` | （protected）维度名称列表（dimensionName 取用） | `QStringList` | 构造入参（派生类传默认名） | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartAbstractProjection` | 构造：存维度名 | `const QStringList& dimNames` | public | — | 派生类构造 | — |
| — | `~QChartAbstractProjection` | 虚析构（default） | 无 | public | — | — | — |
| `virtual CoordinateSystem` | `type` | **纯虚**：坐标系类型 | 无 | public | 7 值 | 测试夹具（Cartesian/Polar 分支）、绘制语义判断 | — |
| `virtual QVector3D` | `toCartesian` | **纯虚**：Numeric → Cartesian（正向，vec3 版） | `const QVector3D& num` | public | `QVector3D` | `QPainterChartRenderer::transformNumericToCartesian`（逐图元） | — |
| `virtual QString` | `glslToCartesian` | **纯虚**：GLSL 表达式（输入 vec3 num，输出 Cartesian 坐标表达式；空串=恒等回退） | 无 | public | `QString` | `QChartGL::buildVertexShader`（%1 注入） | `QChartGL` |
| `virtual QVector3D` | `fromCartesian` | **纯虚**：Cartesian → Numeric（反向；奇点返回 NaN） | `const QVector3D& cart` | public | `QVector3D` | 拾取/反算（S0 无直接调用方） | — |
| `virtual QString` | `glslFromCartesian` | **纯虚**：反向 GLSL 表达式（输入 vec3 cart） | 无 | public | `QString` | 未来 GPU 拾取/反算 | — |
| `virtual int` | `dimension` | **纯虚**：维度数（2 或 3） | 无 | public | `2`/`3` | `QChartGL::projectionTypeHash`（哈希键） | `QChartGL` |
| `QString` | `dimensionName` | 维度名访问器（内联；越界返回空串） | `int dim` | public | `QString` | QChartGL 编译日志（dimensionName(0)）、Axis 标签阶段 | — |
| `virtual bool` | `isIdentityMapping` | 是否恒等映射（快速通道优化；默认 false，QCartesianProjection 覆写 true） | 无 | public | `true`/`false` | `QPainterChartRenderer::transformNumericToCartesian`（Rect/Ellipse 直算分支） | — |
| `virtual int` | `samplingSegmentsHint` | 曲线采样段数提示（弯曲投影建议 64、恒等建议 2；默认 32） | 无 | public | `int` | `QChartLayer::drawGrid`（segments 取值） | — |
| `void` | `createPath` | 参数曲线采样（内联）：t∈[0,1] 均分 segments 段采样 dataCurve(t)；每点校验 isfinite——**无效点结束当前 Path 图元并开启新图元（自动断链）**；有效点追加；收尾提交末图元；只填 numVerts（Numeric），不调 toCartesian；样式由调用方设置 | `std::function<QVector3D(qreal t)> dataCurve, int segments, QVector<QChartPrimitive>& out` | public | — | Series 曲线收集（Series 阶段）；S0 无调用方 | `QChartPrimitive` |

Notes:
- 纯虚未实现项由派生补齐；本类不含数据映射状态（无 Q_OBJECT、无信号——重构清理点）。
- S0 实测引用：CPU/GL 渲染器经 scene.projection（本基类指针）调 toCartesian/isIdentityMapping/glslToCartesian；插值投影经 dynamic_cast 识别取 blend。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
