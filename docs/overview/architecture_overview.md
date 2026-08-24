# architecture_overview.md —— QChartWidget 架构总括

> t53 文档套件 · 总括（顶层文档，不属任何模块）
> 配套：各模块 `docs/<module>/module_<module>.md`（分模块细节）、`docs/<module>/deepdive_*.md`（核心计算深挖）、设计文档 `docs/design/*.md`（五空间/3D/Phase 3 权威出处）。
> 适用范围：重构后目录布局（`include/` 与 `src/` 下 animation/axes/core/layers/series/utils/projection 七模块）。

---

## 1. 定位与构建

QChartWidget 是一个基于 QPainter 的 Qt6 图表库（五空间模型），同一套源码可跨 Windows（MSVC/MinGW）/Linux（GCC）编译。形态为**静态库**（D4，未加导出宏，暂不支持 shared），消费方为 `Test/`（demo）、`TestUnit/`（QTest 单元测试）、`Test/bench/`（性能基准）。

- 构建入口：根 `CMakeLists.txt`；Qt6 组件 Core/Gui/Widgets/Svg/OpenGL/OpenGLWidgets（+Test）。
- 目录：库源码在 `include/<module>/`（头）+ `src/<module>/`（源）；demo/test/bench 原位不动。
- 三条红线贯穿全库：**moc 所有权**（AUTOMOC 仅库 target，消费方 OFF，测试头手动 `qt6_wrap_cpp`）；**行为零变化优先**（纯重构任务以零回归为硬门槛）；**git 由用户操作**（D5，团队只改文件）。

## 2. 五空间模型（2D 权威，design_notes.md §五空间模型）

| # | 空间 | 类型 | 产生者/持有者 | 用途 |
|---|---|---|---|---|
| 1 | Data | 任意类型（Axis 子类决定） | Series 存储 | 用户数据源 |
| 2 | Numeric | qreal | `Axis::toNumeric()` | 跨类型统一数字，喂给 Projection |
| 3 | View (Cartesian) | QPointF（物理长度单位） | `QChartCamera2D::m_viewRect` | Pan/Zoom、空间变换 |
| 4 | ViewNorm | QPointF（[0,1]²） | Widget 内部（线性归一） | 归一化 → 像素 |
| 5 | Pixel | QPointF（像素） | QPainter | 最终绘制 |

正向（绘制）：`Data ─[Axis::toNumeric]→ Numeric ─[Projection::toCartesian]→ View ─[线性]→ ViewNorm ─[线性]→ Pixel`
反向（交互）：`Pixel ─[逆线性]→ ViewNorm ─[逆线性]→ View ─[Projection::fromCartesian]→ Numeric ─[Axis::fromNumeric]→ Data`

关键语义：**Data ≠ Numeric**（即使同为 qreal，QValueAxis 恒等、QLogAxis 是 log10 变换，语义独立）；**不存在 DataNorm 空间**（旧设计归一化到 [0,1] 是绕路）；**边框轴捷径**（仅 Cartesian：Numeric 直接线性插值到 plotArea 边缘，不经 Projection）。

## 3. 3D 扩展链路（design_3d.md §2）

3D 在五空间之上增加 **World** 空间：

```
Data ─[Axis::toNumeric]→ Numeric ─[Projection3D::toWorld]→ World(x,y,z)
     ─[Camera3D::viewProjectionMatrix]→ Clip ─[QChartMath::clipToNdc]→ NDC ─[视口]→ Pixel
```

- **2D 是 3D 的退化特例**（D1/D-3D-2）：正交俯视 + 恒等映射 ≡ 2D `cartesianToPixel`，像素断言锁死。
- 相机 = 纯映射器：viewCube（World 轴对齐盒）+ orientation（yaw/pitch）+ fovY 为主状态，position/lookAt/up/near/far 派生只读（D21）。
- 深度语义：`depth = −viewZ`（越大越远），painter's algorithm **深度降序**（远先画）（D16）。

## 4. 三层分离（D24 + 2D 同构）

| 层 | 职责 | 归属 |
|---|---|---|
| 数据层 | Series 只存 Data，零耦合（不持 Axis/Projection/Widget 引用） | series 模块 |
| 数值化与几何层 | Axis 三件事（数值化/刻度生成/标签格式化）；3D 编排器只产 Numeric 几何 | axes 模块（QChartAxes3D 非 Q_OBJECT 编排器） |
| 渲染层 | Layer 收集图元（collectPrimitives）+ Renderer 绘制/拾取 | layers + core 模块 |

3D 系列的零耦合通过 **ProjectFn3D 闭包**实现（D15）：`collectPrimitives/draw` 注入 `std::function<QChartProjectedPoint(const QDataPoint3D&)>`，Data→Numeric→World→Screen 全链在 Layer3D 组装闭包内完成。

