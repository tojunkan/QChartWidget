# QChartWidget 项目总纲（现状 · 设计框架 · 路线图 · 协作约定）

> **本文档的用途**：主对话统筹各阶段工作的权威备忘录；每个阶段交给 AgentTeams 执行时，把本文 + `design_notes.md` 作为种子上下文喂给成员。
> **维护约定**：每完成一个阶段，回写本文对应小节（勾选 + 验收结果），保证文档永远反映事实。
> 设计细节以 [`design_notes.md`](design_notes.md) 为准（本文是「现状 + 差距 + 路线图 + 决策」，design_notes 是「五空间模型如何工作」）。

---

## 1. 项目现状

### 1.1 定位与状态

- **定位**：基于 QPainter 的通用坐标系图表库（容器 + 图层 + 系列 + 独立轴 + 投影）。
- **当前状态**：2D 可用（7 个 demo + 22 个单测全绿，三套工具链验证过）；**3D 完全不支持**。
- **目标**：Manim 级 3D 动画能力，但要求**实时/可交互**（Manim 是离线渲染，这里要 60fps 交互）。

### 1.2 现有类清单（按职责）

| 职责 | 类 |
|---|---|
| 容器 | `QChartWidget`（Phase 0 后：场景组装 + 交互 pan/zoom/hover + 布局；viewRect/缓存已外移） |
| 相机 | `QChartCamera`（Phase 0 新增：viewRect + fit 策略 + View↔Pixel 映射 + `Q_PROPERTY(center/zoom)`） |
| 渲染器 | `QChartRenderer`（接口 + `QChartScene` 快照，Phase 1 改名）/ `QPainterChartRenderer`（双缓存 + 绘制编排 + `renderUncached`，参数化于 `QPaintDevice`） |
| 主题 | `QChartTheme`（Phase 1 新增：`Preset{Light,Dark}` + 调色板 + override 双槽颜色模型） |
| 图例 | `QChartLegend`（Phase 1 新增：plotArea 四角 overlay，点击切换系列 visible） |
| 导出 | `QChartWidget::saveAsPng/Svg/Pdf` + `QChartExportScope`（Phase 1 新增：默认全 widget，PNG 真栅格 / SVG 真矢量） |
| 轴 | `QChartAxis` + `QValueAxis` / `QLogAxis` / `QDateTimeAxis` / `QBarCategoryAxis` |
| 图层 | `QChartLayer`（组合 X/Y 轴 + 持有 Series + 网格 + hitTest） |
| 系列 | `QChartSeries` + `QXYSeries` / `QLineSeries` / `QScatterSeries` / `QPolygonSeries` / `QBarSeries` / `QRegionSeries` |
| 投影 | `QChartProjection` + `QCartesianProjection` / `QPolarProjection` / `QFunctionalProjection` / `QInterpolatedProjection`；`QChartProjectionFactory`、`ProjectionToolKit.h` |
| 动画 | `QChartAnimation`（基类）+ `QNumericSeriesAnimation`（数值 morph）/ `QBarAnimation`（柱 morph）/ `QViewRectAnimation`（2D 相机漫游：waypoint 贝塞尔 + sizeCurve + Generator）/ `QProjectionSwitchAnimation`（投影间插值） |
| 数据/工具 | `QDataPoint.h`、`QDataRect.h`、`QChartDebug.h`（日志分类） |

### 1.3 构建与验证状态

- **构建系统**：根 `CMakeLists.txt`（C++17，Qt6 ≥ 6.2，`find_package(... Core Gui Widgets Svg)`）。三个 target：
  - `QChartWidget` —— **静态库**（22 个 .cpp：Phase 0 后 + QChartTheme/QChartLegend）
  - `QChartDemo` —— `Test/test.cpp` + `Test/demos/*`（8 个演示）
  - `QChartTests` —— `TestUnit/`（12 个测试类）
- **moc 所有权约定（别碰）**：AUTOMOC 只在库目标开启（所有 Q_OBJECT 类的 moc 唯一编入静态库）；`QChartDemo` 关 AUTOMOC；`QChartTests` 关 AUTOMOC + 对 4 个测试头手动 `qt6_wrap_cpp`。**目的：避免多目标重复 moc 导致链接符号冲突。**
- **已验证（三端全绿）**：
  - MSVC（Qt 6.11.1 msvc2022_64 + VS2026 v145）编译 ✅，测试退出码 0 ✅
  - MinGW（Qt 6.11.0 mingw_64 + mingw1310_64）编译 ✅，测试退出码 0 ✅
  - Linux 原生（Qt 6.4.2 + cmake 3.28 + ninja + g++13.2，WSLg）编译 ✅，`ctest` 1/1 通过 ✅，7 个 demo 弹窗运行并干净退出 ✅
