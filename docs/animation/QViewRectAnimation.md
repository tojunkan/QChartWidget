# QViewRectAnimation Documentation

## Brief Introduction:
2D 相机漫游动画（QChartAnimation 派生）：驱动 QChartWidget 的 **viewRect**（相机位置+缩放），影响所有 Layer/Series。`setTargetViewRect` 必设（目标视窗）；`setWaypoint` 可选（相机中心弧线经过点 → **Quad Bézier 中心路径**）；`setSizeCurve` 可选（视图宽度 vs α 的自定义缩放节奏）；`setGenerator` 可选（α → 完整 viewRect 直出）。**首次 animate 自动快照源**（m_srcRect = 当前 viewRect + plotArea 长宽比锁定）——调用方无需预置起点。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartWidget*` | `m_widget` | 目标 Widget（非持有；setTargetWidget）。 | `QChartWidget*`/`nullptr` | `nullptr` | `QChartWidget` |
| `QRectF` | `m_srcRect` | 源 viewRect 快照（首次 animate 时 = widget->viewRect()）。 | `QRectF` | 无效（isValid 作首次标记） | — |
| `QRectF` | `m_dstRect` | 目标 viewRect（setTargetViewRect；未设 → 停原地）。 | `QRectF` | 无效 | — |
| `QPointF` | `m_waypoint` | 中心弧线经过点（setWaypoint 设置 + m_hasWaypoint 标记）。 | `QPointF` | — | — |
| `bool` | `m_hasWaypoint` | waypoint 激活标记。 | `true`/`false` | `false` | — |
| `std::function<qreal(qreal)>` | `m_sizeCurve` | 宽度曲线（setSizeCurve；α → 宽度）。 | `std::function<qreal(qreal)>`/空 | 空 | — |
| `Generator` | `m_gen` | 生成器（setGenerator；α → 完整 viewRect 直出）。 | `std::function<void(qreal,QRectF&)>`/空 | 空 | — |
| `bool` | `m_useGenerator` | Generator 模式标记。 | `true`/`false` | `false` | — |
| `qreal` | `m_aspectRatio` | 长宽比快照（plotArea 决定，动画全程锁定）。 | `qreal` | 首次快照 | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QViewRectAnimation` | 构造函数。 | `QObject* parent=nullptr` | public | — | 用户/demo | — |
| `void` | `setTargetWidget` | 绑定目标 Widget（内联）。 | `QChartWidget* w` | public | — | 用户 | `QChartWidget` |
| `QChartWidget*` | `targetWidget` | 目标访问器（内联）。 | 无 | public | `QChartWidget*` | 测试 | — |
| `void` | `setTargetViewRect` | 必设：目标视窗。 | `const QRectF& target` | public | — | 用户 | — |
| `void` | `setWaypoint` | 可选：中心弧线经过点。 | `const QPointF& center` | public | — | 用户/demo（相机漫游） | — |
| `void` | `setSizeCurve` | 可选：宽度 vs α 曲线。 | `std::function<qreal(qreal)> curve` | public | — | 用户 | — |
| `void` | `setGenerator` | 可选：α → 完整 viewRect 直出。 | `Generator gen` | public | — | 用户 | — |
| `void` | `animate` | **核心**：Generator 直出；否则首次快照源+长宽比 → 中心路径（waypoint → Quad Bézier；否则直线 lerp）→ 宽度（sizeCurve 或目标宽度 lerp，高=宽/长宽比）→ `widget->setViewRect(合成 rect)`（→ viewChanged → 全链重算）。 | `qreal alpha` | public override | — | `updateCurrentTime`（每帧） | `QChartWidget` |

Notes:
- **首次调用快照**：m_srcRect 无效时取 widget->viewRect()（dstRect 无效 → 停原地）；长宽比由 plotArea 锁定（动画全程不拉伸）。
- Quad Bézier：`C(α) = (1−α)²·P0 + 2(1−α)α·P1 + α²·P2`（P1=waypoint）——相机中心走弧线，视角"绕"过中间点。
- 完整流程见 docs/animation/QViewRectAnimation_animate_flow.md。

## Overrided Qt Events:
None.

## Signals:
None.（继承）
