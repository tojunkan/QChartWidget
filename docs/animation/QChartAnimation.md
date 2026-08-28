# QChartAnimation Documentation

## Brief Introduction:
动画基类（D3 原则的落地骨架）：`QAbstractAnimation` 派生，统一骨架 **`updateCurrentTime → animate(easedAlpha)`**——Qt 负责时间轴驱动，子类只实现 `animate(qreal easedAlpha)`（α∈[0,1]，已含缓动）。`setDuration(ms)`/`setEasingCurve`。D3 分工：**标量属性动画优先 QPropertyAnimation**（Q_PROPERTY + NOTIFY），本类只服务"单属性表达不了"的自定义动画（逐点 morph/投影间插值/相机路径）。子类：QNumericSeriesAnimation/QBarAnimation/QViewRectAnimation/QProjectionSwitchAnimation。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `int` | `m_durationMs` | 动画时长（ms；setDuration 设置）。 | `int > 0` | 构造传入 | — |
| `QEasingCurve` | `m_easing` | 缓动曲线（setEasingCurve；valueForProgress(α)）。 | `QEasingCurve`（任意类型） | 构造传入 | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartAnimation` | 构造函数。 | `QObject* parent=nullptr` | public | — | 子类构造 | — |
| `void` | `setDuration` | 设置时长（内联）。 | `int ms` | public | — | 用户/demo | — |
| `void` | `setEasingCurve` | 设置缓动（内联）。 | `const QEasingCurve& c` | public | — | 用户/demo | — |
| `void` | `updateCurrentTime` | **骨架核心**：`α = clamp(currentTime/duration)` → `eased = easing.valueForProgress(α)` → `animate(eased)`（时长≤0 → 直接返回）。 | `int currentTime` | protected override | — | Qt 动画框架（每帧） | — |
| `void` | `animate` | **纯虚**：子类实现"α → 几何/状态"。 | `qreal easedAlpha` | public pure virtual | 子类实现 | `updateCurrentTime` | — |

Notes:
- 幂等约定：`animate(α)` 只依赖 α 与源/目标快照——可重复调用、可跳帧（暂停/恢复安全）。
- 无信号（继承 QAbstractAnimation 的 finished/stateChanged 供 demo 收尾）。

## Overrided Qt Events:
None.（QAbstractAnimation 非 QWidget；`updateCurrentTime` 为 Qt 动画虚函数覆写，已列入 Member Functions）

## Signals:
None.（继承 QAbstractAnimation 的 finished/stateChanged）