- **测试现状**：12 类 69 用例（轴刻度 4 类 22 + Phase 0 新增 22 + Phase 1 新增 25：主题/图例/导出/图例交互）。**仍零测试**：动画、tooltip/交互事件、布局 sizeHint。
- **demo 入口**：`Test/test.cpp` 支持 argv 选择：`QChartDemo.exe [polar bar pendulum sort camera swirl stress theme]`，无参 = 全部。**注意：git 由用户操作，改动提交由用户完成。**
- 演示清单：`demo_polar`（极坐标五边形）、`demo_bar`、`demo_pendulum`、`demo_sort`、`demo_camera`（2D 相机漫游）、`demo_swirl`（投影切换）、`demo_stress`（1M 点，验证视口裁剪：100 万点 → 只画约 1000 线）、`demo_theme`（Phase 1：深色主题 + 图例 + 三格式导出）。

### 1.4 环境事实（团队必读）

- **开发环境**：WSL Ubuntu；工作区 `/home/unidu/dsh/QChartWidget`（文件策略 workspace-write；**写 `/mnt/e` 需要 danger-full-access 授权**，会弹确认）。
- **Linux 侧**：Qt 6.4.2（`qt6-base-dev` 等，含 OpenGLWidgets）、cmake 3.28.3、ninja 1.11.1、g++ 13.2；`DISPLAY=:0`（WSLg，GUI 可直接弹到 Windows 桌面）。
- **Linux 构建/测试/运行**：
  ```bash
  cmake -S . -B build-linux && cmake --build build-linux
  ctest --test-dir build-linux --output-on-failure
  ./build-linux/QChartDemo          # 无参=全部 demo
  ```
- **Windows 侧**：git 由用户同步到 `E:\Dujia\DuRunHan\Programs\cplusplus\QChartWidget`（镜像与工作区一致，仅 CRLF 差异）。工具链：`E:\Qt\6.11.1\msvc2022_64`、`E:\Qt\6.11.0\mingw_64`、`E:\Qt\Tools\{CMake_64,Ninja,mingw1310_64}`、VS2026 v145。一键脚本 `scripts/build-msvc.bat`、`scripts/build-mingw.bat`。
- **WSL→Windows interop 两坑**：① `cmd.exe` 从 UNC 工作目录启动会被拒（须 `cd /d` 到盘符路径）；② interop 会转义双引号（复杂命令写成 `.bat` 再执行）。

### 1.5 已知小问题（含 Phase 0 处理结果）

1. ~~无主题系统~~ ✅ **已修（Phase 1）**：`QChartTheme`（Light/Dark 预设 + override 双槽 + 系列调色板循环取色 + followSystemPalette），深色模式色差已解决。
2. ~~日志分类名不一致~~ ✅ **已修（Phase 0）**：根因是 `chart.*.verbose=false` 为 Qt **非法规则**（`*` 通配符只能出现在模式末尾，Qt 忽略整条规则）+ `logRenderVerbose` 定义名错位。现 verbose 类默认 `QtWarningMsg` 静默，规则用 `*.verbose=false` 合法形式。
3. ~~重复实现~~ ✅ **已修（Phase 0）**：5 处线性映射（DrawContext 两处、QChartAxis.cpp 匿名命名空间、QChartLayer.cpp、Widget 两处）收敛为 `QChartCamera` 一份。
4. `QChartWidget.h` 直接 include `QChartLayer.h` / `QChartProjection.h`（耦合偏紧；前向声明化解耦为可选项，Phase 0 未做，留待需要时）。
5. 根目录残留空目录 `QChartWidget/`（git 不追踪空目录，无实质影响）。

---

## 2. 设计框架

### 2.1 五空间链路（现状核心）

```
Data ──[Axis::toNumeric]──► Numeric ──[Projection::toCartesian]──► View Cartesian
      ──[线性]──► ViewNorm ──[线性]──► Pixel
```

各空间职责、Data≠Numeric 语义、NaN/Inf 策略、边框轴 vs 数据主脊、Grid 画法等见 `design_notes.md`。

### 2.2 现状的关键约定

