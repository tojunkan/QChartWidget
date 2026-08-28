# QChartAnimation_updateCurrentTime_flow.md —— 基类骨架：时长→α→缓动→animate

> t55 核心函数 flow · animation 模块（src/animation/QChartAnimation.cpp `updateCurrentTime`）★全动画家族的时序引擎

## 控制流（调用图）

```
Qt 动画框架（QAbstractAnimation 每帧驱动，start() 后）
  └─ updateCurrentTime(currentTime)          # currentTime ∈ [0, duration]
       ├─ m_durationMs <= 0 → return         # 时长未设/非法：不驱动
       ├─ α_raw = clamp(currentTime / duration, 0, 1)
       ├─ eased = m_easing.valueForProgress(α_raw)      # 缓动曲线变换
       └─ animate(eased)                     # ★ 纯虚：子类实现"α → 几何/状态"
            ├─ QNumericSeriesAnimation::animate（逐点 lerp / Generator → setRenderOverride）
            ├─ QBarAnimation::animate（逐 rect lerp / Generator → setRenderOverride）
            ├─ QViewRectAnimation::animate（viewRect 合成：Bézier 中心 + 宽度曲线 → setViewRect）
            └─ QProjectionSwitchAnimation::animate（m_interp->setBlend(α) + 双 invalidate）
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `currentTime`（Qt 驱动，单调递增，可能跳帧） |
| 中间量 | `α_raw`（线性归一 [0,1]）、`eased`（缓动后 α——子类只见缓动值，不见原始时间） |
| 出参 | 无返回值；副作用 = 子类状态更新（系列覆盖层/viewRect/投影 blend）+ 对应重绘 |
| 状态变更 | 子类各自（详见各子类 flow/类 doc） |

**关键语义**：缓动在基类统一完成（子类不用感知 easing）；`animate` 只依赖 α 与源/目标快照 → **幂等、可跳帧**（暂停/恢复安全，Qt 时间线跳变不撕裂）。

## 时序（触发时机与先后）

1. **start()** → Qt 内部计时 → 每帧 updateCurrentTime（帧率由 Qt 动画驱动，非定时器）。
2. **α 归一 → 缓动 → animate**：三跳固定顺序；子类帧内完成"α→几何"后由自身触发重绘（setRenderOverride 通知 / setViewRect 通知 / invalidate×2）。
3. **结束**：currentTime 达 duration → QAbstractAnimation 状态机 Stopped → finished 信号（demo 收尾：clearRenderOverride 等）。
4. **与 QPropertyAnimation 分工**（D3）：标量属性动画不走本类（QPropertyAnimation 直接驱动 Q_PROPERTY + NOTIFY）；本类只服务"单属性表达不了"的形态。

## 边界与陷阱

1. **duration ≤ 0**：直接 return（不驱动 animate）——调用方应保证 setDuration>0。
2. **clamp**：currentTime 越界（暂停/seek）→ α 夹取 [0,1]（不越界采样）。
3. **缓动边界**：easing.valueForProgress(0)=0、(1)=1（线性/多数曲线）——端点精确性由子类保证（α=0 源、α=1 目标）。
4. **快照纪律**：子类在 setSource*/首次 animate 时快照——动画期间用户数据变化不撕裂（QViewRectAnimation 首次快照语义见其 flow）。

## 关联

- Called By：Qt 动画框架；下游 = 4 个 animate 覆写（各子类 doc）。
- 深挖：docs/animation/deepdive_animation.md（D3 边界 + 骨架原理）。
- 相关决策：D3（动画优先 QPropertyAnimation）。
