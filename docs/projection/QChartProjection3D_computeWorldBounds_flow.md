# QChartProjection3D_computeWorldBounds_flow.md —— 16³ 采样 + 全 NaN 兜底（与快速通道的关系）

> t55 核心函数 flow · projection 模块（include/projection/QChartProjection3D.h `computeWorldBounds`，header-only 内联）

## 控制流（调用图）

```
QChartWidget3D::fitWorld（A3 全链，src/core/QChartWidget3D.cpp:231）
  ├─ resolveDataBox() → {dataMin, dataMax}            # 显式域盒 > 数据包围盒 > defaultDataBounds
  ├─ m_worldBounds = m_projection3D->computeWorldBounds(dataBox.first, dataBox.second)
  │    └─ QChartProjection3D::computeWorldBounds(dataMin, dataMax)     # 基类默认：16³ 采样
  │         ├─ grid=16；三轴 17³=4913 点：n = min + (i/16)·(max−min)
  │         ├─ toWorld(n0,n1,n2) → 有限点（x/y/z 均 isfinite）→ min/max 聚合
  │         └─ 全 NaN（minX 仍 Inf）→ 回退 {dataMin, dataMax}
  │    （子类覆盖：QChartFunctionalProjection3D 传 boundsFn → 自定义；否则基类采样）
  ├─ m_camera3D->setViewCubeToFit(m_worldBounds)      # R5：viewCube = 目标盒
  ├─ recomputeDataBounds3D()                          # viewCube→dataBounds（5³ 反算，见 deepdive_viewCube）
  └─ pushAxesDataBoxToLayers() + 重绘
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `dataMin/dataMax`（Numeric 空间范围——A3 链 resolveDataBox 产物） |
| 中间量 | `grid=16`（三轴 17 档）；逐点 toWorld；`minX/maxX/minY/maxY/minZ/maxZ`（初始 ±Inf）；全 NaN 判定（`qIsInf(minX)`） |
| 出参 | `QChartWorldBox{min,max}`（World 空间轴对齐盒） |
| 状态变更 | `m_worldBounds`（Widget3D）→ 相机 viewCube（setViewCubeToFit）→ 5³ 反算 dataBounds3D → 轴盒推送 |

**为什么采样而非取角点**：通用坐标系（柱/球）的 World 极值不在参数角点上（球面 r 极值在赤道面、非 (+x,+y,+z) 角）——必须网格采样（与 5³ 反算同因，D23）。

**与 isIdentityMapping 快速通道的关系**（常见误解澄清）：
- `computeWorldBounds`（基类）**不内部判定快速通道**——恒等时也执行 16³ 采样（结果精确：恒等采样 min/max == 直接取盒）；
- 快速通道由**消费方**读取 `isIdentityMapping()` 后跳采：`QChartWidget3D::recomputeDataBounds3D`（反算免采样）、`QChartLayer3D::emitLine`（图元免 toWorld、段数=2）；
- 结论：fitWorld 链中 computeWorldBounds 对 Cartesian3D 仍采样一次（O(4913)，一次性非每帧——A3 可接受；性能敏感时可后续加消费方跳采）。

## 时序（触发时机与先后）

1. **一次性链**：fitWorld 在 setProjection3D/setDomainBox/clearDomainBox/构造时调用（非每帧）；视图变化（orbit/dolly）**不重新 fit**——只反算 dataBounds3D。
2. **先包围盒后相机**：computeWorldBounds → setViewCubeToFit（viewCube=目标盒）→ 反算 → 推轴盒 → 重绘（A3 链严格顺序）。
3. **子类覆盖点**：Functional3D 的 boundsFn 可提供解析极值（跳过 4913 次采样）——自定义包围盒优先。

## 关联

- Called By：QChartWidget3D::fitWorld（:236）；子类覆盖：QChartFunctionalProjection3D（boundsFn 优先）。
- 反向采样（5³ 反算）：docs/projection/deepdive_viewCube.md；快速通道语义：QChartCartesianProjection3D.md。
- 相关决策：D23（5³ 反算 + 快速通道）、A3（内存预算链）、design_3d_axes §5.4。
