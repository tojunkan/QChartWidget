# QChartCamera3D Documentation

## Brief Introduction:
QChartCamera3D 是 **3D 相机**（继承 QChartAbstractCamera，Q_OBJECT）：以 **ViewCube（QCube 数据锚点）+ 姿态（yaw/pitch/roll）+ 镜头参数（fov/distance/near/far）** 定义视图。矩阵接口：`viewMatrix`=lookAt(position, viewCubeCenter, up())；`projectionMatrix`=透视（QChartMath::perspectiveMatrix(fov, aspect, near, far)）或正交（以 viewCubeSize 半宽高 ±，Orthographic 模式）；`project` 经 viewProjectionMatrix → QChartMath::clipToScreen + viewDepth（CPU 拾取/标签/深度排序）；`unproject` NDC 近远平面逆变换成世界射线。fit：`fitToPlotArea` = `fitCameraConfig(plotArea, FixedFov)`，约束掩码（FitConstraint：FixedFov/FixedDist/FixedNear/FixedFar，优先级 Fov>Dist>Near>Far，None→FixedFov 行为）求解 distance/fov 并联动 near/far（用户手动覆盖 near/far 时尊重之）。交互快捷：orbit（yaw/pitch，pitch 钳 ±89°）、dolly（等比缩放 viewCube）、panViewCube（平移中心）。构造注册 QVector3D 动画插值器并按初始 viewCube 重算 distance + resetNearFar。S0 由 QChartLayer3D 以**值成员**持有（scene3D.camera 构造注入）；QChartWidget3D::fitWorld 经其配置。`autoFit` 开关字段在位但本阶段无调用方（fit 由 fitWorld 内联数学完成）。

