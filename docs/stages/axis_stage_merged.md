# Axis Stage Record（v2 合并记录）— 轴渲染阶段全记录

> 阶段：轴渲染阶段（v2 合并记录）
> 范围：原 S0（CPU 轴）+ S1（GPU 轴）**合并为一个轴渲染阶段**（用户决策），并纳入 widget 容器化首改（批次 A）与 3D 轴渲染（批次 B1–B2）、测试矩阵补齐（批次 B3）、阶段 demo（批次 B4）。
> 取代：本文件取代 `docs/stages/S0_axis_pipeline.md`（v1 早期快照，头部已标注"已被 axis_stage_merged.md 取代"）；v1 的架构决策/修复清单 R1–R12/测试矩阵作为子集并入下文并保留原编号。
> 状态：代码冻结，双平台验证全绿（本机 Linux wayland/Mesa llvmpipe 4.5 Core GL 实跑 + Windows 实机 Intel UHD620 dpr=1.5；offscreen ctest 常驻）。审查闭环：t2(needs_revision)→t3→t4(pass)；t7(needs_revision)→t8→t9/t11(pass)；t15(pass)；t17/pass；t19/pass；t21(pass)。
> 基线：工作区 `/home/unidu/dsh/QChartWidget`（新根 CMakeLists 轴阶段子集；旧版 `CMakeLists_OLD.txt`/`main_OLD.cpp`/`LEGACY_DEMOS_OLD.txt` 保留）；全程无 git 写操作。

## 1. 批次结构与落地内容

| 批次 | 内容 | 任务 | 验证 |
| :--- | :--- | :--- | :--- |
| S0（原 v1 记录） | CPU 轴渲染管线（QChartAxis 图元化 + QChartLayer 网格语义 + QPainterChartRenderer 四步管线 + 投影族 CPU/GLSL 契约） | t1–t5 | unit 6、CPU 矩阵 8、GL 8（wayland）、审查 pass |
| 批次 A | widget 容器化首改：无相机/无 buildScene/plotArea 对齐 GL 画布（纯容器架构，用户确认） | t6–t15 | widget 冒烟、GL 宿主取证、HiDPI 修复、审查闭环 |
| 批次 B1 | 3D 轴渲染核心入编译面：QChartAxes3D/QChartLayer3D/QChartCamera3D + renderer 3D 恢复（CPU painter 算法 + GL 批次通路） | t16–t17 | 3D CPU/GL 冒烟、审查 pass |
| 批次 B2 | QChartWidget3D 轻量 3D 容器重写（单 layer3D 托管、相机归层） | t18–t19 | widget3D CPU/GL 冒烟、审查 pass |
| 批次 B3 | 测试矩阵补齐：TestAxisEdge 专项、CPU 矩阵扩 16、3D 轴矩阵 8+8（CPU/GL）、spherical 冒烟（CPU/GL） | t20–t21 | offscreen ctest 2/2、wayland GL 全集、审查 pass |
| 批次 B4 | 阶段 demo：QChartDemo（demo_axis 2D/3D、main 参数化、QCHART_DEMO_SHOT 截图冒烟） | t22 | demo 全绿（含 GL 宿主 FBO 取证） |

## 2. 架构决策（最终形态）

v1 决策全部保留（layer 为渲染单位/图元提交 Numeric/drawAtPosition 图元化/内外分离/五空间/dataDirty-viewDirty/投影 GLSL 表达式契约/CPU-GL 对等 render 入口），增量决策：