- **Axis**：只做 Data↔Numeric + 刻度生成 + 标签格式化 + 绘制（`drawAtEdge`/`drawAtPosition`）；**不做坐标映射**。
- **Projection**：Numeric↔View Cartesian 双向映射 + `dataBounds↔viewRect` 包络 + `createPath`（曲线采样、NaN 自动断路径）。
- **Widget**：唯一 Projection 持有者、所有 Axis 持有者；`viewRect` 是主状态（相机）；`cartesianToPixel` 在此完成。
- **Layer**：组合 axisX/axisY，持有 Series，组装 `toPixel` 闭包注入 Series —— **Series 零耦合于 Axis/Projection/Widget 类型**（这是全库最值得保留的设计）。
- **所有权**：Widget 拥有 Layer/Axis；Layer 拥有 Series。
- **动画**：驱动数据模型/状态（不直接操作 QPainter），改完置脏缓存重绘。

### 2.3 已确认的演进决策（决策记录）

| # | 决策 | 内容 |
|---|---|---|
| D1 | **相机独立成类** | Phase 0 抽 `QChartCamera`：拥有 viewRect + fit 策略 + `toPixel/fromPixel`（收编 D2 提到的重复实现）；暴露 `Q_PROPERTY(center/zoom)` 让平移缩放类动画可用 QPropertyAnimation；3D 时扩展为 position/lookAt/up/FOV → `viewProjectionMatrix()`，**2D 是 3D 的退化特例**。 |
| D2 | **渲染器独立成类，分两层** | 第一层（Phase 0）：`QChartRenderer` + `QPainterChartRenderer`，收编双缓存 + drawBackground/drawForeground 编排，接口**参数化于 `QPaintDevice`**（`render(scene, device)`）→ 导出 PNG/SVG/PDF 直接白捡。第二层（Phase 3）：真正的后端抽象（Series 吐绘制命令/场景图，GL 消费），现在不做，避免过度设计。 |
| D3 | **动画优先 QPropertyAnimation** | 颜色/透明度等标量属性动画直接用 QPropertyAnimation（现状已支持：Series 的 Q_PROPERTY + NOTIFY → invalidateForeground）。只有「单属性表达不了」的才写自定义动画：逐点 morph、投影间插值、相机路径。 |
| D4 | **静态库** | 默认 STATIC；共享库需给公共类加导出宏（暂缓）。 |
| D5 | **git 全部由用户操作** | 主对话/团队不 commit、不 push、不 pull；只改文件。 |
| D6 | **每阶段验收标准** | 最低验收 = Linux 编译绿 + `ctest` 全绿（旧 22 例 + 新增）；7 个 demo 行为不回归。Windows 双工具链编译由用户侧抽验（或在需要时再走 interop）。 |
| D7 | **测试应用类型**（Phase 0） | `TestUnit` 用 `QGuiApplication`；ctest 在非 Windows 设 `QT_QPA_PLATFORM=offscreen` 无头跑，Windows 走默认平台（避免 offscreen 插件依赖）。 |
| D8 | **Qt 日志规则限制**（Phase 0） | 规则的 `*` 通配符只能出现在模式末尾，否则整条规则被 Qt 忽略（告警 "Ignoring malformed logging rule"）；verbose 类分类统一默认 `QtWarningMsg` 静默。 |
| D9 | **逐任务审查**（Phase 1 起） | 每个实现任务完成后立即由 reviewer 独立审查（必须实际运行测试/demo 挑错），不攒到阶段末合并审。 |
| D10 | **设计先行**（Phase 1 起） | 新增独立 designer 角色：新功能先出设计（问卷→方案→设计文档→用户确认），定稿后才交给 engineer；设计期间不动代码。 |
| D11 | **QChart 命名规范**（Phase 1） | 所有新公共类型一律 QChart 前缀：QChartTheme / QChartLegend / QChartScene（Phase 0 的 ChartScene 纯改名）/ QChartExportScope；裸枚举并入类内（QChartTheme::Preset）。 |
| D12 | **颜色 override 双槽模型**（Phase 1） | axis/layer/series/legend/背景的颜色 =「显式 override ?? 主题默认」，主题只当默认值、显式设色优先，`clear*()` 回退主题默认。 |
| D13 | **导出走 renderUncached**（Phase 1） | PNG/SVG/PDF 统一无缓存直绘（真矢量、不污染屏显缓存）；PDF 始终填背景（忽略透明开关）；导出跳过调试黄框（`QChartScene.exportMode`）。 |

### 2.4 当前架构（Phase 0 已落地）