## Constant Variables:
None.（`FitConstraint` 位掩码枚举 + `Q_DECLARE_FLAGS(FitConstraints)` 类型定义非类常量；`ProjectionMode{Perspective, Orthographic}` 为嵌套枚举）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QCube` | `m_viewCube` | （private）数据锚点盒（project 与 fit 的几何基准） | `QCube` | `(0,0,0)-(10,10,10)` | `QCube` |
| `qreal` | `m_yaw` | （private）偏航角（绕世界 Y；度） | `qreal` | `45.0` | — |
| `qreal` | `m_pitch` | （private）俯仰角（setter/交互钳 ±89°；度） | `qreal[-89,89]` | `30.0` | — |
| `qreal` | `m_roll` | （private）翻滚角（批次 B1 起支持） | `qreal` | `0.0` | — |
| `qreal` | `m_distance` | （private）站位距离（>0；构造按 radius/sin(fov/2) 重算） | `qreal(>0)` | `24.14`（注释：将被构造重算） | — |
| `qreal` | `m_fov` | （private）主视野角（垂直方向基准） | `qreal(1,179]` | `45.0` | — |
| `qreal` | `m_near/m_far` | （private）近/远裁面（resetNearFar 自动内切 distance±radius） | `qreal` | `0.01`/`100.0` | — |
| `bool` | `m_nearFarOverride` | （private）用户手动覆盖 near/far 标记（fit 尊重之） | `true`/`false` | `false` | — |
| `ProjectionMode` | `m_projectionMode` | （private）投影模式 | Perspective/Orthographic | `Perspective` | — |
| `qreal` | `m_radius` | （private）缓存半径 = viewCube 对角线/2（updateCachedRadius 维护） | `qreal` | `5.0`（构造即更新） | — |
| `bool` | `m_autoFit` | （private）自动适配开关（setAutoFit；**本阶段无调用方**，待配置阶段） | `true`/`false` | `true` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartCamera3D` | 构造：注册 QVector3D 动画插值器（QVariantAnimation）；updateCachedRadius → distance=radius/sin(fov/2) → resetNearFar | `QObject* parent=nullptr` | public | — | QChartLayer3D 值成员（构造注入 scene3D.camera） | — |
| `QCube` | `viewCube` | 锚点盒访问器（内联） | 无 | public | `QCube` | `QPainterChartRenderer::isPrimitiveVisible`（cull）、测试 | — |
| `void` | `setViewCube` | 设置锚点盒：同值忽略；更新 + updateCachedRadius + viewChanged | `const QCube& box` | public | — | QChartWidget3D::fitWorld/setViewCube | — |
| `QVector3D` | `viewCubeCenter` | 中心 = (min+max)×0.5（内联；= lookAt） | 无 | public | `QVector3D` | 各 lookAt 计算 | — |
| `void` | `setViewCubeCenter` | 平移锚点盒中心（位移量移动 min/max）→ viewChanged | `const QVector3D& c` | public | — | `panViewCube` 内部、用户 | — |
| `QVector3D` | `viewCubeSize` | 尺寸 = max−min（内联） | 无 | public | `QVector3D` | 正交投影/frame | — |
| `void` | `setViewCubeSize` | 按中心重建尺寸（负分量 qWarning 忽略）→ viewChanged | `const QVector3D& s` | public | — | `dolly` 内部 | — |
| `qreal` | `yaw/pitch/roll` | 姿态访问器（内联） | 无 | public | `qreal` | fitWorld 判定、测试 | — |
| `void` | `setYaw` | 设偏航（同值忽略）→ viewChanged | `qreal deg` | public | — | demo_axis3d、fitWorld、测试 | — |
| `void` | `setPitch` | 设俯仰（钳 ±89°）→ viewChanged | `qreal deg` | public | — | 同上 | — |
| `void` | `setRoll` | 设翻滚 → viewChanged | `qreal deg` | public | — | 用户（演示姿态） | — |
| `qreal` | `fov` | 视野角访问器（内联） | 无 | public | `qreal` | fitWorld（halfDiag/sin(fov/2)） | — |
| `void` | `setFov` | 设视野角：∉(1,179] qWarning 忽略；变化 → viewChanged | `qreal deg` | public | — | 用户 | — |
| `qreal` | `distance` | 距离访问器（内联） | 无 | public | `qreal` | fit 求解器 | — |
| `void` | `setDistance` | 设站位距离：≤0 qWarning 忽略；变化 → viewChanged | `qreal d` | public | — | QChartWidget3D::fitWorld | — |
| `qreal` | `nearPlane/farPlane` | 裁面访问器（内联） | 无 | public | `qreal` | — | — |
| `void` | `setNearPlane/setFarPlane` | 设裁面（越界校验 qWarning；置 m_nearFarOverride=true）→ viewChanged | `qreal val` | public | — | 用户（覆盖自动值） | — |
| `void` | `resetNearFar` | 恢复自动内切：near=max(0.01, d−r)、far=d+r；清 override → viewChanged | 无 | public | — | QChartWidget3D::fitWorld/setViewCube、构造 | — |
| `ProjectionMode` | `projectionMode` | 投影模式访问器（内联） | 无 | public | Perspective/Orthographic | — | — |
| `void` | `setProjectionMode` | 切换透视/正交 → viewChanged | `ProjectionMode m` | public | — | 用户 | — |
| `bool` | `autoFit` | 自动适配开关访问器（内联） | 无 | public | `true`/`false` | 本阶段无调用方 | — |
| `void` | `setAutoFit` | 设置自动适配（内联） | `bool on` | public | — | 本阶段无调用方 | — |
| `QMatrix4x4` | `viewMatrix` | 覆写：lookAt(position(), viewCubeCenter(), up()) | 无 | public | `QMatrix4x4` | viewProjectionMatrix | — |
| `QMatrix4x4` | `projectionMatrix` | 覆写：Perspective → QChartMath::perspectiveMatrix(fov, aspect, near, far)；Orthographic → QChartMath::orthographicMatrix(±half) | `qreal aspect` | public | `QMatrix4x4` | viewProjectionMatrix | `QChartMath` |
| `QMatrix4x4` | `viewProjectionMatrix` | 覆写：projectionMatrix×viewMatrix（同基类默认实现语义） | `qreal aspect` | public | `QMatrix4x4` | `QOpenGLChartRenderer::drawPass`（u_viewProj）、CPU project | — |
| `QChartProjectedPoint` | `project` | 覆写：clip=vp×cart → QChartMath::clipToScreen(clip, plotArea)；depth=QChartMath::viewDepth(view, cart)；cart 回传 | `const QVector3D& cart, const QRectF& plotArea` | public | `QChartProjectedPoint` | CPU 3D 渲染（深度/落屏）、标签、worldToPixel | `QChartMath` |
| `Ray` | `unproject` | 覆写：像素 → NDC → 近/远平面逆 vp → 世界射线（w 退化回退 +z） | `const QPointF& pixel, const QRectF& plotArea` | public | `Ray` | 拾取阶段（S0 无调用方） | — |
| `bool` | `fitToPlotArea` | 覆写（内联）：= fitCameraConfig(plotArea, FixedFov) | `const QRectF& plotArea` | public | `true`/`false` | 本阶段无调用方（fitWorld 内联替代） | — |
| `bool` | `fitCameraConfig` | 求解器：plotArea 无效/半径 0 → false；约束冲突取最高优先级（None→FixedFov）；FixedFov→解 distance；FixedDist→解 fov（sinVal>1 时钳 179°）；FixedNear/FixedFar→先由 near/far= d±r 推 d 再解 fov；near/far 联动（用户 override 且未显式 FixedNear/Far 时尊重）；变化 → viewChanged，返回是否变化 | `const QRectF& plotArea, FitConstraints constraints=None` | public | `true`/`false` | 配置阶段 | — |
| `void` | `orbit` | 旋转视角：yaw+=Δ；pitch 钳 ±89° → viewChanged | `qreal deltaYawDeg, qreal deltaPitchDeg` | public | — | 交互阶段（S0 无调用方） | — |
| `void` | `dolly` | 缩放：factor>0 且 viewCube 非零 → setViewCubeSize(尺寸×factor) | `qreal factor` | public | — | 交互阶段 | — |
| `void` | `panViewCube` | 平移：setViewCubeCenter(中心+(dx,dy,0)) | `qreal dxcart, qreal dycart` | public | — | 交互阶段 | — |
| `QVector3D` | `position` | 相机位置 = lookAt − forward×distance | 无 | public | `QVector3D` | viewMatrix | — |
| `QVector3D` | `lookAt` | 注视点 = viewCubeCenter（内联） | 无 | public | `QVector3D` | viewMatrix/position | — |
| `QVector3D` | `up` | 由姿态派生的上向量 | 无 | public | `QVector3D` | viewMatrix | — |