- **widget 层 = 纯容器**：QChartAbstractWidget（QWidget 基）不含相机、不含 buildScene、不含图例/导出/拾取/动画；invalidateBackground/invalidateForeground 语义由 renderer viewDirty/layer dataDirty 取代。
- **相机归 layer**：2D `QChartLayer::m_camera`（QChartCamera 值成员）、3D `QChartLayer3D::m_camera3D`（值成员）；`scene.camera`/`scene3D.camera` 于构造注入指向层相机。
- **容器管理**：layers 列表（非持有）+ 唯一 plotArea（布局计算）+ `plotAreaChanged/projectionChanged` 广播；边距+边框轴 sizeHint 外边距参与 plotArea 计算（calculatePlotArea，防止边距吞画布：保底 40px）。
- **GL 宿主 = plotArea 对齐的 QOpenGLWidget 子控件**（GlPlotWidget，替换旧全窗 GlHost）：GL 只画 plotArea 内，plotArea 外（边框轴 drawAtEdge/标题）由外层 QPainter 绘制；CPU 后端不建 GL 子控件；事件 WA_TransparentForMouseEvents + 外层 final 分发到空虚拟钩子。
- **每帧渲染**：`renderLayers`（CPU：逐 layer collectPrimitives → renderer.render）与 `renderLayersGL`（GL：图元入 FBO、标签经透明 QImage 中间层 SourceOver 合成，plotArea 局部坐标）；pushContextToLayers 渲染前同步 plotArea/投影/背景（幂等）。
- **viewRect/dataBounds 驱动链**：`axis sugar（setRange）→ projection.computeViewRect → 各层相机 viewRect`（onBeforePaint 单次同步）或反向 `viewRect → computeDataBounds（math 取向）→ legacy 取向 → layer->setNumericBounds → 轴 setRange 广播`。
- **3D 轴渲染**：`QChartLayer3D::collectPrimitives` 纯 Numeric 组装（复用轴 drawAtPosition：主轴带刻度点/标签、Box/Lattice 网格、盒 12 边），几何由 QChartAxes3D 编排器静态工具给出；CPU 后端 3D = painter's algorithm（cam3d->project 取 depth 降序）；GL 后端 3D = 同一批次管线（顶点着色器 u_viewProj 变换；widget3D 线框以 depth=2.0 decor 批次渲染：depthTest 关 = 全边可见）。
- **3D 容器形态**：QChartWidget3D = 单 QChartLayer3D 托管（构造即建并接线基类 addLayer）；默认三 QValueAxis（0..10、tick5、黑）；域盒/投影/相机便捷/fitWorld/worldToPixel 全转发 layer3D/camera3D；基类布局/广播/GL 宿主复用（3D 覆写 renderLayers/renderLayersGL/pushContextToLayers/onBeforePaint/drawExternalContent）。

## 3. 编译面与排除清单（轴阶段子集）

QCHART_SOURCES（库源，轴阶段最终）：axes `QChartAxis/QValueAxis/QLogAxis/QDateTimeAxis/QBarCategoryAxis` + `3d/QChartAxes3D`；core `QChartCamera/QChartRenderer/QPainterChartRenderer/QOpenGLChartRenderer/QChartGL/QChartAbstractWidget/QChartWidget/QChartWidget3D/QChartCamera3D`；layers `QChartLayer` + `3d/QChartLayer3D`；projection `QInterpolatedProjection.cpp`（2D/3D 投影族 header-only）。QCHART_HEADERS 对应加齐（含 widget 三件套与 3D 头，AUTOMOC）。

排除/后置（实体注释保留或 *_OLD 保留，不编译引用）：3D 系列（QChartSeries3D 族及其 layer 方法/ProjectFn3D）、theme/legend/导出/动画/拾取（QChartHitTester）/交互（事件钩子空虚）/bench、旧 11 个 demo（LEGACY_DEMOS_OLD.txt）、QChartLayer3D::addSeries3D/removeSeries3D（声明在头、定义与调用待 3D 系列阶段）、轴标题块/系列 worldCache 块（layer3D 注释恢复点）。

## 4. 修复记录

### 4.1 v1 子集（R1–R12，详见 S0_axis_pipeline.md v1）

R1 重复默认参数 / R2 标签 refPrimitiveId 绑定刻度中心点 / R3 PickRecord::layer 注释 / R4 3D 引用与 drawLabels 声明链接修复 / R5（F1）cull lastVisibleIndex 越界守卫 / R6 零面积 AABB 微扩判交 / R7 GL narrowing / R8 GL drawLabels 走共享 drawLabel / R9（F2）GL_PROGRAM_POINT_SIZE / R10 GLSL 表达式注入契约（`vec3 num=a_pos; vec3 cart=<expr>;`）/ R11 QChartLayer 注释保留 + connect 修复 / R12 logProjection 类别临时迁 QInterpolatedProjection.cpp。

