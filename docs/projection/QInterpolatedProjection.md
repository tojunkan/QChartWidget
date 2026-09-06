# QInterpolatedProjection Documentation

## Brief Introduction:
QInterpolatedProjection 是**合成/插值投影**（2D，继承 QChartProjection，唯一带 cpp 的 2D 投影）：两个投影之间的平滑过渡——`toCartesian/fromCartesian` 对两投影结果 **lerp**（alpha∈[0,1] 钳制；NaN 自然传播→路径断开）。动画期间由 QProjectionSwitchAnimation 临时挂到 Widget 驱动 blend（a/b **非持有**：a 由 Widget 持有、b 由调用者/动画持有）。包络（computeDataBounds/computeViewRect）**委托目标投影 B**（动画期间不 pan/zoom，刻度/包络用 B 近似即可）。GLSL：以 `mix((%1),(%2),u_blendAlpha)` 拼接两子投影表达式（缺子投影回退恒等 "num"/"cart"）；使用插值投影的 Shader 必须显式声明 `uniform float u_blendAlpha` 并在渲染循环 setUniformValue（GL 渲染器实测：dynamic_cast 识别后取 blend() 上传）。**R12 遗留**：`Q_LOGGING_CATEGORY(logProjection, "chart.projection")` 现定义于本 cpp（原在未入 S0 的 QChartWidget.cpp；Widget 阶段恢复时删除此处定义、恢复原位置）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartProjection*` | `m_a` | （private）源投影（非持有） | 指针/`nullptr` | `nullptr` | `QChartProjection` |
| `QChartProjection*` | `m_b` | （private）目标投影（非持有；包络委托与 type 判定用） | 指针/`nullptr` | `nullptr` | `QChartProjection` |
| `qreal` | `m_alpha` | （private）混合因子 | `qreal[0,1]`（映射时钳制） | `0.0` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QInterpolatedProjection` | 构造：存 a/b（非持有） | `QChartProjection* a, QChartProjection* b` | public | — | QProjectionSwitchAnimation（未入 S0） | — |
| `void` | `setBlend` | 设置混合因子（内联） | `qreal alpha` | public | — | 动画驱动 | — |
| `qreal` | `blend` | 混合因子访问器（内联） | 无 | public | `qreal` | `QOpenGLChartRenderer::drawPass`（dynamic_cast 后取 u_blendAlpha） | — |
| `CoordinateSystem` | `type` | 覆写（内联）：m_b ? b->type() : Cartesian | 无 | public | `CoordinateSystem` | — |
| `QPointF` | `toCartesian` | 覆写：a/b 各自 toCartesian（缺省恒等）；t=clamp(alpha)；lerp 结果 | `qreal num0, qreal num1` | public | `QPointF` | 渲染/动画路径 | — |
| `QPointF` | `fromCartesian` | 覆写：反向 lerp（同上） | `qreal x, qreal y` | public | `QPointF` | 反算路径 | — |
| `QString` | `glslToCartesian` | 覆写（内联）：a/b 空 → "num"；否则 `mix( (A_expr), (B_expr), u_blendAlpha )` | 无 | public | `QString` | QChartGL Shader 注入 | `QChartGL` |
| `QString` | `glslFromCartesian` | 覆写（内联）：a/b 空 → "cart"；否则反向 mix | 无 | public | `QString` | 未来 GPU 反算 | — |
| `QRectF` | `computeDataBounds` | 覆写：委托 `m_b->computeDataBounds`（b 空 → viewRect 恒等） | `const QRectF& viewRect` | public | `QRectF` | Widget 阶段 | — |
| `QRectF` | `computeViewRect` | 覆写：委托 `m_b->computeViewRect`（b 空 → dataBounds） | `const QRectF& dataBounds` | public | `QRectF` | Widget 阶段 | — |

Notes:
- 默认 m_alpha=0.0 = pureA（需动画显式 setBlend）；映射内部再钳制 [0,1]。
- 中间态奇点：任一子投影奇点 NaN → lerp NaN → CPU 端路径断开、GPU 端 mix 传播 NaN（契约允许）。
- S0 无测试实例（GL 矩阵 fixture 不挂插值投影）；GL 渲染器对插值投影的 uniform 上传路径已就绪。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