Notes:
- 姿态基：yaw 绕世界 Y → pitch 绕右轴 → roll 绕前向（QQuaternion 级联，frame() 内实现）；pitch 钳制防万向退化。
- 深度语义：depth = 视图深度（−viewZ，QChartMath::viewDepth），CPU 3D painter 排序降序（远→近）。
- S0 实测调用方：QChartWidget3D（fitWorld/worldToPixel/viewCube/setViewCube）、QPainterChartRenderer（project/viewCube）、QOpenGLChartRenderer（viewProjectionMatrix）、demo_axis3d（setYaw/setPitch）、冒烟/矩阵测试（setViewCube/setYaw/setPitch/姿态组合）。
- Q_PROPERTY：viewCubeCenter/viewCubeSize/yaw/pitch/roll/fov/distance/nearPlane/farPlane（NOTIFY 全 viewChanged）。

## Overrided Qt Events:
无（QObject）。

## Signals:
None.（本类无新增；继承并发射 `viewChanged`——实测触发点：setViewCube/Center/Size、setYaw/Pitch/Roll/Fov/Distance/Near/Far、resetNearFar、setProjectionMode、fitCameraConfig（变化时）、orbit/dolly/panViewCube）

（注：本文件六段范式含类内枚举/掩码说明；与 QChartAbstractCamera 的 viewChanged 契约一致。）