### 4.2 批次 A 增量

| # | 位置 | 问题 | 修复 |
| :--- | :--- | :--- | :--- |
| A-F1 | `QOpenGLChartRenderer::drawLabels`（t8） | GL 标签整体错位 plotArea 偏移（plotArea≠(0,0) 时锚点落父系坐标） | `painter.translate(-plotArea.topLeft())`；clip 用平移后 plotArea；全窗设备平移为零零回归 |
| A-F2 | widget/渲染器（t8/t10） | 宿主 paintGL 标签合成缺失 + 恢复点注释不完整 | 外层 paintGL 不画标签；`renderLayersGL` 统一透明 QImage 中间层 SourceOver 合成上屏；注释恢复点补全 |
| A-F3 | `GlPlotWidget::paintGL`（t12 诊断 → t13） | glViewport 用逻辑尺寸：Windows dpr=1.5 FBO=498×416 而视口 332×277 → GL 内容只占 FBO 左下、中心采样落空（TestWidgetGl Windows Intel FAIL 根因，环扫描+PNG 门控诊断定位） | 视口改设备像素 `qRound(width()×devicePixelRatioF())` |
| A-F4 | HiDPI 三连（t14） | DPR>1 时 GL 点/线宽比 CPU 细、标签文字二次放大（清晰度） | ① 点/线宽：文件级 DPR 通道 `qchartSetGLPixelRatio/qchartGLPixelRatio`（GlPlotWidget 每帧注入），uploadBatches `pointSize×ratio`、Line pass `glLineWidth(ratio)`（后恢复 1.0）；② 标签 QImage 设备分辨率（plotArea×dpr + setDevicePixelRatio），QPainter 逻辑绘制 Qt 自动映射；③ 测试采样修正：grab 返回设备像素图，采样矩形按 dpr 缩放（test_widget_smoke dprRect） |
| A-F5 | TestWidgetGl/TestWidgetSmoke（t13/t14） | FBO 尺寸与采样未按 dpr | FBO 尺寸锁定断言 = hostGeo×dpr（±1px）；采样坐标 dpr 感知 |
| A-F6 | offscreen qExec 顺序（t21） | QtTest 怪癖：QSKIP 类之后的 qExec 类零执行 → offscreen ctest 静默漏跑 | 纯 CPU 类（TestAxisMatrixCpu/TestAxes3dMatrixCpu）排在 GL/QSKIP 类之前（integration main.cpp 注释记录根因） |
| A-F7 | 断言判别力缺口 D14 系列（t17/t19/t21） | 网格默认浅灰 (220,220,220) 被 isInk>40 阈值排除致 Box/Lattice 差异原不可断言；姿态判别不足；恢复点注释缺口 | 自修自验：网格可见性/姿态判别断言改造；注释补齐 |

### 4.3 批次 B1–B4 备注

B1：QPainterChartRenderer 三处 3D 恢复（drawPrimitives3D/drawLabels3D/isPrimitiveVisible3D）+ dynamic_cast 双路分发（2D/3D）；QChartCamera3D/QChartLayer3D/QChartAxes3D 入编译面。B2：QChartWidget3D 整体重写为轻量容器。B3：drawAtEdge 专项（TestAxisEdge 四方向/拒绝/样式）+ CPU 矩阵边框轴维度 8 组合 + 3D 矩阵 8+8 + spherical（CPU/GL）。B4：demo（见 §6）。B3 注意：边框轴维度组合验证"边框轴不进 plotArea 内基线"（interior diff<120）。

## 5. 测试矩阵与双平台结果（最终冻结态实测）

