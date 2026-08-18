# QChartWidget 项目总纲（现状 · 设计框架 · 路线图 · 协作约定）

> **本文档的用途**：主对话统筹各阶段工作的权威备忘录；每个阶段交给 AgentTeams 执行时，把本文 + `design_notes.md` 作为种子上下文喂给成员。
> **维护约定**：每完成一个阶段，回写本文对应小节（勾选 + 验收结果），保证文档永远反映事实。
> 设计细节以 [`design_notes.md`](design_notes.md) 为准（本文是「现状 + 差距 + 路线图 + 决策」，design_notes 是「五空间模型如何工作」）。

---

## 1. 项目现状

### 1.1 定位与状态

- **定位**：基于 QPainter 的通用坐标系图表库（容器 + 图层 + 系列 + 独立轴 + 投影）。
- **当前状态**：2D 可用 + **3D 数学先行 + 轴/网格控制流落地**（11 个 demo + 17 类 158 用例全绿，三套工具链验证过）；3D 线框渲染与参照系（盒/spine/刻度/标签）就绪，**GPU 实时渲染（Phase 3）未做**。
- **目标**：Manim 级 3D 动画能力，但要求**实时/可交互**（Manim 是离线渲染，这里要 60fps 交互）。

### 1.2 现有类清单（按职责）

| 职责 | 类 |
|---|---|
| 容器 | `QChartWidget`（Phase 0 后：场景组装 + 交互 pan/zoom/hover + 布局；viewRect/缓存已外移）+ `QChartWidget3D`（Phase 2：3D 场景组装 + orbit/dolly 交互（平移仅 API）+ 联动信号 uvHovered/uvSelected + **控制器**（viewCube 5³ 反算 dataBounds/域盒链/轴盒推送），基类仅两处虚化钩子） |
| 相机 | `QChartCamera`（Phase 2 起为基类：QObject + viewChanged）+ `QChartCamera2D`（原 2D 相机改名：viewRect + fit 策略 + View↔Pixel 映射 + `Q_PROPERTY(center/zoom)`）+ `QChartCamera3D`（Phase 2 补项 R5 重构：**viewCube 主状态** + orientation(yaw/pitch) + fovY，position/lookAt/up/near/far 派生只读，`Q_PROPERTY(viewCubeCenter/Size/yaw/pitch/fovY)`，orbit/dolly/panViewCube/setViewCubeToFit，正交模式 viewCube 即投影盒） |
| 渲染器 | `QChartRenderer`（接口 + `QChartScene` 快照，含 Phase 2 3D 段：camera3D/layers3D/worldBounds/is3D）/ `QPainterChartRenderer`（双缓存 + 绘制编排 + `renderUncached` + 3D 子路径：图元分层 {Grid,Series,ForegroundDecor} + 深度降序 + 网格深度偏置 kGridDepthBias + billboard 标签） |
| 主题 | `QChartTheme`（Phase 1 新增：`Preset{Light,Dark}` + 调色板 + override 双槽颜色模型） |
| 图例 | `QChartLegend`（Phase 1 新增：plotArea 四角 overlay，点击切换系列 visible） |
| 导出 | `QChartWidget::saveAsPng/Svg/Pdf` + `QChartExportScope`（Phase 1 新增：默认全 widget，PNG 真栅格 / SVG 真矢量） |
| 轴 | `QChartAxis` + `QValueAxis` / `QLogAxis` / `QDateTimeAxis` / `QBarCategoryAxis`（3D 侧 axisX/Y/Z 只做 toNumeric；3D 参照系由 `QChartAxes3D` 编排器复用其 tickValues/tickLabels） |
| 图层 | `QChartLayer`（组合 X/Y 轴 + 持有 Series + 网格 + hitTest）+ `QChartLayer3D`（Phase 2：三轴 + 3D 系列 + ProjectFn3D 全链闭包 + 图元收集 + **轴/网格编排**：QChartAxes3D×3 + axesDataBox + GridMode{Box,Lattice} + 盒 12 边/3 spine/tick 点/标签 + 快速通道） |
| 系列 | 2D：`QChartSeries` + `QXYSeries` / `QLineSeries` / `QScatterSeries` / `QPolygonSeries` / `QBarSeries` / `QRegionSeries`；3D（Phase 2）：`QChartSeries3D` + `QChartScatterSeries3D` / `QChartLineSeries3D` / `QChartSurfaceSeries`（Data 层 QVariant 三元组、World 层 QVector3D 缓存，全链闭包注入零耦合） |
| 投影 | 2D：`QChartProjection` + `QCartesianProjection` / `QPolarProjection` / `QFunctionalProjection` / `QInterpolatedProjection`；3D（Phase 2，header-only）：`QChartProjection3D` + `QChartCartesianProjection3D` / `QChartCylindricalProjection3D` / `QChartSphericalProjection3D` / `QChartFunctionalProjection3D`（2→3 参数曲面嵌入 + 3→3 坐标变换 + **快速通道** `isIdentityMapping()`/`samplingSegmentsHint`）；`QChartProjectionFactory`、`ProjectionToolKit.h` |
| 3D 参照系 | `QChartAxes3D`（Phase 2 补项：非 Q_OBJECT 编排器，组合复用 QChartAxis 刻度/标签，产 Numeric 空间几何——盒 12 边/3 spine/tick 锚点/标签配置；三层分离红线） |
| 动画 | `QChartAnimation`（基类）+ `QNumericSeriesAnimation`（数值 morph）/ `QBarAnimation`（柱 morph）/ `QViewRectAnimation`（2D 相机漫游：waypoint 贝塞尔 + sizeCurve + Generator）/ `QProjectionSwitchAnimation`（投影间插值）；3D 相机动画走 QPropertyAnimation（viewCubeCenter/Size/yaw/pitch/fovY） |
| 3D 数学/工具 | `QChartMath.h`（Phase 2：Clip→NDC→Screen 纯函数 + 透视/正交矩阵 + viewDepth + projectBatch 批量投影入口）、`QDataPoint.h`、`QDataPoint3D.h`、`QDataRect.h`、`QChartWorldBox`/`QChartProjectedPoint`/`QChartPrimitive`（图元列表，Phase 3 命令缓冲雏形）、`QChartDebug.h`（日志分类） |

