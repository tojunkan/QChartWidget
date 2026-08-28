# QProjectionSwitchAnimation_animate_flow.md —— 投影间插值：临时投影机制与生命周期

> t55 核心函数 flow · animation 模块（src/animation/QProjectionSwitchAnimation.cpp）

## 控制流（调用图）

```
start()（Qt 动画框架）
  └─ updateState(Running, 非Running)                    # ① 挂载
       ├─ src = widget->projection()（快照源投影）
       ├─ m_interp = new QInterpolatedProjection(src, m_dst.get())   # 合成投影（a=源,b=目标）
       └─ widget->setTemporaryProjection(m_interp)      # ★ 临时投影挂到 Widget（仅渲染路径）

每帧
  └─ updateCurrentTime → animate(eased)
       ├─ m_interp->setBlend(α)                         # ★ α 推进（双投影采样 → 结果空间 lerp）
       ├─ widget->invalidateForeground()                # 前景系列刷新（toPixel 依赖投影）
       └─ widget->invalidateBackground()                # 背景网格线刷新（drawAtPosition 数据主脊依赖投影）

stop()/完成（Qt 动画框架）
  └─ updateState(Stopped, Running)                      # ② 落地
       ├─ widget->clearTemporaryProjection()            # 摘除临时投影（渲染回正式路径）
       ├─ delete m_interp; m_interp = nullptr
       └─ widget->setProjection(std::move(m_dst))       # ★ 目标投影落地（所有权转移给 Widget）

析构兜底（动画被中止未走到 Stopped）
  └─ ~QProjectionSwitchAnimation
       ├─ widget->clearTemporaryProjection()            # 防悬挂临时投影
       └─ delete m_interp
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 挂载期 | src（Widget 当前投影，非持有）、m_dst（动画持有 unique_ptr）；m_interp 创建并挂到 Widget.m_tempProjection（**裸指针，非持有**） |
| 每帧 | `setBlend(α)`：QInterpolatedProjection 的 `toCartesian = lerp(a->toCartesian, b->toCartesian, α)`——**View Cartesian 结果空间插值**（非参数/数据空间）；fromCartesian 对称插值（交互反向一致） |
| 落地期 | clearTemporaryProjection（m_tempProjection=nullptr + invalidate）→ delete m_interp → `setProjection(std::move(m_dst))`（Widget 独占持有目标，动画释放） |
| 状态变更 | Widget.m_tempProjection（挂载/摘除）、Widget.m_projection（落地替换）、m_interp 生命周期（new/delete） |

**为什么可行**：任意两投影（如恒等↔极坐标）的 toCartesian 输出都在同一 View Cartesian 空间——输出间 lerp 平滑过渡，无需共同参数空间（深挖见 deepdive_animation §5）。

## 时序（触发时机与先后）

1. **start 挂载先于首帧**：updateState(Running) 在第一次 updateCurrentTime 之前（Qt 状态机保证）——首帧即用合成投影。
2. **每帧三动作**：setBlend → invalidateForeground → invalidateBackground（前景背景都要刷新——网格线数据主脊依赖投影，切换时一起扭曲）。
3. **结束落地**：updateState(Stopped) 摘除 + 删除 + 落地；若动画被中止（析构），兜底摘除（防 Widget 持有悬挂 m_interp 指针）。
4. **落地后**：Widget.setProjection 同步坐标系到所有 Layer + invalidate（常规 setProjection 路径）。

## 边界与陷阱

1. **重复 start**：setTargetProjection 后多次 start → 每次 Running 重新快照源 + 重建 m_interp（旧 m_interp 若未清理会泄漏——正常流程 Stopped 已删；中止由析构兜底）。
2. **m_dst 生命周期**：落地前 m_dst 由动画持有（unique_ptr）；落地时 move 转移——动画对象此后 m_dst 为空（不可复用同一动画二次 start，需重新 setTargetProjection）。
3. **src 快照语义**：源 = Widget 当前 projection（可能本身是临时投影？——正常流程动画不嵌套，防御上以当前值为准）。
4. **结果空间插值的 NaN**：任一投影奇点 → lerp 结果 NaN → 路径断开（与静态投影同策略）。

## 关联

- Called By：Qt 动画框架（start/stop/每帧）；QChartWidget::setTemporaryProjection/clearTemporaryProjection（挂载/摘除）。
- 合成投影：docs/projection/QInterpolatedProjection.md；深挖：docs/animation/deepdive_animation.md §5。
- 相关决策：D3（自定义动画场景）、design_notes §Projection 统一性（投影切换）。
