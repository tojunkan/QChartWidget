# QProjectionSwitchAnimation Documentation

## Brief Introduction:
投影切换动画（QChartAnimation 派生）：**投影间插值**——动画期间把 Widget 的 Projection 临时替换为 `QInterpolatedProjection{a=源投影, b=目标投影, α}`，每帧 `setBlend(α)` 推进（双投影采样 → **View Cartesian 结果空间 lerp**）；状态机钩子 `updateState`：**start 挂临时投影、结束落地**（clearTemporaryProjection + setProjection(m_dst) 所有权转移给 Widget）。析构兜底：动画被中止时清理临时投影。`setTargetProjection(unique_ptr)` 持有目标（动画生命周期）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartWidget*` | `m_widget` | 目标 Widget（非持有；setTargetWidget）。 | `QChartWidget*`/`nullptr` | `nullptr` | `QChartWidget` |
| `std::unique_ptr<QChartProjection>` | `m_dst` | 目标投影（**独占持有**；动画结束所有权转移给 Widget）。 | `std::unique_ptr<QChartProjection>`/空 | 空 | `QChartProjection` |
| `QInterpolatedProjection*` | `m_interp` | 合成投影（动画期挂到 Widget；start 创建/结束删除）。 | `QInterpolatedProjection*`/`nullptr` | `nullptr` | `QInterpolatedProjection` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QProjectionSwitchAnimation` | 构造函数。 | `QObject* parent=nullptr` | public | — | 用户/demo（swirl） | — |
| — | `~QProjectionSwitchAnimation` | 析构兜底：动画未跑完被中止 → `clearTemporaryProjection()` + delete m_interp（防悬挂临时投影）。 | — | public | — | — | `QChartWidget` |
| `void` | `setTargetWidget` | 绑定目标 Widget（内联）。 | `QChartWidget* w` | public | — | 用户 | `QChartWidget` |
| `QChartWidget*` | `targetWidget` | 目标访问器（内联）。 | 无 | public | `QChartWidget*` | 测试 | — |
| `void` | `setTargetProjection` | 设置目标投影（unique_ptr 转移持有）。 | `std::unique_ptr<QChartProjection> dst` | public | — | 用户/demo | `QChartProjection` |
| `void` | `updateState` | **状态机钩子**：Running（非 Running 进入）→ 快照源投影 + new QInterpolatedProjection(src, dst) + `setTemporaryProjection(m_interp)`；Stopped（Running 离开）→ `clearTemporaryProjection()` + delete m_interp + `setProjection(std::move(m_dst))`（落地）。 | `State newState, oldState` | public override | — | Qt 动画框架（start/stop） | `QChartWidget` <br> `QInterpolatedProjection` |
| `void` | `animate` | **每帧**：`m_interp->setBlend(α)` + `invalidateForeground()` + `invalidateBackground()`（前景系列 + 背景网格线都依赖投影，切换时一起扭曲刷新）。 | `qreal alpha` | public override | — | `updateCurrentTime` | `QChartWidget` |

Notes:
- **生命周期契约**：m_dst 独占（动画持有）→ 结束所有权转移给 Widget（setProjection）；m_interp 是动画期临时对象（非持有于 Widget——Widget 的 m_tempProjection 为裸指针）。
- 结果空间插值：两投影 toCartesian 输出间 lerp（非参数插值）——任意投影对可过渡（见 deepdive_animation §5）。
- 完整流程见 docs/animation/QProjectionSwitchAnimation_animate_flow.md。

## Overrided Qt Events:
None.

## Signals:
None.（继承）
