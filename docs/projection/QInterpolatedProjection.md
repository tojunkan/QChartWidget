# QInterpolatedProjection Documentation

## Brief Introduction:
合成投影（动画基座）：两个投影之间的平滑过渡——`toCartesian/fromCartesian` 在两投影**结果之间 lerp**（View Cartesian 结果空间插值，非参数插值——任意两投影都能过渡）。动画期间由 `QProjectionSwitchAnimation` 临时挂到 Widget 上驱动 `setBlend(α)`；`type()` 跟随目标投影 B。包络（computeDataBounds/computeViewRect）**委托给 B**（动画期间不 pan/zoom，仅刻度/包络用近似值）。a/b **非持有**（a 由 Widget 持有，b 由调用者/动画持有）。唯一有 .cpp 的投影类（src/projection/QInterpolatedProjection.cpp）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartProjection*` | `m_a` | 起始投影（非持有；通常 = Widget 当前投影）。 | `QChartProjection*` <br> `nullptr`（→ 恒等映射） | `nullptr` | `QChartProjection` |
| `QChartProjection*` | `m_b` | 目标投影（非持有；动画持有所有权）。 | `QChartProjection*` <br> `nullptr`（→ 恒等映射） | `nullptr` | `QChartProjection` |
| `qreal` | `m_alpha` | 混合因子（0=pureA，1=pureB；setBlend 修改）。 | `qreal`（clamp [0,1]） | `0.0` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QInterpolatedProjection` | 构造函数（a/b 非持有）。 | `QChartProjection* a` <br> `QChartProjection* b` | public | — | `QProjectionSwitchAnimation`（src/animation/QProjectionSwitchAnimation.cpp） | `QChartProjection` <br> `QProjectionSwitchAnimation` |
| `void` | `setBlend` | 设置混合因子（内联）。 | `qreal alpha` | public | — | `QProjectionSwitchAnimation::animate(α)` | `QProjectionSwitchAnimation` |
| `qreal` | `blend` | 混合因子访问器（内联）。 | 无 | public | `[0,1]` | 测试 | — |
| `CoordinateSystem` | `type` | 跟随 B（b 空 → Cartesian）。 | 无 | public override | `m_b ? m_b->type() : Cartesian` | 测试/Widget 同步 | — |
| `QPointF` | `toCartesian` | **核心**：`lerp(a->toCartesian, b->toCartesian, clamp(α))`；a/b 空 → 恒等；NaN 自然传播（任一投影奇点 → 结果 NaN → 路径断开）。 | `qreal num0, qreal num1` | public override | `QPointF` | `DrawContext`/createPath（动画期间渲染） | — |
| `QPointF` | `fromCartesian` | 对称 lerp（反向链路一致——hover/交互动画期间不跳变）。 | `qreal x, qreal y` | public override | `QPointF` | 交互反向（动画期间） | — |
| `QRectF` | `computeDataBounds` | 委托 B（b 空 → viewRect）。 | `const QRectF& viewRect` | public override | `m_b ? m_b->computeDataBounds(...) : viewRect` | `QChartWidget`（动画期间刻度/包络） | — |
| `QRectF` | `computeViewRect` | 委托 B。 | `const QRectF& dataBounds` | public override | 同上 | `QChartWidget` | — |

Notes:
- **插值空间**：View Cartesian 结果空间（两投影 toCartesian 输出间 lerp），不是映射参数/数据空间——恒等↔极坐标等任意组合可平滑过渡（深挖见 docs/animation/deepdive_animation.md §5）。
- 所有权红线：a/b 非持有——Widget 必须仍持有 a（m_projection），b 由动画持有；析构顺序由调用方保证。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
