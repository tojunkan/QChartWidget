# QChartLayer_makeToPixel_flow.md —— 2D 闭包组装：Data→Pixel 四跳链

> t55 核心函数 flow · layers 模块（src/layers/QChartLayer.cpp `makeToPixel`）★全库最核心的组装函数（Series 零耦合的 2D 落点）

## 控制流（调用图）

```
QPainterChartRenderer::drawForeground → layer->drawAllSeries(p, ctx)
  └─ makeToPixel(ctx)                                  # 组装（protected，drawAllSeries/hitTest 共用）
       ├─ 注入 ctx.toNumeric0 = [axisX ? axisX->toNumeric : d.toDouble]   # Series 不需要知道 Axis 类型
       ├─ 注入 ctx.toNumeric1 = [axisY ? axisY->toNumeric : d.toDouble]
       └─ 返回 lambda(DataX, DataY) → Pixel：
            ├─ num0 = toNumeric(dataX)；num1 = toNumeric(dataY)
            ├─ !isfinite(num0/num1) → NaN（非法输入短路）
            ├─ !ctx.projection → NaN
            ├─ cartesian = ctx.projection->toCartesian(num0, num1)
            ├─ !isfinite(cartesian) → NaN（奇点短路）
            └─ QChartCamera2D::cartesianToPixel(ctx.viewRect, ctx.plotArea, cartesian)
  └─ 逐系列：s->draw(painter, toPixel, &ctx)（每系列 save/opacity/restore）

命中路径（同源）：
  layer->hitTest(pixel, ctx) → makeToPixel(ctx) → QChartHitTester::hitTest(pixel, m_series, toPixel, &ctx)
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `DrawContext& ctx`（读 plotArea/dataBounds/viewRect/projection；**写** toNumeric0/1） |
| 中间量 | toNumeric0/1（轴闭包）；逐跳 NaN 短路（toNumeric 层 + toCartesian 层） |
| 出参 | `std::function<QPointF(QVariant,QVariant)>`（Data→Pixel 闭包） |
| 状态变更 | ctx.toNumeric0/1 写入（const_cast——组装副作用，系列画曲线边用） |

**四跳链**：`Data ─[axisX/Y::toNumeric]→ Numeric ─[NaN 检查]→ [projection::toCartesian]→ View Cartesian ─[QChartCamera2D::cartesianToPixel（线性 + y 翻转）]→ Pixel`
**关键语义**：toNumeric 由 Layer 注入（Series 不知道 Axis 类型）；toCartesian 由 Projection 定义"怎么映射"；cartesianToPixel 是唯一线性实现（静态纯函数）。

## 时序（触发时机与先后）

1. **每绘制帧**：drawAllSeries 每次调用组装一次闭包 → 逐系列消费（n 系列共用同一闭包——组装成本 O(1) 摊销）。
2. **每命中调用**：hitTest 独立组装（与绘制同源，行为一致）。
3. **NaN 短路顺序**：toNumeric 层 → toCartesian 层——奇点（Polar 极点等）在几何层被拦截，不进入像素映射。

## 边界与陷阱

1. **axis 为 null**：toNumeric 退化为 `d.toDouble()`（无轴场景直通）——闭包仍可用。
2. **ctx 非 const 写入**：makeToPixel 对 DrawContext const_cast 注入 toNumeric0/1——组装是幂等的（重复调用覆盖同值）。
3. **投影奇点**：toCartesian 返回 NaN/Inf → 短路返回 NaN（调用方 series->draw 跳过 NaN 点）。
4. **与 3D 同构**：ProjectFn3D（makeProjectFn）是同一思想的 3D 版（Data→toNumeric→toWorld→camera.project）——见 QChartLayer3D_makeProjectFn_flow.md。

## 关联

- Called By：drawAllSeries（绘制）/hitTest（命中）——均 src/layers/QChartLayer.cpp。
- 3D 对应：docs/layers/QChartLayer3D_makeProjectFn_flow.md；命中消费：QChartHitTester::hitTest（2D）。
- 相关决策：D15（系列零耦合——2D toPixel 同构延续）、design_notes §核心链路。
