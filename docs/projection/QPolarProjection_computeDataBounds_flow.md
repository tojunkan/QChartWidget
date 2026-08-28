# QPolarProjection_computeDataBounds_flow.md —— 32×32 采样 + 跨 0° 边界修复

> t55 核心函数 flow · projection 模块（include/projection/QPolarProjection.h `computeDataBounds`，header-only 内联实现）

## 控制流（调用图）

```
QChartWidget::fitViewRectToPlotArea / setViewRect / setDataRangeDim0/1
  └─ m_projection->computeDataBounds(m_camera->viewRect())
       └─ QPolarProjection::computeDataBounds(viewRect)
            ├─ 1. containsOrigin = viewRect.contains((0,0))          # 原点在视口内 → rMin 精确 0
            ├─ 2. 32×32 网格采样（i,j ∈ [0,32]²，33×33=1089 点）:
            │     processPoint(x,y): fromCartesian(x,y) → (θ,r)
            │       有限点 → θMin/Max、rMax 聚合；!containsOrigin 时 rMin 聚合
            ├─ 3. 兜底：!finite(rMin)→0；!finite(rMax)→1
            ├─ 4. 跨 0° 边界判定（核心修复）:
            │     crossesZero   = (θMax−θMin > 180°)                  # 覆盖超过半圆
            │     coversPositiveX = 矩形覆盖正 X 轴（left≤0≤right 且 top≤0≤bottom 且 right>0）
            │     └─ 任一成立 → 返回完整圆盘 [0°,360°)×[rMin,rMax]
            │          QRectF(0, rMin, nextafter(360,−∞), rMax−rMin)  # nextafter 防闭区间 [0,360]
            └─ 5. 正常返回：θMin≤θMax 修正（swap）→ QRectF(θMin,rMin,θSpan,rSpan)
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `viewRect`（View Cartesian 矩形，极坐标下圆心通常不在原点——可能是任意位置圆盘/扇形） |
| 中间量 | `containsOrigin`；`θMin/θMax`（初始 360/0）、`rMin`（containsOrigin?0:Inf）、`rMax`（0）；`crossesZero`、`coversPositiveX` 双判据 |
| 出参 | `QRectF(θMin, rMin, θSpan, rSpan)`（正常）或完整圆盘 `QRectF(0, rMin, nextafter(360,−∞), rMax−rMin)`（跨 0°） |
| 状态变更 | 无（纯函数） |

**为什么跨 0° 必须特殊处理**：θ 采样值域 [0°,360°)——矩形同时含 θ≈0° 与 θ≈350° 的采样点时，朴素 min/max 得 [0,350] 跨度 350°（看似近全圆），实际视口可能只覆盖 10° 的窄扇形（跨 0° 射线）。两判据兜住：
- `crossesZero`：θSpan>180° → 无法可靠判定方向 → 直接返回完整圆盘（保守正确）；
- `coversPositiveX`：矩形覆盖正 X 轴 → θ 范围必含 0° → 完整圆盘（精确）。
- `nextafter(360,−∞)`：防闭区间 [0°,360°] 语义（360°≡0° 重复）——返回开区间上界。

## 时序（触发时机与先后）

1. **视图变化驱动**：pan/zoom/setViewRect/setProjection 后由 Widget 调用（fit 链）；Polar 下每视图变化重算（数据范围随视口变）。
2. **先于刻度生成**：computeDataBounds 结果 → Widget 持有 m_dataBounds → DrawContext 供轴 tickValues 生成。
3. **往返漂移防护**：Widget::fitViewRectToPlotArea 仅在相机 fit **实际修改 viewRect** 时反算（避免 computeDataBounds(computeViewRect(dataBounds)) 无限往返）。

## 关联

- Called By：QChartWidget（fit/反算，src/core/QChartWidget.cpp:191/215/227/239）。
- 同构：QFunctionalProjection::computeDataBounds 的 32×32 采样 fallback（无跨 0° 特殊逻辑——功能性投影无角度环绕语义）。
- 相关决策：design_notes §viewRect 与 dataBounds（包络互转职责）。