| 层 | 内容 | 结果（offscreen/wayland 实测） |
| :--- | :--- | :--- |
| Test/unit | TestAxisPipeline(6)+TestWidgetSmoke(3)+TestAxes3DSmoke(3)+TestWidget3DSmoke(3)+TestAxisEdge(5)——QtTest 计数含 init/cleanup | offscreen **20 passed**（ctest 常驻） |
| Test/integration CPU | TestAxisMatrixCpu：2D 8 组合 + 边框轴维度 8 组合 = **16**；TestAxes3dMatrixCpu：**8** 组合（Cartesian3D × {Box,Lattice} × {姿态 A(0,0),B(45,30)}）+ **spherical CPU 冒烟**（非恒等投影） | offscreen 全绿（常驻 ctest） |
| Test/integration GL | TestAxisMatrixGl 2D 8 组合；TestAxes3dMatrixGl 8 组合 + spherical GL；TestWidgetGl（plotArea 对齐宿主 FBO 取证）；TestAxes3DGl；TestWidget3DGl | offscreen 整类 QSKIP；**wayland（Mesa/llvmpipe 4.5 Core）全套实跑全绿**（刻度点邻域/FBO 尺寸/出墨硬证据） |
| ctest | QChartUnitTests + QChartIntegrationTests | offscreen 2/2 通过；构建 0 error/0 warning |
| Windows 实机 | Intel UHD620（dpr=1.5）：t12 诊断（环扫描+PNG 门控）定位中心墨迹 FAIL → FBO=498×416 实锤（FBO=逻辑×dpr）；t13/t14 修复后中心墨迹与 HiDPI 对齐验证 | 修复闭环（t13/t14 记录含实机 dump 证据） |
| demo shot | QCHART_DEMO_SHOT=1：demo_axis/demo_axis3d grab 存 demo_*.png；GL 后端额外 grabFramebuffer 存 demo_*_glhost.png | 冒烟通过（offscreen GL 窗口跳过不挂死，shot-skip 记录） |

## 6. Demo 清单（批次 B4）

QChartDemo（Test/demos：test.cpp 参数化主程序 + demo_axis.cpp）：
- `QChartDemo` → 全部新 demo（axis、axis3d，CPU 后端）
- `QChartDemo axis [cpu|gl]` / `axis3d [cpu|gl]` → 单 demo 可选后端
- 2D：QChartWidget + QValueAxis×2（±10、tick9）+ QChartLayer 网格 + 边框轴（外层 drawAtEdge）+ 标签；3D：QChartWidget3D + setProjection3D(Cartesian3D) + Lattice 网格（可注释切 Box）+ 域盒(-3..3) + 初始姿态 yaw45/pitch30
- QCHART_DEMO_SHOT=1 截图冒烟；旧 demo 实体保留不编译（LEGACY_DEMOS_OLD.txt）。

## 7. 遗留与后续提示

- 恢复点（随阶段取回，git 历史有旧实现，禁止 git 写操作）：theme/legend/导出/动画/交互（事件钩子已就位）/拾取（QChartHitTester）；QChartLayer3D 系列方法（addSeries3D 等声明无定义，勿引用）；Layer3D 轴标题块、makeToPixel/drawAllSeries/hitTest 注释块。
- 变更遗留：logProjection 类别临时定义于 QInterpolatedProjection.cpp（R12，Widget 消费方回归时移回 QChartWidget.cpp 位置）。
- **informational（记录在案，非阻塞）**：① decor depth 下沉——3D GL 线框以 depth=2.0（decor 批次、depthTest 关）全边可见渲染，3D series 的深度排序/批次语义后续批次接管（GL 批次 layer 推断 depth>0.5/>1.0 与 3D 视图深度并存待整理）；② 非恒等投影矩阵——3D 投影族（spherical 等）CPU 冒烟已绿，矩阵对等/GLSL 注入覆盖面随 3D series 阶段扩展；③ 进程级 DPR 通道正式 API 化——`qchartSetGLPixelRatio/qchartGLPixelRatio` 为 .cpp 自由函数（头文件不动），正式 setPixelRatio API 随渲染器配置阶段落头；④ offscreen qExec 根因（QSKIP 后零执行怪癖）记录于 integration main.cpp 注释；⑤ I1 样式单像素——颜色生效类断言采用单像素取色（可接受性记录于 t21）；⑥ QFunctionalProjection 未覆写 glsl* 纯虚 → 抽象类（用户待决策清单，functional/demo 阶段确认意图）。

## 8. 登记总表（轴阶段全部类——类-文档映射）

