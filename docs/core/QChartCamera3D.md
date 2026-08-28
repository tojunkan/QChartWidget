# QChartCamera3D Documentation

## Brief Introduction:
3D 相机（R5/D21 viewCube 主状态模型，用户拍板）：状态 = **viewCube**（World 空间轴对齐盒，2D viewRect 的 3D 对标物，与相机无关）+ **orientation**（yaw/pitch，绕盒中心）+ **fovY**（固定用户参数，默认 45°）。**派生只读**（相机 = 纯映射器）：lookAt=盒中心；d=radius/tan(fovY/2)（radius=半对角线，保守拟合）；forward/up = R(yaw,pitch)·(0,0,−1)/(0,1,0)；position=lookAt−forward·d；near=max(0.01,d−1.5·radius)、far=d+1.5·radius；viewProjectionMatrix = perspective·lookAt。R5 删除 orthographicBox 独立状态（正交模式 viewCube 即投影盒，D-3D-2 硬验收）；R6 硬约束：orbit 只旋转 orientation（viewCube 不动）、平移无鼠标手势（仅 API panViewCube）。全推导见 docs/core/deepdive_viewRect.md。

## Constant Variables:
None.（`projectionMode()` 枚举 `ProjectionMode{Perspective, Orthographic}` 为类型级常量）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartWorldBox` | `m_viewCube` | World 轴对齐盒 {min,max}（主状态；fit/反算/投影盒来源）。 | `QChartWorldBox` | `{0,0,0}–{10,10,10}` | `QChartWorldBox` <br> `QChartProjection3D` |
| `qreal` | `m_yaw` | 绕世界 up 轴的朝向角（度；`setYaw` 修改）。 | `qreal`（度） | `45°`（3/4 视角） | — |
| `qreal` | `m_pitch` | 绕右轴的朝向角（度；clamp ±89° 防万向锁）。 | `qreal`（度） | `30°` | — |
| `qreal` | `m_fovY` | 纵向视场角（度；(1,179]）。 | `qreal`（度） | `45°` | — |
| `ProjectionMode` | `m_projectionMode` | 投影模式。 | `Perspective` <br> `Orthographic` | `Perspective` | `QChartMath` |

Notes:
- position/lookAt/up/near/far **不存储**——每次访问派生计算（`frame()` 重建 forward/up/right，`distance()/radius()` 由 viewCube 尺寸与 fovY 派生）。
- 零尺寸 viewCube：orbit/dolly no-op（防除零）。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartCamera3D` | 构造函数（内置默认 viewCube/orientation/fovY）。 | `QObject* parent` | public | — | QChartWidget3D 构造 | — |
| `QChartWorldBox` | `viewCube` | 盒访问器（内联）。 | 无 | public | — | Widget3D 反算/测试 | `QChartWorldBox` |
| `void` | `setViewCube` | 绝对设置盒 + emit viewChanged（变化时）。 | `const QChartWorldBox& box` | public | — | `setViewCubeToFit`/用户 | `QChartWorldBox` |
| `QVector3D` | `viewCubeCenter` | 盒中心访问器（内联 = (min+max)/2；Q_PROPERTY）。 | 无 | public | — | 动画/测试 | — |
| `void` | `setViewCubeCenter` | 平移盒（pan；R6 仅 API/动画驱动）+ emit viewChanged。 | `const QVector3D& c` | public | — | `panViewCube`/QPropertyAnimation | — |
| `QVector3D` | `viewCubeSize` | 盒尺寸访问器（内联 = max−min；Q_PROPERTY）。 | 无 | public | — | 动画/测试 | — |
| `void` | `setViewCubeSize` | 缩放盒（dolly；Q_PROPERTY）+ emit viewChanged。 | `const QVector3D& s` | public | — | QPropertyAnimation | — |
| `qreal` | `yaw` | yaw 访问器（内联；Q_PROPERTY）。 | 无 | public | — | 动画/测试 | — |
| `void` | `setYaw` | 设置 yaw（绕世界 up）+ emit viewChanged。 | `qreal deg` | public | — | orbit/QPropertyAnimation | — |
| `qreal` | `pitch` | pitch 访问器（内联；Q_PROPERTY）。 | 无 | public | — | 动画/测试 | — |
| `void` | `setPitch` | 设置 pitch（绕右轴；clamp ±89°）+ emit viewChanged。 | `qreal deg` | public | — | orbit/QPropertyAnimation | — |
| `qreal` | `fovY` | fovY 访问器（内联；Q_PROPERTY）。 | 无 | public | — | 测试 | — |
| `void` | `setFovY` | 设置 fovY（(1,179]）+ emit viewChanged。 | `qreal deg` | public | — | 用户 | — |
| `QVector3D` | `position` | 派生：lookAt − forward·d（只读）。 | 无 | public | — | 渲染/测试 | — |
| `QVector3D` | `lookAt` | 派生：= viewCubeCenter（只读，内联）。 | 无 | public | — | 渲染/测试 | — |
| `QVector3D` | `up` | 派生：= R(yaw,pitch)·(0,1,0)（只读）。 | 无 | public | — | 渲染/测试 | — |
| `qreal` | `nearPlane` | 派生：= max(0.01, d−1.5·radius)（只读）。 | 无 | public | — | 投影矩阵 | — |
| `qreal` | `farPlane` | 派生：= d + 1.5·radius（只读）。 | 无 | public | — | 投影矩阵 | — |
| `ProjectionMode` | `projectionMode` | 投影模式访问器（内联）。 | 无 | public | — | 测试 | — |
| `void` | `setProjectionMode` | 切换透视/正交（正交模式 viewCube 即投影盒）。 | `ProjectionMode m` | public | — | 用户/测试 | — |
| `QMatrix4x4` | `viewMatrix` | `QMatrix4x4::lookAt(position, lookAt, up)`。 | 无 | public | — | `viewProjectionMatrix`/渲染 | — |
| `QMatrix4x4` | `projectionMatrix` | 透视 `perspective(fovY,aspect,near,far)` / 正交 `ortho(±盒半尺寸,near,far)`。 | `qreal aspect` | public | — | `viewProjectionMatrix` | `QChartMath` |
| `QMatrix4x4` | `viewProjectionMatrix` | World→Clip 合并矩阵（P·V；Phase 3 直接产出，D-3D-10）。 | `qreal aspect` | public | — | Layer3D 闭包/GL 渲染器 | `QChartMath` |
| `void` | `orbit` | 绕盒中心旋转 orientation（yaw 绕世界 up、pitch 绕右轴；pitch clamp ±89°；**viewCube 不动**，R6；零尺寸 no-op）。 | `qreal deltaYawDeg` <br> `qreal deltaPitchDeg` | public | — | `QChartWidget3D::mouseMoveEvent`（拖拽） | `QChartWidget3D` |
| `void` | `dolly` | 缩放 viewCube（factor<1=盒缩小=内容放大；距离随盒尺寸重派生 → 2D zoom 同构）。 | `qreal factor` | public | — | `QChartWidget3D::wheelEvent` | `QChartWidget3D` |
| `void` | `panViewCube` | 平移 viewCube（dx/dy World 单位；lookAt/position 跟随；R6 仅 API/动画）。 | `qreal dxWorld` <br> `qreal dyWorld` | public | — | 用户/动画 | `QChartWidget3D` |
| `void` | `setViewCubeToFit` | fit：设置 viewCube=目标盒（中心=盒中心），orientation/fovY 保持。 | `const QChartWorldBox& box` | public | — | `QChartWidget3D::fitWorld`（A3 链终点） | `QChartWidget3D` |
| `QChartProjectedPoint` | `project` | `viewProjectionMatrix(aspect)·world` → clipToScreen + viewDepth；w≤0 → screen=NaN。 | `const QVector3D& world` <br> `const QRectF& plotArea` | public | — | Layer3D 闭包（makeProjectFn）/`worldToPixel` | `QChartMath` <br> `QChartProjectedPoint` |

Notes:
- 派生公式一致性：`frame()` 内 yaw 先绕世界 up、pitch 再绕 right（每步正交化）；`radius=|size|/2`（半对角线）、`d=radius/tan(fovY/2)`（保守拟合，头注释备精确拟合将来）。
- 交互不在此类（D-3D-4：Camera 不碰事件；手势在 QChartWidget3D 事件层）。

## Overrided Qt Events:
None.（非 QWidget）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `viewChanged` | 视图状态变化（viewCube/yaw/pitch/fovY/projectionMode 变化时 emit）。 | — | `QChartWidget3D` 连接 ×2（src/core/QChartWidget3D.cpp:95/142：反算 dataBounds3D + 推轴盒 + 重绘） | `QChartWidget3D` |

Notes:
- Q_PROPERTY 五个（viewCubeCenter/Size/yaw/pitch/fovY）均 NOTIFY viewChanged → QPropertyAnimation 可直接驱动（3D 相机动画/联动）。
- 发射点均在 setter 值实际变化时（setYaw/setPitch 带 clamp 后的实际值比较）。