### 1.3 构建与验证状态

- **构建系统**：根 `CMakeLists.txt`（C++17，Qt6 ≥ 6.2，`find_package(... Core Gui Widgets Svg)`）。四个 target：
  - `QChartWidget` —— **静态库**（29 个 .cpp：Phase 0/1 后 + Phase 2 新增 QChartCamera3D/QChartSeries3D/QChartScatterSeries3D/QChartLineSeries3D/QChartSurfaceSeries/QChartLayer3D/QChartWidget3D）
  - `QChartDemo` —— `Test/test.cpp` + `Test/demos/*`（11 个演示）
  - `QChartTests` —— `TestUnit/`（16 个测试类）
  - `QChartBench` —— `Test/bench/bench_main.cpp`（`BUILD_BENCH` option）
- **moc 所有权约定（别碰）**：AUTOMOC 只在库目标开启（所有 Q_OBJECT 类的 moc 唯一编入静态库）；`QChartDemo` 关 AUTOMOC；`QChartTests` 关 AUTOMOC + 对测试头手动 `qt6_wrap_cpp`。**目的：避免多目标重复 moc 导致链接符号冲突。**
- **已验证（三端全绿）**：
  - MSVC（Qt 6.11.1 msvc2022_64 + VS2026 v145）编译 ✅，测试退出码 0 ✅
  - MinGW（Qt 6.11.0 mingw_64 + mingw1310_64）编译 ✅，测试退出码 0 ✅
  - Linux 原生（Qt 6.4.2 + cmake 3.28 + ninja + g++13.2，WSLg）编译 ✅，`ctest` 1/1 通过 ✅，7 个 demo 弹窗运行并干净退出 ✅
