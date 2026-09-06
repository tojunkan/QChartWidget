# S0 Stage Record — 轴渲染管线（CPU+GL 双后端）

> ⚠ v1 记录：本文件为轴渲染阶段早期快照，widget 容器化与 3D 轴渲染已并入本阶段（用户决策），阶段记录将在代码落地后由 v2 合并记录取代

> 阶段：S0（架构重构阶梯第一步）
> 代码基线：`/home/unidu/dsh/QChartWidget`（新根 CMakeLists S0 子集；旧版 `CMakeLists_OLD.txt` 保留）
> 状态：审查通过（t2 needs_revision → t3 修复 F1/F2 后自验完成；t4 复审查 verdict=pass，含 GL 刻度点出墨硬证据）
> 文档准则：本目录记录以**当前新架构代码**为准；docs/ 下旧文档若描述重构前架构，已由 S0 每类文档覆盖。

## 1. 架构决策（S0 落地）

- **layer 为渲染单位**：相机属 layer/场景（S0 内 `QChartScene.camera` 持 2D 相机；3D 相机待 3D 阶段）。
- **图元提交 Numeric 坐标**：`QChartAxis::drawAtPosition()` 已图元化——只向 `QChartScene` 追加 `QChartPrimitive`（Path/Point）与 `QChartTextLabel`，不直接 QPainter 绘制（2D/3D 通用）。
- **renderer 四步管线**：`collect`（Widget/Layer 完成，S0 由测试/夹具直接装配）→ `transformNumericToCartesian` → `cullAndResolveLabels` → `drawPrimitives` → `drawLabels`。
- **projection 统一 `QVector3D` 接口**：`QChartAbstractProjection::toCartesian/fromCartesian(const QVector3D&)` + `dimension()` + `glslToCartesian/glslFromCartesian` 字符串（GPU 注入 shader 动态编译；GLSL 契约为**表达式**：输入 `vec3 num`，输出 Cartesian 坐标表达式，着色器内包装为 `vec3 num = a_pos; vec3 cart = <expr>;`）。
- **五空间保留**：Data → Numeric → Cartesian（View Cartesian）→ ViewNorm（NDC）→ Pixel；Axis 只做 Data↔Numeric，Projection 做 Numeric↔Cartesian，Camera 做 Cartesian↔Pixel（2D 线性 / 3D 矩阵），Renderer 编排。
- **CPU 与 GPU 渲染器对等**：`render(QChartScene&, QPaintDevice*)` 同一入口；差异仅在步骤 2/3 实现（CPU 端 CPU 变换+精确裁剪+QPainter；GPU 端 shader 变换、批次上传、GL 绘制 + QPainter 标签层）。
- **dataDirty/viewDirty 模型**：`QChartRenderer::m_viewDirty`（默认 true）驱动步骤 2/3 重算；`m_visibilityCache` 与 scene.primitives 一一对应；Layer 侧保留 `m_dataDirty` 待 Widget 阶段消费。

## 2. S0 子集与排除清单

### QCHART_SOURCES（库源，S0 子集）

| 模块 | 文件 |
| :--- | :--- |
| axes | `src/axes/QChartAxis.cpp`、`src/axes/2d/QValueAxis.cpp`、`src/axes/2d/QLogAxis.cpp`、`src/axes/2d/QDateTimeAxis.cpp`、`src/axes/2d/QBarCategoryAxis.cpp` |
| core | `src/core/QChartCamera.cpp`、`src/core/QChartRenderer.cpp`、`src/core/QPainterChartRenderer.cpp`、`src/core/QOpenGLChartRenderer.cpp`、`src/core/QChartGL.cpp` |
| layers | `src/layers/QChartLayer.cpp` |
| projection | `src/projection/QInterpolatedProjection.cpp`（2D 投影族其余为 header-only） |

### 后置清单（未入 S0，后续阶段加回）

