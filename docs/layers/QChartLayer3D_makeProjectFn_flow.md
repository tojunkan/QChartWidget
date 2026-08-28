# QChartLayer3D_makeProjectFn_flow.md —— ProjectFn3D 全链闭包组装（series 零耦合落点）

> t55 核心函数 flow · layers 模块（src/layers/3d/QChartLayer3D.cpp `makeProjectFn`）★series 零耦合（D15）的 3D 落点

## 控制流（调用图）

```
collectPrimitives（Renderer collectScene / QPainter 3D 子路径）
  └─ g->makeProjectFn(scene.camera3D, scene.plotArea)      # 组装一次，全系列共用
       └─ 返回 lambda(const QDataPoint3D& d) → QChartProjectedPoint：
            ├─ n0 = axisX ? axisX->toNumeric(d.x()) : d.x().toDouble()
            ├─ n1 = axisY ? axisY->toNumeric(d.y()) : d.y().toDouble()
            ├─ n2 = axisZ ? axisZ->toNumeric(d.z()) : d.z().toDouble()     # Z 仅 toNumeric
            ├─ world = projection3D ? projection3D->toWorld(n0,n1,n2)
            │                        : QVector3D(n0,n1,n2)                  # 无投影直通
            ├─ !cam → QChartProjectedPoint{NaN, 0.0}                        # 相机缺失短路
            └─ cam->project(world, plotArea)                                # → {screen, depth, world}

QChartWidget3D::updateHover（CPU 分支）→ g->makeProjectFn(m_camera3D.get(), m_plotArea)   # 悬停复用同一闭包
```

## 数据流（入参/出参/状态变更）

| 项 | 说明 |
|---|---|
| 入参 | `cam`（QChartCamera3D*）+ `plotArea`（像素区）+ 本层状态（axisX/Y/Z、projection3D） |
| 中间量 | toNumeric×3（轴闭包，axis null → toDouble 直通）；toWorld（投影，null → 恒等直通） |
| 出参 | `ProjectFn3D = std::function<QChartProjectedPoint(const QDataPoint3D&)>` |
| 状态变更 | 无（纯组装；闭包捕获 this/cam/plotArea——调用方保证生命周期） |

**全链**：`Data(QDataPoint3D) ─[axisX/Y/Z::toNumeric]→ qreal×3 ─[projection3D::toWorld]→ World ─[camera3D.project（viewProj·clip→clipToScreen+viewDepth）]→ {screen, depth, world}`
**零耦合语义**：系列只消费 `projectFn(at(i))`——不持 Axis/Projection/相机引用（reviewer grep 验证点）；映射知识集中在 Layer3D（与 2D makeToPixel 同构）。

## 时序（触发时机与先后）

1. **收集期组装一次**：collectPrimitives 对每系列 `s->collectPrimitives(fn, out)` 传同一闭包（O(1) 组装摊销）；预投影（Line3D/Surface 两阶段）逐点调用。
2. **悬停复用**：updateHover CPU 分支每 move 组装（闭包构造廉价，仅为收集复用一致性）。
3. **GL 差异**：GL 路径 collectScene 同样经 makeProjectFn（CPU 收集）→ 图元 worldA/B → VBO（World 坐标，顶点着色器仅 u_viewProj——D30 每帧零 CPU 投影）。

## 边界与陷阱

1. **cam 为 null**：返回 `{NaN screen, 0 depth}`（调用方 at() 后跳过）——防御性短路。
2. **投影为 null**：toWorld 恒等直通（Numeric≡World）——与 Cartesian3D 快速通道语义一致。
3. **捕获生命周期**：闭包捕获 `this`（Layer3D）与 `cam`——调用方（collect 期间）保证两者存活；悬停路径 cam 为 Widget3D 相机（长期存活）。
4. **NaN 传播**：toNumeric（非法 Data）或 toWorld（奇点）产 NaN → project 的 clip w≤0 → screen NaN → 收集方跳过（断段/跳过点）。

## 关联

- Called By：collectPrimitives（Renderer/Layer3D）/QChartWidget3D::updateHover（CPU 分支）。
- 2D 同构：docs/layers/QChartLayer_makeToPixel_flow.md；消费方：各 3D 系列 collectPrimitives（docs/series/*）。
- 相关决策：D15（全链闭包）、D-3D-6（3D 系列两层组织）、design_3d §6.2。