- **测试现状**：17 类 158 用例全绿（旧 69 + Phase 2 新增 33 + **补项新增 22**：axes3d 8 / 分层像素 5 / 反算三路径 3 / 计数与快速通道 6；另 34 init/cleanup）。**仍零测试**：动画、tooltip/交互事件、布局 sizeHint（3D 交互由 demo + 合成事件实证覆盖）。
- **demo 入口**：`Test/test.cpp` 支持 argv 选择：`QChartDemo.exe [polar bar pendulum sort camera swirl stress theme scatter3d line3d surface3d]`，无参 = 全部。**注意：git 由用户操作，改动提交由用户完成。**
- 演示清单：`demo_polar`（极坐标五边形）、`demo_bar`、`demo_pendulum`、`demo_sort`、`demo_camera`（2D 相机漫游）、`demo_swirl`（投影切换）、`demo_stress`（1M 点，验证视口裁剪：100 万点 → 只画约 1000 线）、`demo_theme`（Phase 1：深色主题 + 图例 + 三格式导出）、`demo_scatter3d`（Phase 2：球面均匀采样 3D 散点 + 球面/柱面投影切换）、`demo_line3d`（Phase 2：3D 螺旋线 + 相机飞行动画）、`demo_surface3d`（Phase 2：球面/莫比乌斯环切换 + 网格地板 + **双 Widget 联动**：左 3D ↔ 右 (u,v) 平面互显）。

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
6. **QValueAxis 在极坐标下不友好**：niceStep 生成的刻度无法整除（如 0~360° 想每 90° 一分度，niceStep 给不出 90，需 `setTickInterval(90)` 手动实现美观分度）。**优先级低**：留待封装便捷 Layer 时做「极坐标友好刻度」策略。（用户提出，必须记住）
7. **MinGW 下动画更新吃力、MSVC 流畅**：swirl 投影切换动画在 MinGW 构建下明显卡顿，MSVC 构建流畅（同为 Debug）。QPainter 动画在 MinGW/GCC 与 MSVC 存在优化差距（或调试版开销差异）。**记入 Phase 3 性能评估输入**：性能指标必须双工具链实测，不能只测一边。（用户提出，必须记住）
8. ~~`config.local.bat.example` 行内 REM 缺陷~~ ✅ **已修（Phase 2 终验）**：`set BUILD_DIR=... REM 可选` 行中 REM 会并入变量值，已改 REM 独占行（reviewer D14 自修）。
9. **Phase 2 遗留观察**：`demo_line3d` 头注释「半径 1.5」指相机伴飞路径（绘制线 r=1.0），措辞含糊（非缺陷）；3D 悬停每 move 重收集图元 + 曲面 worldCache 每帧重填 → **Phase 3 优化点**（t12 基线：64×64 曲面 + 2000 散点 ≈ 44.5ms/帧，collect 占 47% 主成本）。
10. **demo 无参运行时逐窗口依次加载，长耗时 demo（stress/3D）导致「未响应」**（用户反馈，低优先级）：建议主页面改为导航菜单（点击运行单个 demo），或单个 demo 独立进程/非阻塞加载——记入 backlog，不急。
11. ~~**3D 控制流缺失（用户反馈，严重）**~~ ✅ **已修（Phase 2 补项）**：3D 轴/网格/刻度/标签控制流完整落地——viewCube 主状态相机（R5）+ 5³ 反算与笛卡尔快速通道 + 盒 12 边/3 spine/tick 点标记/billboard 标签 + Box/Lattice 网格 + 参考线分层与深度偏置（详见 §4 Phase 2 补项小节）。

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
| D14 | **小问题 reviewer 自修**（用户拍板） | reviewer 审查发现的「轻微/小问题」（笔误、注释、小修）由 reviewer **直接修复并自验**，不回传 engineer；只有实质性问题才经 captain 派回 engineer。 |
| D15 | **3D 系列全链闭包 ProjectFn3D**（Phase 2） | `collectPrimitives/draw` 注入 `std::function<QChartProjectedPoint(const QDataPoint3D&)>`：Data→Numeric→World→Screen 全链在 Layer3D 组装闭包内完成，系列只存 Data、零耦合（2D toPixel 同构延续）；QChartPrimitive 带 dataIndex（hover 定位 (u,v)）。 |
| D16 | **painter's algorithm 深度降序**（Phase 2） | depth=−viewZ（**越大越远**）；图元按 depth **降序**排序绘制（远先画、近者后画覆盖），nearCoversFar 像素断言锁死；排序归 Renderer 3D 子路径（跨系列全局视野）。 |
| D17 | **Widget 形态：QChartWidget3D 子类**（Phase 2） | 3D 场景/交互/联动隔离在子类；基类仅两处最小改动（场景组装抽 buildScreenScene 虚化 + buildExportScene 访问级虚化），2D 类可证明零回归。 |
| D18 | **双 Widget 联动 = 信号互发 (u,v)**（Phase 2） | Series 单归属是硬约束（QObject 单父 + Layer qDeleteAll），不共享 Series 对象；两 Widget 同构数据副本 + 相同 Axis range → 同一 Numeric 空间，uvHovered/uvSelected/uvHoveredEnd 单向传值，高亮标记只收不发防回环。 |
| D19 | **Phase 2 3D 渲染边界：数值型 Data**（Phase 2） | 渲染正确性保证限于数值型 Data（闭包内 QVariant→qreal 恒等）；非数值 Axis 渲染转换（QDateTime/QBarCategory）记入 Phase 3，与「数值预转换缓存」性能项合并实现。 |
| D20 | **QChartCamera3D::orthographicBox**（Phase 2） | 正交模式投影盒显式访问器（非 Q_PROPERTY），「正交盒=viewRect」硬验收（3D 正交俯视 ≡ 2D cartesianToPixel）工程必需；默认 (0,0,10,10)。**R5 起废弃删除**（viewCube 即投影盒）。 |
| D21 | **viewCube 主状态相机（R5，Phase 2 补项）** | 3D 取景框 = World 空间盒（2D viewRect 的 3D 对标物，与相机无关）+ orientation + fovY；position/lookAt/up/near/far 派生只读；Q_PROPERTY=viewCubeCenter/Size+yaw/pitch+fovY；dolly=缩放盒（2D zoom 同构）、orbit=转朝向（viewCube 不动）；D-3D-2 硬验收正交俯视 ≡ 2D 直接成立。 |
| D22 | **平移无鼠标手势（R6，用户拍板）** | 三方向手势语义未定——viewCube 平移只经 API（panViewCube/setViewCubeCenter，代码/动画驱动）；拖拽=转相机角度。 |
| D23 | **5³ 反算 + 笛卡尔快速通道（R6，用户拍板）** | viewCube→dataBounds 用 5×5×5=125 点网格采样（通用坐标系极值不在角上）；`isIdentityMapping()`（Cartesian3D=true）→ 反算免采样、图元免 toWorld、段数=2。 |
| D24 | **3D 参照系分层与编排（Phase 2 补项）** | 网格与系列统一深度排序 + kGridDepthBias=1e-3（同深度系列赢）；spine/刻度点/标签前景层恒后画；`QChartAxes3D` 非 Q_OBJECT 编排器（组合复用 QChartAxis，三层分离：只产 Numeric 几何）；Box/Lattice 网格模式。 |

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