- 3D：`QChartAxes3D`、`QChartCamera3D`（含其 CPU/GL 渲染分支，见 §4）、`QChartLayer3D`、3D 投影族、`QChartWidget3D`。
- Widget/消费方：`QChartWidget`、`QChartAbstractWidget`、theme/legend/导出/动画/bench/demo。
- 拾取：`QChartHitTester` 实现与 `QChartPrimitive::Layer` 枚举（`PickRecord::layer` 字段已注释，标注“拾取整体后置”）。
- Series：`QChartSeries` 族及其 Layer 管理方法（addSeries/removeSeries/hook…/drawAllSeries 注释保留，随 Series 阶段恢复）。

## 3. 测试矩阵与结果

| 层 | 内容 | 结果 |
| :--- | :--- | :--- |
| Test/unit `TestAxisPipeline` | Cartesian/Polar 轴脊+刻度点落屏像素断言；网格开/关 × 标签开/关 2×2 组合 | offscreen 6/6 绿 |
| Test/integration `TestAxisMatrixCpu` | 投影{Cartesian,Polar}×网格{开/关}×标签{开/关} = 8 组合 | offscreen 8/8 绿 |
| Test/integration `TestAxisMatrixGl` | 同上 8 组合；真实 GL（wayland）实跑 | wayland 8/8 绿（Mesa/llvmpipe 4.5 Core）；offscreen 整类 QSKIP |
| 刻度点邻域断言（t3） | Cartesian 刻度臂（±0.3 cart、半径 2 邻域）CPU+GL 共用 | GL 真实出墨（2px 点） |
| ctest | `QChartUnitTests` + `QChartIntegrationTests` | offscreen 2/2 通过；构建 0 error/0 warning |

GL 环境记录：`platform=wayland vendor=Mesa renderer=llvmpipe (LLVM 20.1.2, 256 bits) version=4.5 (Core Profile) Mesa 25.2.8`。

## 4. 修复记录（S0 内）

| # | 位置 | 问题 | 修复 |
| :--- | :--- | :--- | :--- |
| R1 | `src/axes/QChartAxis.cpp` | drawAtPosition 定义重复默认参数（头已声明） | 删除 cpp 侧 `=72/=true` |
| R2 | `src/axes/QChartAxis.cpp` | 轴标签以 sourceId=-1/refPrimitiveId=-1 自由标签提交，两后端 cull 均判不可见 | 标签绑定本刻度中心点图元（`refPrimitiveId = centerIdx`） |
| R3 | `include/core/QChartHitTester.h` | PickRecord::layer 引用已删的 QChartPrimitive::Layer | 注释字段并标注“拾取后置” |
| R4 | `QPainterChartRenderer`（.h/.cpp） | QCube include 缺失 / drawLabels 声明未定义 / 3D 分支引未入 S0 的 QChartCamera3D typeinfo+moc 致链接失败 | 补 include；实现 drawLabels/drawLabels2D；3D 分支 `#if 0` 后置（代码保留，待 3D 阶段恢复）；QCube 引用随 3D 恢复 |
| R5 | `QPainterChartRenderer.cpp`（F1，t3） | cullAndResolveLabels 对 sourceId=-1 图元 `lastVisibleIndex[-1]` 越界写（UB） | 加 `0≤sourceId<size` 守卫 |
| R6 | `QPainterChartRenderer.cpp` | 零面积 AABB（水平/垂直轴脊）`QRectF::intersects` 返回 false 致整条轴脊被误裁 | 退化盒微扩 1e-6 后判交 |
| R7 | `QOpenGLChartRenderer.cpp` | 4 个 narrowing warning | float 显式转换 |
| R8 | `QOpenGLChartRenderer.cpp` | GL drawLabels 用零尺寸 rect drawText，不产生文字 | 改走共享 `QChartRenderer::drawLabel` |
| R9 | `QOpenGLChartRenderer.cpp`（F2，t3） | 未 `glEnable(GL_PROGRAM_POINT_SIZE)` → GL Point（轴刻度 7 点）不可见 | 绘制前 enable、Point pass 后 disable |
| R10 | `src/core/QChartGL.cpp` | glslToCartesian() 为表达式，shader 模板按语句注入且未定义 `num` → GLSL 编译失败 | 注入 `vec3 num = a_pos; vec3 cart = <expr>;`（空投影回退 `num`） |
| R11 | `QChartLayer`（.h/.cpp） | series/hitTest/Widget 注入依赖未入 S0 的 QChartSeries.cpp/QChartAbstractWidget | 注释保留（非删除），随 Series/Widget 阶段恢复；connect 指针语法修复；去未用局部变量 |
| R12 | `src/projection/QInterpolatedProjection.cpp` | logProjection 类别定义原在未入 S0 的 QChartWidget.cpp，QPolarProjection 等 header qCDebug 链接失败 | 临时迁入 QInterpolatedProjection.cpp（**遗留：Widget 阶段把 QChartWidget.cpp 加回时，删除此处定义、恢复原文件定义**） |

