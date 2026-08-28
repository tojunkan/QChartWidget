# QChartProjection_createPath_flow.md —— createPath / createPath3D：NaN 断路径语义

> t55 核心函数 flow · projection 模块
> 2D：include/projection/QChartProjection.h `createPath`（默认实现）；3D：include/projection/QChartProjection3D.h `createPath3D`（默认实现）。

## 控制流（调用图）

```
2D createPath（segments=64 默认）:
  QChartAxis::drawAtEdge（边框轴曲线，src/axes/QChartAxis.cpp:400，segments=72）
  DrawContext::toPath（QChartAxis.h:50，series 路径/轴路径）
    └─ ctx.projection->createPath(dataCurve, segments)
         ├─ for i ∈ [0, segments]: t = i/segments
         │    numeric = dataCurve(t)                    # Axis 定义"在哪画"（Numeric）
         │    cartesian = toCartesian(numeric)          # Projection 定义"怎么映射"（View Cartesian）
         │    ├─ !isfinite(cartesian) → firstValid=true; continue   # ★ 断路径（moveTo 重开）
         │    └─ firstValid → path.moveTo(cartesian) else path.lineTo(cartesian)
         └─ 返回 QPainterPath（View Cartesian 空间；调用方还需 cartesianToPixel 变换后绘制）

3D createPath3D（segments=64 默认）:
  库内无调用方（测试/外部消费方；与 2D 同构）
    └─ for i ∈ [0, segments]: t = i/segments
         numeric = dataCurve(t)（QVector3D 三元组）
         w = toWorld(numeric)
         ├─ !isfinite(w) → 当前子路径 flush 入 result；current.clear()        # ★ 断路径（重开子路径）
         └─ current.append(w)
         （结尾 flush 残余子路径）
    └─ 返回 QVector<QVector<QVector3D>>（子路径列表，每段内连续）
```

## 数据流（入参/出参/状态变更）

| 项 | 2D createPath | 3D createPath3D |
|---|---|---|
| 入参 | `dataCurve: t∈[0,1]→Numeric (num0,num1)`；`segments` | `dataCurve: t∈[0,1]→Numeric 三元组`；`segments` |
| 出参 | `QPainterPath`（View Cartesian 空间，子路径在 NaN 处断开） | `QVector<QVector<QVector3D>>`（World 空间子路径列表） |
| 断路径语义 | NaN/Inf 点 → `firstValid=true`（下一次有效点 moveTo 重开） | NaN/Inf 点 → flush 当前子路径 + 重开 |
| 状态变更 | 无（纯函数） | 无（纯函数） |

**断路径为什么必要**：极坐标奇点（r=0 → fromCartesian NaN）、3D 柱/球奇点（r=0）与投影域外点在路径中若直接 lineTo 会画"跨奇点"的假连线（如从 +180° 直连 −180° 穿过圆心）——断路径保证每段子路径内连续、奇点处视觉断开。

## 时序（触发时机与先后）

1. **2D**：轴绘制（drawAtEdge 边框轴曲线，72 段）与 DrawContext::toPath（系列/网格路径）每次绘制调用——路径在 View Cartesian 空间生成后经 cartesianToPixel 变换到像素。
2. **3D**：createPath3D 库内无调用方（一次性生成 World 折线供外部/测试消费；Layer3D 用 emitLine/collectPrimitives 走闭包路径，不经 createPath3D）——语义与 2D 对齐（D-3D-5 同构）。
3. **与渲染关系**：createPath 产出的是几何路径（非图元）；渲染路径（QPainterChartRenderer）最终消费 View 空间路径绘制。

## 关联

- Called By（2D）：QChartAxis.cpp:400（drawAtEdge，72 段）/QChartAxis.h:50（DrawContext::toPath）；3D：无库内调用。
- 相关决策：design_notes §Projection 统一性（createPath 由 Projection 定义映射）、D-3D-5（3D 同构）。
