# updateHover_flow.md —— 3D 悬停：统一后端分支（GL ID 帧 / CPU 屏幕近邻）

> t55 核心函数 flow · core 模块（src/core/QChartWidget3D.cpp `updateHover`）

## 控制流（调用图）

```
QChartWidget3D::mouseMoveEvent
  └─ updateHover(pos)                        # D-3D-13：不弹 tooltip，只发信号
       ├─ glActive = (renderBackend==OpenGL) && m_glRenderer && m_glRenderer->isReady()
       │
       ├─ GL 分支（glActive）:
       │    ├─ scene = buildScreenScene()
       │    ├─ local = pos − scene.plotArea.topLeft()      # 外层坐标 → 宿主局部（ID 帧视口=plotArea）
       │    ├─ id = m_glRenderer->pickIdAt(local, scene)   # ID 帧 + 三闸（见 pickIdAt_flow）
       │    ├─ r = QChartHitTester::hitTestGPU(id 的 RGB, m_glRenderer->pickTable())
       │    └─ hitSeries = qobject_cast<QChartSeries3D*>(r.series); hitIndex = r.dataIndex
       │
       └─ CPU 分支（QPainter 后端，A7 保留路径）:
            ├─ bestDist = 8.0（全局阈值，跨系列）
            ├─ 遍历 m_layers3D → g->makeProjectFn(cam, m_plotArea)  # 全链闭包
            │    └─ 遍历 series3DList（可见）→ s->collectPrimitives(fn, items)
            │         └─ r = QChartHitTester::hitTest(pos, items, bestDist)（屏幕近邻）
            │              └─ 命中 → 收紧 bestDist = min(该 dataIndex 图元距离)（保持全局最近语义）
            └─ hitSeries/hitIndex = 最近命中
       │
       └─ 命中处理:
            ├─ 命中且 (u,v) 变化 → emit uvHovered(u,v)；m_hoverActive=true；m_lastHoverUV=(u,v)
            └─ 未命中且 m_hoverActive → emit uvHoveredEnd()；m_hoverActive=false
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `pos`（widget 坐标，鼠标位置） |
| GL 分支中间量 | scene（快照复用）；`local`（ID 帧视口局部坐标）；`id`（RGB24，0xFFFFFF=未命中）；pickTable（与批次同步构建） |
| CPU 分支中间量 | `bestDist`（全局最近阈值，初始 8px，命中后收紧）；collectPrimitives 图元列表（瞬态）；makeProjectFn 闭包（Data→Numeric→World→Screen 全链） |
| 出参 | `(u,v)`（**Numeric 空间**：hitSeries->at(hitIndex).x/y().toDouble()）经 uvHovered/uvHoveredEnd 信号 |
| 状态变更 | `m_hoverActive`（激活/结束）、`m_lastHoverUV`（去重：同点不重复发） |

## 时序（触发时机与先后）

1. **鼠标移动驱动**（mouseMoveEvent）——不是定时器；高频移动由 Qt 事件循环合并 + 拾取三闸（GL 分支）自然降频。
2. **统一后端一致性（D26）**：`glActive` 判定保证拾取与渲染同后端（GL 渲染时绝不走 CPU 近邻，反之亦然——禁止混搭）。
3. **GL 分支的限流**：pickIdAt 内部三闸（①事件合并外部 ②位移≥1px + ≥16ms ③m_glReady）→ 高频 move 返回缓存结果。
4. **信号去重**：`(u,v)` 与 m_lastHoverUV 相同且已激活 → 不重复 emit（D18 联动防风暴）；离开 widget（leaveEvent）→ uvHoveredEnd。
5. **dataIndex 语义**：命中图元 dataIndex → series3D->at(index)（3D 系列 dataIndex 即数据索引，与 2D HitResult.index 对齐）。

## 关联

- pickIdAt 全流程：docs/core/pickIdAt_flow.md；命中解码：QChartHitTester::hitTestGPU（docs/core/QChartHitTester.md）。
- 相关决策：D18（uv 信号联动）、D26（统一后端）、D-3D-13（3D 悬停简化版，不弹 tooltip）。