## 5. 统一后端原则（D26，用户定案）

后端开关**同时决定渲染与拾取**，禁止混搭：

| 后端 | 渲染 | 拾取 |
|---|---|---|
| CPU（QPainter） | QPainterChartRenderer（QPainter 路径） | QChartHitTester CPU 实现（2D 像素 in 多边形 / 3D 屏幕近邻） |
| GPU（OpenGL 3.3 Core） | QOpenGLChartRenderer（VBO 批次，GLSL 330） | QChartHitTester GPU 实现（ID 帧颜色编码，design_phase3 §8） |

`QCHART_GL=0` 环境变量兜底；GL 未就绪时 GlHost 隐藏、回退纯 QPainter（§5.1 透明像素教训）。

## 6. 模块目录与依赖图

```
include/ 与 src/ 各含：
  animation/  axes/  core/  layers/  series/  utils/  projection/
```

依赖图（头级 include 实测；`→` = "依赖"）：

```
                    ┌──────────────┐
                    │    utils     │   QChartMath / QChartDebug / QDataPoint(3D) / QDataRect / ProjectionToolKit
                    └──────┬───────┘
                           │ ▲
          ┌────────────────┘ └─────────────┐
          ▼                                ▼
   ┌────────────┐                  ┌──────────────┐
   │ projection │                  │     core     │   widget/camera/renderer/hitTester/theme/legend/GL
   └──────┬─────┘                  └───┬──────┬────┘
          │                            │      │
          ▼                            ▼      ▼
   ┌────────────┐    ┌────────────┐  ┌──────────────┐
   │    axes    │◄───│   layers   │  │    series    │   axes: QChartAxis(2D/3D 共用) + QChartAxes3D
   └────────────┘    └────────────┘  └──────────────┘
                              │
                              ▼
                       ┌────────────┐
                       │ animation  │   .cpp 级依赖 core/projection/series（头级仅 Qt + 基类）
                       └────────────┘
```

实测边：`axes→{core,projection}`；`core→{layers,projection,utils}`；`layers→{axes,core,series}`；`series→{core,utils}`；`projection→utils`；`utils→projection`（ProjectionToolKit）；animation 头级无跨模块边（依赖在 .cpp）。

⚠ **core↔layers 头级小环**（`QChartWidget.h` include `QChartLayer.h`，`QChartLayer.h` include `QChartAxis.h`→`QChartCamera.h`）：`#pragma once` 下安全，设计使然——DrawContext 定义于 QChartAxis.h、QChartScene 定义于 core 的 QChartRenderer.h，均为跨模块共享类型。依赖图按"逻辑层次"阅读，不追求 DAG。

## 7. 决策索引 D1~D31 速查表（权威全文：ROADMAP.md §1.3 决策记录）