```
Projection（Data/Numeric → View Cartesian / World）
     ↓
Camera（World → Screen；2D=viewRect，3D=视锥+矩阵）
     ↓
Renderer（Screen → 像素/缓存；QPainterRenderer | 未来 QOpenGLChartRenderer）

旁挂组件：Axis（数值化+刻度）/ Layer（轴+系列组合）/ Series（存数据，接收 toPixel）
```

`QChartWidget` 收缩为「控制器」：场景组装 + 交互（pan/zoom/orbit/hover）→ 改状态 → 通知 Renderer。

---

## 3. 差距分析（对标 Manim 3D + 实时）

### 3.1 2D 基本盘欠账

图例（仅接口注释，未实现）· 主题/调色板（硬编码）· 导出 PNG/SVG/PDF（无）· 实时数据流（双缓冲/QueuedConnection 未实现）· 框选缩放（预留未做）· hover 仅 tooltip 文本无高亮点 · 测试覆盖仅轴刻度。

### 3.2 3D 硬缺口（最大跨越）

1. **数学维度**：全链路 `QPointF/QRectF/[0,1]²`。需 `QVector3D` + 3D 包围盒 + 视锥；ViewNorm 这套 2D 归一化要换成 **World → Camera → Clip → NDC → Screen**。
2. **相机**：现有 `QViewRectAnimation` 是 2D 相机（center+zoom，做得完整）。缺 3D：position/lookAt/up/FOV/近远平面/透视与正交 + orbit/pan/dolly。
3. **渲染后端**：QPainter 是 CPU 光栅化 2D immediate mode，无深度、无 GPU。实时 3D 需 `QOpenGLWidget`（z-buffer、透视、光照、深度排序）。
4. **3D 系列**：3D 散点、参数曲线、曲面 z=f(x,y)、mesh、矢量场、3D 柱。
5. **拾取**：现 hitTest 是「像素 in 多边形」；3D 需射线求交。
6. **3D 轴/轴平面**：三维 spine + 网格平面（类似 matplotlib 3D）。

### 3.3 动画缺口（Manim 的本质不是画 3D，而是可变换对象 + 场景时间轴 + 相机漫游）

1. **对象/变换层级**：无 Mobject 式 position/rotation/scale 层级；3D 旋转需四元数插值（欧拉角万向锁）。
2. **动画类型**：缺 Transform/Morph（形状间插值）、Write/Create（描边出现）、3D 刚体变换。
3. **时间轴编排**：现为独立 QTimer/QAbstractAnimation，无统一 playbook（可重叠、可 seek）。Manim 的 `Scene` 即此。
4. **公式/文本**（需降级）：Manim 靠 LaTeX；实时场景应做富文本+数学符号标注，必要时 MathJax 离线缓存字形。

### 3.4 性能缺口（实时）

GPU 批量（VBO）· 帧循环（requestUpdate + vsync）· 缓存策略升级（静态双 QPixmap 对「每帧相机都动」的 3D 不适用，需脏段更新）。实测：stress demo 1M 点在 2D 下靠 culling 每次重绘约 0.7s。

### 3.5 保留 vs 重建

| 保留（扩展） | 重建 |
|---|---|
| Axis 的 Data↔Numeric + tick 生成（扩成 3 轴） | ViewNorm/ViewCartesian 2D 视窗状态 → World/Camera/Screen |
| Projection「通用坐标系」思想（升级 3D 投影+相机） | QPainter 渲染路径 → GL 后端 |
| Series「只存数据+注入变换」解耦思路 | 2D layout/plotArea → 3D 场景 + 2D overlay 分离 |
| 动画基类 + easing | 动画目标从「数据」扩到「对象变换+相机」 |

---

## 4. 路线图（阶段划分，按依赖链排序）

### Phase 0 —— 抽缝 + 锁行为（纯重构，零新功能）✅ **已完成**

1. ✅ 抽 `QChartCamera`（viewRect + fit 策略 + toPixel/fromPixel；消灭 5 处重复映射；Q_PROPERTY center/zoom）
2. ✅ 抽 `QChartRenderer` + `QPainterChartRenderer`（缓存 + 绘制编排；`render(scene, QPaintDevice*)`，QImage 渲染验证通过）
3. ✅ 补单测锁行为：相机 9 例 / 投影 6 例 / 渲染器 4 例 / 命中 3 例（共 22 例新增）
4. ✅ 日志分类清理（根因：非法通配符规则 + 定义名错位；verbose 默认静默）
5. ⏭ 未做（可选项）：`QChartWidget.h` 前向声明化解耦