> **Phase 2 已落地**：1（数学链 World→Camera→Clip→NDC→Screen + QChartMath）✅、2（3D 相机 viewCube 主状态 + orbit/dolly，防万向锁）✅、4（3D 散点/参数曲线/曲面线框）✅、5（屏幕近邻命中；射线求交仍缺）◐、6（**3D 轴/网格/刻度/标签控制流，Phase 2 补项**）✅；**仍缺口**：3（GL 渲染后端）、5（射线拾取增强）。

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

### Phase 2 —— 3D 数学先行（不碰 GPU）✅ **已完成**

**前置子项（✅ 已完成）性能基准基线**：`QChartBench`（Test/bench/bench_main.cpp，`BUILD_BENCH` option）——QElapsedTimer + 预热 + 20 迭代 → min/median/avg/max，CSV 落 build 目录（bench_results.csv），env/toolchain 运行时自动识别（`kernel|platform|compiler`，reviewer 复核落地）。配套小 API：`QLineSeries::setCullingEnabled(bool)`（默认 true=旧行为）。

**基准结论（Linux GCC / offscreen 软渲染，reviewer 独立复核确认，Phase 3 优化输入）**：
- line 1M 局部缩放（≈1000 可见）：culling on ≈493-523ms vs off ≈507-552ms → **收益仅 2.7%~5.3%**；全览下 culling 反而慢 ~8%（检查开销>收益）。
- **瓶颈在 toPixel 投影而非绘制**：draw 对全部 N 点先 toPixel 再判裁剪（O(N) 无条件），culling 只省廉价 lineTo → **Phase 3/优化第一目标：裁剪前移（先粗筛数据点再做投影）或 GPU 批量投影**。
- scatter 100k zoom 83.7ms vs full 417ms：收益主要来自 **QPainter clip**（setClipRect(plotArea)），不是 QChart 裁剪——bench 已注明，勿误读。
- export PNG 1M ≈663-745ms、SVG 100k ≈141ms、PDF 100k ≈237ms；缓存重建 100k ≈73.8ms。
- 工具链差距：MinGW 动画吃力 vs MSVC 流畅（§1.5-7）。**Windows MSVC 基线已回填（用户实测，Debug 构建）**：
  - 10k 点：line_full on median **54.1ms** / off 49.6；line_zoom on **46.1** / off 95.9
  - 100k 点：line_full on median **877.1ms** / off 815.5；line_zoom on **616.4** / off 752.0
  - 结论与 Linux 一致（全览下 culling 收益为负、缩放场景有收益）；MSVC Debug 明显慢于 Linux 侧量级（100k ≈ 870ms vs Linux 1M ≈ 500ms，构建类型可能不同）→ Phase 3 需同构建类型（Release）双端对比。

