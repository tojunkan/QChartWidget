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
| 容器 | `QChartWidget`（场景状态 + viewRect + 双缓存 + pan/zoom/hover + 布局） |
| 轴 | `QChartAxis` + `QValueAxis` / `QLogAxis` / `QDateTimeAxis` / `QBarCategoryAxis` |
| 图层 | `QChartLayer`（组合 X/Y 轴 + 持有 Series + 网格 + hitTest） |
| 系列 | `QChartSeries` + `QXYSeries` / `QLineSeries` / `QScatterSeries` / `QPolygonSeries` / `QBarSeries` / `QRegionSeries` |
| 投影 | `QChartProjection` + `QCartesianProjection` / `QPolarProjection` / `QFunctionalProjection` / `QInterpolatedProjection`；`QChartProjectionFactory`、`ProjectionToolKit.h` |
| 动画 | `QChartAnimation`（基类）+ `QNumericSeriesAnimation`（数值 morph）/ `QBarAnimation`（柱 morph）/ `QViewRectAnimation`（2D 相机漫游：waypoint 贝塞尔 + sizeCurve + Generator）/ `QProjectionSwitchAnimation`（投影间插值） |
| 数据/工具 | `QDataPoint.h`、`QDataRect.h`、`QChartDebug.h`（日志分类） |

### 1.3 构建与验证状态

- **构建系统**：根 `CMakeLists.txt`（C++17，Qt6 ≥ 6.2）。三个 target：
  - `QChartWidget` —— **静态库**（20 个 .cpp）
  - `QChartDemo` —— `Test/test.cpp` + `Test/demos/*`（7 个演示）
  - `QChartTests` —— `TestUnit/`（4 个测试类）
- **moc 所有权约定（别碰）**：AUTOMOC 只在库目标开启（所有 Q_OBJECT 类的 moc 唯一编入静态库）；`QChartDemo` 关 AUTOMOC；`QChartTests` 关 AUTOMOC + 对 4 个测试头手动 `qt6_wrap_cpp`。**目的：避免多目标重复 moc 导致链接符号冲突。**
- **已验证（三端全绿）**：
  - MSVC（Qt 6.11.1 msvc2022_64 + VS2026 v145）编译 ✅，测试退出码 0 ✅
  - MinGW（Qt 6.11.0 mingw_64 + mingw1310_64）编译 ✅，测试退出码 0 ✅
  - Linux 原生（Qt 6.4.2 + cmake 3.28 + ninja + g++13.2，WSLg）编译 ✅，`ctest` 1/1 通过 ✅，7 个 demo 弹窗运行并干净退出 ✅
- **测试现状**：仅轴刻度有单测（`TestUnit/tests/`，4 类 22 用例）。Projection / Series / hitTest / 缓存失效 / 动画**零测试**。
- **demo 入口**：`Test/test.cpp` 支持 argv 选择：`QChartDemo.exe [polar bar pendulum sort camera swirl stress]`，无参 = 全部。**注意：此改动尚未提交 git（git 由用户操作）。**
- 演示清单：`demo_polar`（极坐标五边形）、`demo_bar`、`demo_pendulum`、`demo_sort`、`demo_camera`（2D 相机漫游）、`demo_swirl`（投影切换）、`demo_stress`（1M 点，验证视口裁剪：100 万点 → 只画约 1000 线）。

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

### 1.5 已知小问题（Phase 0 一并处理）

1. **无主题系统**：颜色全硬编码（轴默认黑、网格 220 灰、demo 里写死白色）。用户在深色 Windows 下看到的色差即由此而来。
2. **日志分类名不一致**：`logRenderVerbose` 在 `QChartDebug.h` 声明（render 组），却在 `QChartAxis.cpp` 定义为 `"chart.projection.verbose"`；实测 `chart.*.verbose=false` 压不住 `createPath` 逐采样点刷屏。
3. **重复实现**：`QChartWidget::cartesianToPixel/pixelToCartesian` 与 `DrawContext::numericToPixel`（`QChartAxis.h` 内）是同一段线性映射写了两遍。
4. `QChartWidget.h` 直接 include `QChartLayer.h` / `QChartProjection.h`（耦合偏紧，可前向声明化）。
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

### 2.4 目标架构（Phase 0 完成后）

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

### Phase 0 —— 抽缝 + 锁行为（纯重构，零新功能；决定后面能否安全加速）

1. 抽 `QChartCamera`（viewRect + fit 策略 + toPixel/fromPixel；消灭 1.5-3 的重复实现；Q_PROPERTY center/zoom）
2. 抽 `QChartRenderer` + `QPainterChartRenderer`（缓存 + 绘制编排；`render(scene, QPaintDevice*)`）
3. 补单测锁行为：相机映射往返 / fit 策略 / 缓存失效 / hitTest / 投影包络
4. 日志分类清理（1.5-2，verbose 默认静默、命名归位）
5. （可选）`QChartWidget.h` 前向声明化解耦

**验收**：行为完全不变；Linux 编译绿 + 旧 22 例 + 新增全绿；7 demo 无回归。

### Phase 1 —— 2D 补全（见效快，顺带解决深色模式）

主题/调色板（深色/浅色一键）· 图例 legend · 导出 PNG/SVG/PDF（借 Phase 0 渲染器 device 参数化）。
**验收**：深色模式 demo、图例显示/交互、三格式导出与屏显一致。

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

- **主对话 = 统筹**：只做决策、验收、文档维护；具体编码交给团队。
- **每个阶段 = 一支队/一批任务**：任务描述必须引用本文档对应章节 + `design_notes.md` 相关小节 + 验收标准（D6）。
- **阶段结束回写本文**：勾选完成项、更新 1.3 验证状态、记录新增决策（D7、D8…）。
- **红线**：不碰 moc 所有权约定（1.3）；不改 CMake target 结构除非阶段目标要求；git 操作留给用户（D5）。
- 工作区路径：`/home/unidu/dsh/QChartWidget`；构建命令见 1.4。