**验收结果（reviewer 独立验收 + captain 复核）**：行为完全不变；`--clean-first` 编译 0 error/0 warning；ctest 8 类 **44 用例全绿**（旧 22 + 新 22）；7 demo 冒烟无回归、无刷屏。
**遗留小观察**：`QChartProjection.h:13` 有一行死注释可顺手清理。

### Phase 1 —— 2D 补全（主题 / 图例 / 导出）✅ **已完成**

- ✅ 主题/调色板：`QChartTheme`（Light/Dark 预设）+ override 双槽（显式设色优先）+ 系列调色板循环取色 + `setFollowSystemPalette`（可选跟随系统）——深色模式色差解决。
- ✅ 图例：独立 `QChartLegend`（plotArea 四角 overlay、点击切换系列 visible、文字色跟随主题）。
- ✅ 导出：`saveAsPng/Svg/Pdf` + `QChartExportScope`（默认全 widget，可选仅 plotArea）；PNG 栅格 / SVG 真矢量 / PDF；透明开关；导出跳过调试黄框。
- ✅ 深色演示 `demo_theme`（Dark 主题 + 图例 + 一键导出三格式）；旧 7 demo 去硬编码白轴。

**验收结果（reviewer 逐任务审查 + t29 终验 + captain 复核）**：干净构建 0 error/0 warning；ctest **12 类 69 用例全绿**（旧 44 + 新 25）；8 demo 冒烟无回归；theme 三格式产物复核通过（PNG 640×480 暗底 / SVG 15 个 `<path>` 无栅格 / PDF `%PDF-1.4`）。
**设计文档**：`design_theme.md` / `design_legend.md` / `design_export.md`（拆分 + QChart 命名规范 + QChartScene 说明）。
**遗留小观察**：`Test/test.cpp` 提示文案漏写 "theme"（demos[] 已含）；`QChartProjection.h:13` 死注释（Phase 0 遗留）。

### Phase 2 —— 3D 数学先行（不碰 GPU）

`QVector3D`/`QMatrix4x4`、相机升 3D、3D 系列（散点/曲线/曲面线框）；**QPainter 线框 + painter's algorithm 验证透视与相机数学**。
**验收**：旋转透视正确、无万向锁、参数曲面 demo 可交互旋转。

### Phase 3 —— GPU 实时

`QOpenGLChartRenderer`、VBO 批量、光照、深度；orbit/zoom/pan 实时交互。
**验收**：10 万点 60fps；1M 点可交互。

### Phase 4 —— Manim 级动画

对象变换层级（四元数插值）、相机路径动画 3D 化、场景时间轴（编排/seek）、数学标注。
**验收**：类似 3Blue1Brown 的镜头运镜 + Transform 动画 demo。

---

## 5. 协作约定（AgentTeams 用法）

- **主对话 = 统筹**：只做决策、验收、文档维护、任务派发，并作为**用户与团队之间的唯一中转**（团队成员的问卷/问题经 captain 转达用户，用户决定再转回）；具体设计/编码/审校交给团队。
- **角色分工（Phase 1 起）**：
  - `designer`（设计者）：与用户沟通设计——发问卷、提方案、出设计文档；**设计定稿前不动代码**；沟通经 captain 中转。
  - `engineer`（实现者）：按设计文档实现；每完成一个 task 立即交付，不等批量。
  - `reviewer`（校验员）：**每个 engineer task 完成后立即独立审查**（不做阶段末合并审查）；审查必须**实际运行**测试与 demo 找问题，尽可能挑错、宁可错杀不放过；不只通读代码。
- **每个阶段 = 一串小任务**：设计任务（designer）→ 用户确认设计 → 实现任务×N（engineer），**每个实现任务后紧跟一个审查任务（reviewer）**，审查通过才进下一个实现任务。任务描述必须引用本文档对应章节 + `design_notes.md` + 验收标准（D6）。
- **阶段结束回写本文**：勾选完成项、更新 1.3 验证状态、记录新增决策（D7、D8…）。
- **红线**：不碰 moc 所有权约定（1.3）；不改 CMake target 结构除非阶段目标要求；git 操作留给用户（D5）；任何设计分歧/阻塞必须上报 captain 转用户商讨，不得自作主张。
- 工作区路径：`/home/unidu/dsh/QChartWidget`；构建命令见 1.4。
