# cartesianToPixel_viewCube_flow.md —— 2D 映射流与 3D viewCube 派生链流

> t55 核心函数 flow · core 模块
> 主题：2D `cartesianToPixel/pixelToCartesian`（View Cartesian ↔ Pixel）调用流 + 3D viewCube → 相机派生 → 投影流（2D 是 3D 退化特例）。
> 全推导：docs/core/deepdive_viewRect.md；纯函数实现：src/core/QChartCamera.cpp / QChartCamera3D.cpp。

## 一、2D 映射流（QChartCamera2D::cartesianToPixel / pixelToCartesian）

### 控制流（调用图）

```
QChartWidget::cartesianToPixel(cx, cy)  ← 用户/交互/测试
   └─ QChartCamera2D::cartesianToPixel(m_viewRect, m_plotArea, cx, cy)   # 静态纯函数
DrawContext（QChartAxis.h:42/53，轴/刻度/系列绘制）
   └─ QChartCamera2D::cartesianToPixel(viewRect, plotArea, ...)          # 静态版直接复用
QChartWidget::pixelToCartesian(pixel)
   └─ QChartCamera2D::pixelToCartesian(...)                              # 逆映射
      └─ 消费方：mouseMoveEvent（pan 位移换算）/wheelEvent（缩放中心）/命中
```

### 数据流（入参/出参）

```
入参：viewRect（View Cartesian 空间矩形）、plotArea（像素矩形）、点坐标
正向：nx=(cx−vl)/vw；ny=(cy−vt)/vh；px=pl+nx·pw；py=(pt+ph)−ny·ph   # y 翻转
反向：nx=(px−pl)/pw；ny=(pb−py)/ph；cx=vl+nx·vw；cy=vt+ny·vh
出参：QPointF（像素/View 坐标）；无状态变更（纯函数）
```

### 时序

- 每次绘制（轴/刻度/系列经 DrawContext）与交互（pan/zoom/命中）即时调用；无缓存（线性 O(1)）。
- y 翻转一致性：与 3D `QChartMath::clipToScreen` 同向（View 上 → 像素上）——正交俯视 ≡ 2D 的硬验收锚点（D-3D-2）。

## 二、3D viewCube 派生链流（QChartCamera3D）

### 控制流（调用图）

```
主状态 setter（setViewCube/ViewCubeCenter/ViewCubeSize/setYaw/Pitch/FovY）
  └─ emit viewChanged → QChartWidget3D 槽（×2）
       ├─ recomputeDataBounds3D（5³ 反算，见 deepdive_viewCube）
       └─ pushAxesDataBoxToLayers + 重绘
交互：mouseMoveEvent → camera3D->orbit / wheelEvent → camera3D->dolly / API panViewCube
  └─ 修改主状态 → viewChanged → 同上

渲染/拾取链：
Layer3D::makeProjectFn → 闭包内：
  ├─ camera3D->viewProjectionMatrix(aspect)     # P·V：projectionMatrix · viewMatrix
  │    ├─ viewMatrix = lookAt(position, lookAt, up)
  │    ├─ projectionMatrix = perspective(fovY,aspect,near,far) | ortho(±盒半尺寸, near, far)
  │    └─ 派生：radius=|size|/2；d=radius/tan(fovY/2)；position=lookAt−forward·d；
  │            near=max(0.01,d−1.5r)；far=d+1.5r；frame() 产 forward/up/right
  └─ camera3D->project(world, plotArea)
       └─ viewProj·world → clipToScreen（w≤0→NaN）+ viewDepth（−viewZ）
```

### 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 主状态（入参） | viewCube{min,max}（World 盒）、yaw/pitch、fovY、projectionMode |
| 派生（只读，不存储） | radius/distance/position/lookAt/up/nearPlane/farPlane（每次访问由 frame()/公式重算） |
| 出参 | viewMatrix / projectionMatrix(aspect) / viewProjectionMatrix(aspect) / project() → QChartProjectedPoint{screen, depth, world} |
| 状态变更 | 主状态 setter 值实际变化时 emit viewChanged → 下游反算 + 重绘；相机本身不缓存派生量 |

### 时序（触发时机与先后）

1. **交互 → 状态 → 信号 → 反算**：orbit/dolly/panViewCube（或属性动画）修改主状态 → viewChanged → recomputeDataBounds3D（每帧不重算：仅实际变化时）→ 推轴盒 → 重绘。
2. **渲染时派生**：Layer3D 闭包组装时即时调用 viewProjectionMatrix/project（每帧）；派生公式 O(1) 无缓存。
3. **视图变化不触发 VBO 重建**（GL 路径）：顶点是 World 坐标，相机变化仅换 u_viewProj uniform（D30）。
4. **2D 是 3D 退化特例**：正交模式 + 恒等映射（Cartesian3D）下，viewCube 即投影盒 → 正交俯视 ≡ 2D cartesianToPixel（单测像素断言锁死）。

## 关联

- 全推导与边界：docs/core/deepdive_viewRect.md；5³ 反算：docs/projection/deepdive_viewCube.md。
- 相关决策：D1（相机独立成类）、D21（viewCube 主状态）、R5/R6（派生只读/无平移手势）、D-3D-2（退化一致性硬验收）。