**Phase 2 完成项**（7 实现任务 + 7 逐任务审查 + 终验，全程零回退）：
1. ✅ 数学层 `QChartMath.h`（header-only）：Clip→NDC→Screen 纯函数（w≤0→NaN 哨兵、y 翻转与 2D 一致）+ 透视/正交矩阵 + `viewDepth` + `projectBatch` 批量投影入口（Phase 3 GPU 预留）。
2. ✅ 相机三件套：`QChartCamera`（基类）+ `QChartCamera2D`（原 2D 相机改名搬入，行为零变化，改名映射表 15 处引用点）+ `QChartCamera3D`（position/lookAt/up/FOV/near/far、透视/正交、orbit pitch clamp ±89° 防万向锁、dolly/panTarget/fitToBounds、`Q_PROPERTY(position/lookAt/fovY)` + QVector3D 插值器）。**硬验收**：3D 正交俯视 ≡ 2D cartesianToPixel（49 采样点单测锁死）。
3. ✅ `QChartProjection3D` 家族（header-only）：Cartesian3D（恒等）/ Cylindrical3D（(r,θ,z)，r=0→θ NaN）/ Spherical3D（(r,θ,φ)）/ Functional3D（2→3 参数曲面嵌入 + 3→3 坐标变换，球面/莫比乌斯环 lambda 落地）。
4. ✅ 3D 系列：`QDataPoint3D`（QVariant 三元组）+ `QChartSeries3D` + 散点/线/曲面三子类；Data 层存任意类型、World 层 QVector3D 渲染时产生；**全链闭包 ProjectFn3D 注入**（D15），系列零耦合红线延续。
5. ✅ 渲染 3D 路径：QChartScene 3D 段 + `QChartLayer3D`（三轴 toNumeric + 网格地板 + worldCache 直算）+ `QChartPrimitive` 图元列表（depth + dataIndex）+ **painter's algorithm 深度降序**（D16，nearCoversFar 像素断言真实通过）；2D 路径逐字节未动。
6. ✅ `QChartWidget3D` 子类（D17）+ 交互（左键 orbit / 滚轮 dolly / 右键 pan）+ 3D 悬停简化版（屏幕近邻 <8px → dataIndex → (u,v)）+ **双 Widget 联动**（D18：uvHovered/uvSelected 信号互发，demo 实证双向往返）。
7. ✅ 3 demo 按「散点→线→曲面、静态→动态」节奏：`demo_scatter3d` / `demo_line3d`（相机飞行动画）/ `demo_surface3d`（球面/莫比乌斯切换 + 双 Widget 联动）。