> 六段范式：Brief Introduction/Constant Variables/Member Variables/Member Functions/Overrided Qt Events/Signals（内容中文、表头英文）；QCube 为值类型文档形态。v1 22 项已于 t5 交付并保持有效（轴阶段内 API 未变），本表为其总括。

| # | 类 | 模块 | 文档 | 形态 | 交付 |
| :---: | :--- | :--- | :--- | :--- | :---: |
| 1 | QChartAxis | axes | docs/axes/QChartAxis.md | 六段 | v1 |
| 2 | QValueAxis | axes/2d | docs/axes/QValueAxis.md | 六段 | v1 |
| 3 | QLogAxis | axes/2d | docs/axes/QLogAxis.md | 六段 | v1 |
| 4 | QDateTimeAxis | axes/2d | docs/axes/QDateTimeAxis.md | 六段 | v1 |
| 5 | QBarCategoryAxis | axes/2d | docs/axes/QBarCategoryAxis.md | 六段 | v1 |
| 6 | QChartAxes3D | axes/3d | docs/axes/QChartAxes3D.md | 六段（非 Q_OBJECT 编排器；信号/事件章节为空） | v2 |
| 7 | QChartAbstractCamera | core | docs/core/QChartAbstractCamera.md | 六段 | v1 |
| 8 | QChartCamera（2D） | core | docs/core/QChartCamera.md | 六段 | v1 |
| 9 | QChartCamera3D | core | docs/core/QChartCamera3D.md | 六段 | v2 |
| 10 | QChartAbstractWidget | core | docs/core/QChartAbstractWidget.md | 六段（纯容器） | v2 |
| 11 | QChartWidget（2D 容器） | core | docs/core/QChartWidget.md | 六段 | v2 |
| 12 | QChartWidget3D | core | docs/core/QChartWidget3D.md | 六段（3D 容器形态） | v2 |
| 13 | QChartScene | core | docs/core/QChartScene.md | 六段（无信号/事件） | v1 |
| 14 | QChartPrimitive | core | docs/core/QChartPrimitive.md | 六段（无信号/事件） | v1 |
| 15 | QChartTextLabel | core | docs/core/QChartTextLabel.md | 六段（无信号/事件） | v1 |
| 16 | QChartRenderer | core | docs/core/QChartRenderer.md | 六段 | v1 |
| 17 | QPainterChartRenderer | core | docs/core/QPainterChartRenderer.md | 六段（3D 分支恢复后，v2 修订） | v2 修订 |
| 18 | QOpenGLChartRenderer | core | docs/core/QOpenGLChartRenderer.md | 六段（HiDPI/DPR + 3D 通路，v2 修订） | v2 修订 |
| 19 | QChartGL | core | docs/core/QChartGL.md | 六段（全静态） | v1 |
| 20 | QChartLayer | layers | docs/layers/QChartLayer.md | 六段（相机归层语义，v2 修订） | v2 修订 |
| 21 | QChartLayer3D | layers/3d | docs/layers/QChartLayer3D.md | 六段 | v2 |
| 22 | QChartAbstractProjection | projection | docs/projection/QChartAbstractProjection.md | 六段 | v1 |
| 23 | QChartProjection（2D 基类） | projection/2d | docs/projection/QChartProjection.md | 六段 | v1 |
| 24 | QCartesianProjection | projection/2d | docs/projection/QCartesianProjection.md | 六段 | v1 |
| 25 | QPolarProjection | projection/2d | docs/projection/QPolarProjection.md | 六段 | v1 |
| 26 | QFunctionalProjection | projection/2d | docs/projection/QFunctionalProjection.md | 六段（抽象类待决） | v1 |
| 27 | QInterpolatedProjection | projection | docs/projection/QInterpolatedProjection.md | 六段 | v1 |
| 28 | QCube | utils | docs/utils/QCube.md | 值类型形态 | v1 |

- 同模块流程文档（docs/<module>/xxx_flow.md 等）非类文档，不在此表；其中描述重构前架构的旧类流程文档（如 QChartAxes3D_ticks_flow）随对应模块阶段落地时再迁移。
- demo/测试类（QChartDemo、Test*）为阶段载体不入类文档表。