| # | 决策 | 一句话 | 落点模块 |
|---|---|---|---|
| D1 | 相机独立成类 | QChartCamera 拥有 viewRect + fit + toPixel/fromPixel；2D 是 3D 退化特例 | core |
| D2 | 渲染器独立成类分两层 | QChartRenderer + QPainterChartRenderer，`render(scene, device)` 参数化 QPaintDevice → 导出白捡 | core |
| D3 | 动画优先 QPropertyAnimation | 标量属性动画直接用 QPropertyAnimation；只有单属性表达不了才写自定义动画 | animation |
| D4 | 静态库 | 默认 STATIC；共享库需导出宏（暂缓） | CMake |
| D5 | git 全部由用户操作 | 团队只改文件，不 commit/push/pull | 协作 |
| D6 | 每阶段验收标准 | Linux 构建绿 + ctest 全绿 + demo 行为不回归 | 流程 |
| D7 | 测试应用类型 | TestUnit 用 QGuiApplication；非 Windows 跑 offscreen | TestUnit |
| D8 | Qt 日志规则限制 | 通配符 `*` 只能出现在模式末尾；verbose 分类默认静默 | utils（QChartDebug） |
| D9 | 逐任务审查 | 每个实现任务后 reviewer 独立实跑审查 | 流程 |
| D10 | 设计先行 | 新功能先设计（问卷→方案→文档→确认）再动代码 | 流程 |
| D11 | QChart 命名规范 | 公共类型一律 QChart 前缀；裸枚举并入类内 | 全局 |
| D12 | 颜色 override 双槽模型 | 显式 override ?? 主题默认；`clear*()` 回退主题 | core/axes/series |
| D13 | 导出走 renderUncached | PNG/SVG/PDF 无缓存直绘；PDF 恒填背景；跳过调试黄框 | core（QChartWidget） |
| D14 | 小问题 reviewer 自修 | 轻微问题 reviewer 直接修复自验，不回传 | 流程 |
| D15 | 3D 系列全链闭包 ProjectFn3D | collectPrimitives/draw 注入 std::function 全链闭包；系列只存 Data 零耦合 | series/layers |
| D16 | painter's algorithm 深度降序 | depth=−viewZ 越大越远；图元深度降序绘制；nearCoversFar 断言 | layers/core |
| D17 | Widget 形态 QChartWidget3D 子类 | 3D 场景/交互隔离在子类；基类仅 buildScreenScene 虚化等两处最小改动 | core |
| D18 | 双 Widget 联动信号互发 (u,v) | 不共享 Series；同构副本 + 相同 Axis range；uvHovered/uvSelected/uvHoveredEnd 单向传值 | core |
| D19 | 3D 渲染边界：数值型 Data | 正确性保证限于数值型；非数值 Axis 转换记入 Phase 3 | series |
| D20 | orthographicBox（R5 废弃） | 正交投影盒显式访问器；R5 起 viewCube 即投影盒，删除 | core（历史） |
| D21 | viewCube 主状态相机（R5） | 取景框 = World 盒 + orientation + fovY；position/lookAt/up/near/far 派生只读 | core |
| D22 | 平移无鼠标手势（R6） | 拖拽=转相机角度；viewCube 平移仅 API/动画驱动 | core |
| D23 | 5³ 反算 + 笛卡尔快速通道（R6） | viewCube→dataBounds 用 5×5×5=125 点采样；isIdentityMapping 免采样免 toWorld | projection/core |
| D24 | 3D 参照系分层与编排 | 网格与系列统一深度排序 + kGridDepthBias=1e-3；QChartAxes3D 非 Q_OBJECT 编排器（三层分离） | axes |
| D25 | OpenGL 3.3 Core + GLSL 330 | QOpenGLWidget + ShaderProgram + VBO/VAO；字符串运行期编译；6.4.x setShareContext 守卫 | core（QChartGL） |
| D26 | 统一后端原则 | 后端开关同决渲染与拾取，禁止混搭；QCHART_GL=0 兜底 | core |
| D27 | GPU 拾取归属 QChartHitTester | 拾取不是 renderer 职责；hitTest 三实现统一进 QChartHitTester；uvHovered 零改动 | core |
| D28 | 内存预算与图元瞬态化 | 1M 点 ≤70MB；collectPrimitives 瞬态缓存；数值型 float3 权威存储 12B/点 | series/core |
| D29 | z-buffer 分层与两后端等价 | GL Grid/Series 开深度 + glPolygonOffset（等价 kGridDepthBias）；与 QPainter 深度语义像素级等价断言 | layers/core |
| D30 | GPU 投影路径 | World float3 VBO attribute + 顶点着色器仅 viewProj uniform；每帧零 CPU 投影 | core（GL 渲染器） |
| D31 | 硬件基线验收 | 验收数字绑定硬件/构建类型/分辨率/双工具链；llvmpipe 仅冒烟不作基准 | 流程 |

## 8. 跨模块关键类型索引（定义处）

| 类型 | 定义文件 | 用途 |
|---|---|---|
| `QChartWorldBox` | `include/projection/QChartProjection3D.h` | World 轴对齐盒（fit/反算/批次） |
| `QChartProjectedPoint{screen, depth, world}` | `include/core/QChartCamera3D.h` | 投影结果；GL 路径携带 World 源点 |
| `QChartPrimitive` / `QChartScene` | `include/core/QChartRenderer.h` | 图元（含 Layer/depth/dataIndex）+ 场景图 |
| `ProjectFn3D` | `include/series/3d/QChartSeries3D.h` | 3D 全链闭包类型 |
| `HitResult` / `PickRecord` | `include/core/QChartHitTester.h` | 统一命中结果 / GPU 拾取记录 |
| `DrawContext` | `include/axes/QChartAxis.h` | 绘制上下文（跨层共享） |
| `kGridDepthBias = 1e-3` | `include/core/QChartRenderer.h` | 网格深度偏置（两后端等价） |
| `ViewRectFitMode` / `FitStrategy` | `include/core/QChartCamera.h` | 2D 视窗 fit 策略 |

## 9. 测试与验收骨架

- ctest：offscreen 下 **180 PASS + 2 SKIP**（20 测试类；2 SKIP = GL 相关类在无真实 GL 环境 QSKIP）。
- demo：`QChartDemo [name...]` 11 个 demo（2D 8 + 3D 3），无参=全部；GL 冒烟在 wayland/xcb 实跑（llvmpipe 软渲染仅冒烟不作基准，D31）。
- 性能口径：相机旋转 200 帧中位、排除首帧 shader 编译；内存 RSS 实测（D28/D31）。
- 文档一致性：本套件（overview/module/deepdive）与 `docs/design/*`（权威设计）引用关系见各 module 文档 §7「设计文档对应」。