**验收结果（7 次独立审查 + t17 终验 + captain 复核）**：`--clean-first` 干净全量 0 error/0 warning（四 target）；ctest **16 类 134 用例全绿**（旧 69 零回归 + 新 33）；**11 demo 冒烟全过**（8 旧 + 3 新，无参=全部）；旋转透视正确（orbit 像素差异实证）、**无万向锁**（pitch 恰 clamp 89.0000°、40× 混合 orbit 不越界）、参数曲面可交互旋转、双 Widget 联动互显（合成事件实证）；theme 三格式导出回归 PASS；脚本结构核查通过（`config.local.bat.example` 行内 REM 缺陷 D14 自修）。**性能基线**：3D 单帧（64×64 曲面 + 2000 散点 ≈ 1 万图元）≈ **44.5ms**（collect 47% 主成本：worldCache 每帧重填 + 闭包逐点投影；Phase 3 第一优化目标）；QChartBench 复跑与旧基线一致（终验期间环境负载致 ~2× 波动，非回归）。
**设计文档**：`design_3d.md`（14 节 + 实现期修订记录 R1~R4：全链闭包定案、深度降序修正、渲染边界、orthographicBox）。
**遗留**：3D 轴刻度/射线拾取/3D tooltip/光照 → Phase 3+；数值预转换缓存 + 非数值 Axis 渲染转换 → Phase 3（D19）；3D 场景导出（buildExportScene 已虚化，天然支持，未验收）。

### Phase 2 补项 —— 3D 轴/网格控制流（用户反馈升级）✅ **已完成**

**背景**：用户实测后判定「只做了数据计算流，没有控制流——三维轴/网格/刻度/标签完全没有」，升级为必补项。经三轮架构切磋定案（问卷作废存档于 `design_3d_axes_questionnaire.md`）。

**完成项**（7 实现任务 + 7 逐任务审查 + 终验，全程零回退）：
1. ✅ **viewCube 主状态相机（R5）**：`QChartCamera3D` 状态 = viewCube（World 空间盒，2D viewRect 的 3D 对标物，与相机无关）+ orientation(yaw/pitch) + fovY；position/lookAt/up/near/far 派生只读；Q_PROPERTY 迁移；**D-3D-2 硬验收新形态直接成立**（正交模式 viewCube 即投影盒 → 线性映射同构 ≡ cartesianToPixel）；删 orthographicBox。
2. ✅ **交互语义（R6）**：拖拽 = 转相机角度（orientation），viewCube 不动；dolly = 缩放 viewCube（2D zoom 同构）；**平移无鼠标手势**（三方向语义未定，仅 API panViewCube）；hover 只扫 Series 层。
3. ✅ **viewCube→dataBounds 反算**：5×5×5=125 点网格采样（通用坐标系极值不在角上，用户定案；柱坐标棱中点极值捕获测试实证 8 角会漏）+ **笛卡尔快速通道** `isIdentityMapping()`（反算 0 次 fromWorld、图元免 toWorld）。
4. ✅ **QChartAxes3D 编排器**（非 Q_OBJECT，三层分离红线：只产 Numeric 几何）：盒 12 边 + 3 强调 spine（min 角）+ tick 点标记（屏幕像素 4px）+ billboard 标签/轴标题，复用 QChartAxis tickValues/tickLabels。
5. ✅ **网格**：Box 模式（默认，盒底面 tick 对齐网格 ≈1k 图元）/ Lattice 晶格模式（三坐标面族，换投影即时空扭曲 ≈9.6k 图元）；gridFloor 旧 API 并入迁移。
6. ✅ **渲染分层**：QChartPrimitive::Layer{Grid,Series,ForegroundDecor}；网格与系列统一深度排序 + kGridDepthBias=1e-3（painter polygon offset，同深度系列赢；球前网格遮挡/球后被盖像素断言实证）；spine/刻度/标签前景层恒后画。
7. ✅ **Widget3D 控制器**：A3 域盒链（显式 setDomainBox > 数据包围盒 > defaultDataBounds）→ computeWorldBounds → setViewCubeToFit；视图变化钩子（反算+推轴盒+重绘，每帧不重算）；A9 兜底（无 fromWorld → 锚定域盒静态参照系）。
8. ✅ **demo 同步**：surface3d（球面/莫比乌斯 A9 静态路径 + 'A' 键 + 联动不回归）、line3d（Cylindrical 视图驱动路径 + viewCube 动画）、scatter3d 统一观感。