## 5. S0 涉及类文档登记（每类一文档，六段范式）

> QCube 为值类型（无 Qt 事件/信号），采用值类型文档形态并在本总括登记。
> 本表 22 项文档已按当前新架构代码全部就位（QChartAxis.md 亦为新架构版）；表内路径即交付文件。

| # | 类 | 文档 | 形态 |
| :---: | :--- | :--- | :--- |
| 1 | QChartAxis | `docs/axes/QChartAxis.md` | 六段 |
| 2 | QValueAxis | `docs/axes/QValueAxis.md` | 六段 |
| 3 | QLogAxis | `docs/axes/QLogAxis.md` | 六段 |
| 4 | QDateTimeAxis | `docs/axes/QDateTimeAxis.md` | 六段 |
| 5 | QBarCategoryAxis | `docs/axes/QBarCategoryAxis.md` | 六段 |
| 6 | QChartAbstractCamera | `docs/core/QChartAbstractCamera.md` | 六段 |
| 7 | QChartCamera（2D） | `docs/core/QChartCamera.md` | 六段 |
| 8 | QChartScene | `docs/core/QChartScene.md` | 六段 |
| 9 | QChartPrimitive | `docs/core/QChartPrimitive.md` | 六段（无信号/事件章节为空） |
| 10 | QChartTextLabel | `docs/core/QChartTextLabel.md` | 六段（无信号/事件章节为空） |
| 11 | QChartRenderer | `docs/core/QChartRenderer.md` | 六段 |
| 12 | QPainterChartRenderer | `docs/core/QPainterChartRenderer.md` | 六段 |
| 13 | QOpenGLChartRenderer | `docs/core/QOpenGLChartRenderer.md` | 六段 |
| 14 | QChartGL | `docs/core/QChartGL.md` | 六段（全静态，无事件/信号章节为空） |
| 15 | QChartLayer | `docs/layers/QChartLayer.md` | 六段 |
| 16 | QChartAbstractProjection | `docs/projection/QChartAbstractProjection.md` | 六段 |
| 17 | QChartProjection（2D 基类） | `docs/projection/QChartProjection.md` | 六段 |
| 18 | QCartesianProjection | `docs/projection/QCartesianProjection.md` | 六段 |
| 19 | QPolarProjection | `docs/projection/QPolarProjection.md` | 六段 |
| 20 | QFunctionalProjection | `docs/projection/QFunctionalProjection.md` | 六段 |
| 21 | QInterpolatedProjection | `docs/projection/QInterpolatedProjection.md` | 六段 |
| 22 | QCube | `docs/utils/QCube.md` | 值类型文档形态（总括登记） |

## 6. 遗留与后续提示

- `logProjection` 临时定义位置（QInterpolatedProjection.cpp）——Widget 阶段恢复时处理（R12）。
- 3D 渲染分支（QPainterChartRenderer #if 0 块）、Layer Series 管理、hitTest 注释块——分别随 3D / Series / 拾取阶段恢复，S0 范围无行为损失。
- `QChartLayer::drawGrid` 的“每网格脊带标签”语义沿用现状；S0 测试矩阵以直接 `drawAtPosition` 装配等价几何。
- 新增测试文件仅注册 Test/unit 与 Test/integration 列表；库源列表（QCHART_SOURCES）S0 无净改动；全程无 git 写操作。
