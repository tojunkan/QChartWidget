# module_projection.md —— projection 模块

> 属于 t53 文档套件（t52 重构新增的第七模块，用户点名）；配套 deepdive：`docs/projection/deepdive_viewCube.md`（viewCube 5³ 反算与快速通道）。

## 1. 职责与边界

projection = **2D/3D 投影家族**（Numeric ↔ View Cartesian / Numeric ↔ World 双向映射）+ **工厂** + **World 包围盒**。

- 依赖：仅 `utils`（QChartDebug 日志；QDataPoint 无关）。
- 被依赖：`axes`（QChartAxis include QChartProjection）、`core`（QChartWidget/QChartRenderer/QChartCamera3D include 投影与 QChartWorldBox）、`utils`（ProjectionToolKit 组合工厂）、`animation`（.cpp 用 QInterpolatedProjection/QChartProjection）。
- 纯几何模块：**不知道 Axis、Widget、Renderer**；映射函数只吃 Numeric 数字、产出几何坐标。

## 2. 文件与类清单

| 文件（include/projection/ + src/projection/） | 类 | 说明 |
|---|---|---|
| QChartProjection.h | `QChartProjection`（基类）+ `CoordinateSystem{Cartesian,Polar,Functional}` | 2D 投影抽象 |
| QCartesianProjection.h | `QCartesianProjection` | 恒等映射（Numeric ≡ View） |
| QPolarProjection.h | `QPolarProjection` | 极坐标 |
| QFunctionalProjection.h | `QFunctionalProjection` | lambda 定义的前向/反向映射 |
| QInterpolatedProjection.h/.cpp | `QInterpolatedProjection` | 两投影间插值（动画） |
| QChartProjectionFactory.h | `QChartProjectionFactory`（静态） | `create(CoordinateSystem)` / `createFunctional(...)` |
| QChartProjection3D.h | `QChartProjection3D`（基类，header-only）+ `QChartWorldBox` | 3D 投影抽象 |
| QChartCartesianProjection3D.h | `QChartCartesianProjection3D` | 恒等（`isIdentityMapping()==true`） |
| QChartCylindricalProjection3D.h | `QChartCylindricalProjection3D` | 柱坐标 `x=r·cosθ, y=r·sinθ, z=z` |
| QChartSphericalProjection3D.h | `QChartSphericalProjection3D` | 球坐标 `x=r·cosθ·sinφ, y=r·sinθ·sinφ, z=r·cosφ` |
| QChartFunctionalProjection3D.h | `QChartFunctionalProjection3D` | lambda 定义 toWorld/fromWorld |

全部无 Q_OBJECT（无 moc）；仅 QInterpolatedProjection 有 .cpp（其余 header-only，库源列表只有 `src/projection/QInterpolatedProjection.cpp`）。

## 3. 公共 API 一览

**QChartProjection（2D 基类）**
- `type()` → CoordinateSystem；`toCartesian(num0,num1)` / `fromCartesian(x,y)`（Numeric↔View，纯几何，无需 dataBounds）。
- `computeDataBounds(viewRect)` / `computeViewRect(dataBounds)`（dataBounds↔viewRect 包络互转）。
- `defaultDataBounds()` = `QRectF(0,0,10,10)`（2D 默认）。
- `createPath(dataCurve, segments)`：采样 `dataCurve(t)→(num0,num1)` 经 toCartesian 连成 QPainterPath。

**2D 子类**：QCartesianProjection（恒等）、QPolarProjection（极径/极角）、QFunctionalProjection（前向/反向 lambda，backward 可选=nullptr）、QInterpolatedProjection（`setInterpolation(a,b,t)` 型，type() 跟随 b）。

**QChartProjectionFactory**：`create(Cartesian/Polar)`；`create(Functional)` 拒绝（缺 lambda → nullptr + qWarning）；未知枚举 → 回退 Cartesian + qWarning。`createFunctional(forward, backward=null, defaultBounds, dataToView=null, viewToData=null, name0="x", name1="y")`。

