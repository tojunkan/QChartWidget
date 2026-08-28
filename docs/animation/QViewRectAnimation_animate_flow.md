# QViewRectAnimation_animate_flow.md —— 2D 相机漫游：首次快照 + Bézier 中心 + 宽度曲线

> t55 核心函数 flow · animation 模块（src/animation/QViewRectAnimation.cpp `animate`）

## 控制流（调用图）

```
start() → Qt 每帧 → updateCurrentTime → animate(eased)

animate(α):
  ├─ Generator 模式（m_useGenerator && m_gen）:
  │    m_gen(α, out) → widget->setViewRect(out)（直出，跳过合成）
  ├─ 首次调用快照（!m_srcRect.isValid()）:
  │    m_srcRect = widget->viewRect()            # ★ 自动快照源（调用方无需预置）
  │    !m_dstRect.isValid() → m_dstRect = m_srcRect（未设目标 → 停原地）
  │    m_aspectRatio = plotArea 宽高比（锁定，动画全程不拉伸）
  ├─ 中心路径:
  │    hasWaypoint → Quad Bézier: C(α) = (1−α)²·P0 + 2(1−α)α·P1 + α²·P2（P1=waypoint）
  │    else → 直线 lerp: center = srcCenter + (dstCenter−srcCenter)·α
  ├─ 视窗宽度:
  │    sizeCurve → w = m_sizeCurve(α)（自定义缩放节奏）
  │    else → w = srcW + (dstW−srcW)·α（宽度 lerp）
  │    h = w / m_aspectRatio（高随宽，长宽比锁定）
  ├─ 合成 QRectF(center − (w/2, h/2), w, h)
  └─ widget->setViewRect(合成 rect)
       └─ QChartWidget::setViewRect → dataBounds 反算 + invalidate×2 + emit viewChanged（全链重算）
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `α`（缓动后 [0,1]）+ 状态（m_widget/m_srcRect/m_dstRect/m_waypoint/m_sizeCurve/m_aspectRatio） |
| 中间量 | srcCenter/dstCenter（中心路径源/目标）；Bézier 系数 t1/t2/t3；w（宽度，sizeCurve 或 lerp）；h（=w/aspectRatio） |
| 出参 | 合成 `QRectF`（center 定心 + 尺寸） |
| 状态变更 | `m_srcRect/m_aspectRatio`（首次快照，仅一次）；`widget->viewRect`（每帧 → viewChanged → 全链：dataBounds 反算/fit/invalidate/重绘） |

**Quad Bézier 语义**：中心经 waypoint 走弧线——"相机"绕中间点转向（demo_camera 相机漫游）；无 waypoint 时直线最短路径。

## 时序（触发时机与先后）

1. **首次帧快照**：第一次 animate 时取 widget 当前 viewRect 为源（动画中途 start 也正确——从当前状态出发）。
2. **每帧合成 → setViewRect**：widget->setViewRect 是唯一写入点（→ viewChanged → 相机/数据/重绘全链）——动画只驱动一个属性，其余全自动。
3. **长宽比锁定**：快照自 plotArea（动画全程不拉伸——Fit 语义由 Widget 布局维持）。
4. **结束**：α=1 → 合成 = 目标（dstRect 精确）；finished 信号供调用方收尾。

## 边界与陷阱

1. **未设目标**：dstRect 无效 → 停原地（m_dstRect=m_srcRect）——防御性行为。
2. **首次快照语义**：src 快照后动画期间 widget viewRect 的外部修改会被覆盖（动画独占——符合"动画驱动相机"语义）。
3. **sizeCurve 域**：m_sizeCurve(α) 输出任意正宽度（调用方保证合理）；h 由长宽比推导（曲线只控宽）。
4. **Generator 与合成互斥**：Generator 直出跳过全部合成逻辑（调用方完全接管）。

## 关联

- Called By：updateCurrentTime（每帧）；QChartWidget::setViewRect（下游）。
- 类文档：docs/animation/QViewRectAnimation.md；深挖：docs/animation/deepdive_animation.md §4。
- 相关决策：D3（相机路径属"单属性表达不了"的自定义动画）、design_notes §Pan/Zoom。