**验收结果（t22→t34 逐项独立审查 + t35 终验 + captain 复核）**：`--clean-first` 干净全量 0 error/0 warning（四 target）；ctest **17 类 158 用例全绿**（旧 69 零回归 + 补项新增 22）；11 demo 冒烟全过；验收项复核：轴/网格/标签可见（surface3d grid=224/decor=392/labels=11）、'A' 开关、晶格行数公式、深度偏置像素语义、viewCube 派生不变量、R6 无平移手势、联动双向不回归；**性能增量（归一化）**：盒模式 ≈+1~6ms、晶格 ≈+25~28ms（19200 次投影为主成本；Phase 3 输入：轴/网格图元缓存 + 裁剪前移）；QChartBench 与基线一致（环境负载非回归）。
**设计文档**：`design_3d_axes.md`（A1~A10 + v2/v3/v4 修订记录）；`design_3d.md` 增 R5/R6 修订记录。
**遗留**：正交模式 2D 边框轴（A4 后置）；3D 轴/网格动画（Phase 4）；tick 次刻度；轴/网格图元缓存（Phase 3）；精确拟合距离公式（注释备将来）。

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

---

## 6. 阶段复盘（经验教训，Phase 0/1 沉淀）

1. **增量构建优先，全量只在验收时**：项目变大后 `--clean-first` 全量重编每次数十秒，日常迭代一律增量 `cmake --build`；`--clean-first` 只留给 reviewer 审查与终验。**任务粒度与构建次数直接挂钩**——合并任务（图例/导出各合并为一个）就是为了减少构建轮次，后续排期继续把「构建次数」当成本。
2. **目录纪律（本次血泪）**：
   - 脚本一律以「脚本所在目录」为锚（`%~dp0`/`SCRIPT_DIR`），**不得依赖调用者的 cwd**；`cmake -S` 用 `-S "%ROOT_DIR%"` 锚定，绝不用相对 `-S ../`（从不同 cwd 调用会指向错误目录）。
   - 运行 demo/测试的产物（`demo_export.*`、`test_log.txt`）会落到 cwd——在源码根目录运行会污染源码树；已 gitignore，但**更该从 build 目录运行**或让程序写临时目录。
   - 批处理里 `REM` 注释必须**独占一行**（跟在命令后会被当作参数传给程序，本次修掉了这个 bug）。
3. **本地配置与仓库分离**：`scripts/config.local.bat`（gitignore）+ `.example` 模板，机器相关路径不进仓库、不进脚本默认值。
4. **reviewer 实跑真能抓 bug**：逐任务审查抓出了「setColor 静默不重绘」「demo 白轴隐形」「PDF 忽略透明开关」「调试黄框泄漏进导出」「SVG/PDF 假成功」等真问题——比阶段末合并审有效得多，继续坚持。
5. **designer 问卷 + 用户拍板省返工**：Phase 1 每个 API/交互点都先问清（14 题 + 3 次补充确认），实现阶段零返工；新功能坚持「设计定稿 → 用户确认 → 再动手」。
6. **任务粒度按用户节奏动态调**：Phase 1 从 10 个小任务调整到「主题 4 + 白轴修复 + 图例 1 + 导出 1 + demo 1」——用户有权随时合并/拆分，captain 只负责把依赖链改对（注意：取消任务前先看有没有下游依赖它，本次 t20/t21 链就踩了这个坑）。
7. **工具链差异必须双测**：MinGW/MSVC 行为与性能不一致（动画卡顿、日志规则等），重要结论都要两边验证过才算数。
8. **Phase 2 沉淀**：①设计文档内部矛盾（闭包签名归属、深度排序方向）在实现期暴露——处理机制有效：engineer 上报 → captain 转 designer 出权威修订 → 修订清单统一回写文档（design_3d.md R1~R4）；②设计文档的矛盾靠 reviewer 实跑（像素断言 nearCoversFar）与 engineer 上报双通道发现，比纯读代码有效；③终验性能数字受环境负载影响（~2× 波动）——复测时先核对代码 mtime，非回归要注明；④「先立零回归门槛再动新功能」策略有效：改名+基类任务先行，旧 69 例全绿后才进 3D 新代码。
