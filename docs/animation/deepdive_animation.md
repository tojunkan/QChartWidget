# deepdive_animation.md —— 动画机制：QPropertyAnimation 优先与自定义动画原理

> t53 核心计算深挖 · animation 模块
> 主题：D3 原则的落地（什么用 QPropertyAnimation、什么必须自定义）、统一骨架 `updateCurrentTime→animate(α)`、投影切换插值的数学原理（QInterpolatedProjection）。

---

## 0. 问题背景

动画要覆盖三类场景：标量属性（颜色/透明度）、逐点数据 morph（柱/折线）、跨对象状态（viewRect 相机、投影切换）。D3 定案：**能 QPropertyAnimation 就 QPropertyAnimation**（属性 + NOTIFY → invalidate 重绘，零自定义代码）；"单属性表达不了"的才写自定义动画。

## 1. 决策边界（D3：什么用 QPropertyAnimation）

| 场景 | 途径 | 原因 |
|---|---|---|
| 颜色/透明度/可见性 | QPropertyAnimation 驱动 Q_PROPERTY | 单标量属性，NOTIFY 信号 → invalidateForeground 自动重绘 |
| center/zoom（2D 相机） | QPropertyAnimation 驱动 QChartCamera2D 的 center/zoom 属性 | viewRect 派生属性，viewChanged → 重算 + 重绘 |
| viewCubeCenter/Size/yaw/pitch/fovY（3D 相机） | QPropertyAnimation 驱动 QChartCamera3D 属性 | 同上（viewChanged 统一信号） |
| 逐点数据 morph | **自定义**（QBarAnimation/QNumericSeriesAnimation） | 每帧整批几何插值，不是单个属性 |
| viewRect 弧线路径 + 尺寸曲线 | **自定义**（QViewRectAnimation） | 单属性表达不了（waypoint/sizeCurve） |
| 投影切换（坐标映射本身） | **自定义**（QProjectionSwitchAnimation + QInterpolatedProjection） | 映射函数间插值，无属性可驱动 |

## 2. 统一骨架：updateCurrentTime → animate(easedAlpha)

```
QChartAnimation（: QAbstractAnimation）
  setDuration(ms) / setEasingCurve(curve)
  updateCurrentTime(currentTime)          # Qt 每帧调用，currentTime ∈ [0, duration]
    α_raw = currentTime / duration
    easedAlpha = easingCurve().valueForProgress(α_raw)
    animate(easedAlpha)                    # 纯虚；子类只做"α → 几何"
```

要点：
- **幂等**：`animate(α)` 只依赖 α 与源/目标状态，可重复调用、可跳帧（Qt 动画暂停/恢复安全）。
- **源/目标快照**：自定义动画在 `setTarget*/setSource*` 时快照数值数组（QVector<QRectF>/QVector<QPointF>），动画期间不再读取用户数据（防数据中途变更撕裂）。

## 3. 逐点 morph 原理（QBarAnimation / QNumericSeriesAnimation）

```
源数组 S（numeric 空间）、目标数组 T（同构）
第 i 点：Pᵢ(α) = Sᵢ + α·(Tᵢ − Sᵢ)          # 逐点线性插值（Numeric 空间！）
每帧：整批更新系列数据（一次性替换，非逐点追加——避免 N 次信号风暴）
```
- 插值在 **Numeric 空间**（QBarSeries 的 rect / QXYSeries 的点），渲染侧（toPixel/投影）每帧照常走——映射正确性由渲染层保证。
- Generator 模式（demo_sort）：`setGenerator` 提供"α 时刻的完整数据"而非插值两帧——冒泡排序动画每帧重排，天然幂等。

## 4. viewRect 相机动画原理（QViewRectAnimation）

```
必设 setTargetViewRect(target)
可选 setWaypoint(center)：相机中心弧线经过点——用二次贝塞尔/两段线性经过
可选 setSizeCurve(fn)：视图宽度 vs α（缩放节奏自定义）
animate(α)：
  center(α) = 当前中心与 target 中心（经 waypoint）的插值
  size(α)   = sizeCurve 作用于宽度插值（高度按目标长宽比）
  → widget->setViewRect(合成 rect)        # viewChanged → 全链重算
```
影响面：viewRect 是 View Cartesian 空间的全局视窗 → 所有 Layer/Series 同时变换（"相机动画"语义）。

## 5. 投影切换插值原理（QProjectionSwitchAnimation + QInterpolatedProjection）

`QInterpolatedProjection{a, b, α}`（include/projection/QInterpolatedProjection.h）：

```
toCartesian(num0, num1)   = lerp(a->toCartesian(...), b->toCartesian(...), α)
fromCartesian(x, y)       = lerp(a->fromCartesian(...), b->fromCartesian(...), α)
type()                    = b ? b->type() : Cartesian
```

动画流程（QProjectionSwitchAnimation）：
```
1. setTargetProjection(dst)：保存目标投影，构造插值投影 {当前, dst, α=0}
2. 动画期间：widget->setProjection(插值投影)；animate(α) → 插值投影 setBlend(α)
   （坐标映射平滑过渡：每个 Numeric 点的屏幕位置在两投影结果间滑动）
3. updateState(StateChange)：结束 → widget->setProjection(dst)（落定最终投影，移除插值壳）
```

**为什么可行**：插值在 **View Cartesian 结果空间**做（两投影的 toCartesian 结果之间 lerp），而非插值映射参数——任意两投影（如恒等 ↔ 极坐标）都能平滑过渡，无需共同参数空间。

## 6. 边界与陷阱

1. **投影插值的可逆性**：`fromCartesian` 同时插值保证反向链路一致（hover/交互在动画期间不跳变）。
2. **结束落定**：`updateState` 必须把最终投影替换插值壳——否则动画结束后 widget 仍持插值投影（α=1 但对象残留），后续 setProjection 语义错乱。
3. **快照一致性**：源/目标数组长度不同 → 插值越界（调用方保证同构；demo 用同构数据）。
4. **Numeric 空间插值 vs Data 空间**：morph 必须插值 Numeric（数值化后），不能插值 Data（QDateTime/QString 无插值语义）——这也是"Data ≠ Numeric"红线在动画侧的体现。
5. **QPropertyAnimation 场景的 NOTIFY**：属性动画依赖 NOTIFY 信号触发重绘——缺 NOTIFY 的属性（如 title）不能驱动动画。

## 7. 单测对照

| 锚点 | 断言 |
|---|---|
| TestQChartAnimation（或 demo 级验证） | animate(0)/animate(1) 端点精确（源/目标）、α=0.5 中点插值正确 |
| TestQChartProjection（QInterpolatedProjection） | lerp 端点与中点、type 跟随、fromCartesian 与 toCartesian 一致 |
| demo_sort / demo_camera / demo_swirl | 冒泡排序每帧重排、viewRect 弧线路径、恒等↔Swirl 投影切换平滑（冒烟） |

> 回归口径：ctest 180 PASS + 2 SKIP 全绿；动画插值公式/结束落定逻辑改动需重跑对应测试类 + 相关 demo 冒烟。