**QChartProjection3D（3D 基类，header-only）**
- `dimensionName(dim)`（x/y/z 轴名）；`toWorld(n0,n1,n2=0)` / `fromWorld(w)`（奇点 NaN 策略延续）。
- `computeWorldBounds(dataMin, dataMax)`：三轴 **16 段网格采样**（17³=4913 点）toWorld 取有限点 min/max；全 NaN → 回退 `{dataMin, dataMax}`。
- `defaultDataBounds()` = `{(0,0,0),(10,10,10)}`（与 2D 对齐）；`samplingSegmentsHint()`（弯曲投影 32、Cartesian3D 2）；`isIdentityMapping()`（默认 false，Cartesian3D true）。
- `createPath3D(dataCurve, segments=64)`：t∈[0,1] → Numeric 三元组 → World 折线，**NaN 断路径**（子路径列表）。

**3D 子类**：QChartCartesianProjection3D（恒等 + 快速通道）、QChartCylindricalProjection3D（r,θ°,z）、QChartSphericalProjection3D（r,θ°,φ°）、QChartFunctionalProjection3D（lambda 版 toWorld/fromWorld）。

## 4. 信号槽

无（非 QObject 模块）。投影变化经宿主（QChartWidget::setProjection / QChartWidget3D::setProjection3D）触发重算与重绘，不直接发信号。

## 5. 核心机制

1. **纯几何映射**：2D `toCartesian` 不需要 dataBounds（Numeric 直接几何映射到 View Cartesian），dataBounds 只用于包络互转（computeDataBounds/computeViewRect）。
2. **奇点 NaN 策略**：反向映射在奇点（极径 r=0、柱/球 atan2 退化）产出 NaN 哨兵，调用方跳过（不画、不参与包围盒）。
3. **采样法包围盒**：通用坐标系的极值不在参数角上（如球面），必须采样（16³ 正向 / 5³ 反向）而非取角点。
4. **恒等快速通道**（D23）：`isIdentityMapping()` 下反算免采样、图元免 toWorld、段数=2（两点直线）——Cartesian3D 专用。
5. **插值投影**：QInterpolatedProjection 是动画基座（QProjectionSwitchAnimation 使用），见 docs/animation/deepdive_animation.md。

## 6. 核心函数一览表（含文件位置）

| 函数 | 职责 | 调用方 | 文件 |
|---|---|---|---|
| `QChartProjection::toCartesian/fromCartesian` | 2D Numeric↔View（纯几何） | DrawContext/Axis 边框轴/Layer | include/projection/QChartProjection.h |
| `QChartProjection::computeDataBounds/computeViewRect` | 包络互转 | QChartWidget（dataBounds 维护） | include/projection/QChartProjection.h |
| `QChartProjectionFactory::create/createFunctional` | 投影实例工厂 | QChartWidget::setProjection / demo / ProjectionToolKit | include/projection/QChartProjectionFactory.h |
| `QChartProjection3D::computeWorldBounds` | World 包围盒 16³ 采样（全 NaN 回退） | QChartWidget3D::fitWorld（A3 链） | include/projection/QChartProjection3D.h |
| `QChartProjection3D::createPath3D` | 数据曲线→World 折线（NaN 断路径） | Layer3D 网格/系列路径生成 | include/projection/QChartProjection3D.h |
| `QChartCartesianProjection3D::isIdentityMapping` | 恒等快速通道开关 | QChartWidget3D::recomputeDataBounds3D / Layer3D | include/projection/QChartCartesianProjection3D.h |
| `QInterpolatedProjection`（setInterpolation 等） | 投影间插值 | QProjectionSwitchAnimation | src/projection/QInterpolatedProjection.cpp |

## 7. 设计文档对应

- 2D 投影：`docs/design/design_notes.md`（§Projection 统一性、§viewRect 与 dataBounds）。
- 3D 投影家族：`docs/design/design_3d.md`（§5 Projection3D 家族、§5.3 参数化示例）。
- 5³ 反算/快速通道：`docs/design/design_3d.md` §2.2 + `design_3d_axes.md` §5.4（D23）。
- 决策：D23（5³ 反算 + 快速通道）、D-3D-5（Projection3D 家族）、D-3D-10（Phase 3 预留）。
