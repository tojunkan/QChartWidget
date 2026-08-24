# 目录模块化重构方案（阶段 1：方案）

> 任务 t52 · 架构监督员 · 状态：**方案先行，用户确认后才动文件**
> 红线：不碰 git；行为零变化（纯搬移）；moc 所有权约定不变；Test/TestUnit 路径若不动则不动。

---

## 1. 现状盘点（已核实的事实）

| 项 | 事实 |
|---|---|
| 库源码布局 | 工作区根目录扁平存放 **52 个头文件 + 36 个 .cpp**（QChart* 家族 + ProjectionToolKit.h + QDataPoint.h / QDataPoint3D.h / QDataRect.h / QChartDebug.h） |
| Windows 镜像骨架 | `/mnt/e/.../QChartWidget/include|src` 下六模块：`animation, axes{/2d,/3d}, core, layers{/2d,/3d}, series{/2d,/3d}, utils`，目前仅 `include/utils/ProjectionToolKit.h` 已移入；骨架即目标，**可增补模块**（用户明示） |
| 工作区 include//src/ | 两目录已存在但为空（未跟踪，git status 干净） |
| 库内 include 写法 | **全部扁平**：`#include "QChartAxis.h"`（无路径前缀）——依赖 CMake 的 `target_include_directories(QChartWidget PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})` |
| Test/demos、TestUnit/tests | 用 **`../../QChartXxx.h` 相对路径**（约 30 个文件、~160 处引用） |
| Test/bench/bench_main.cpp | **已是扁平写法** `#include "QChartWidget.h"`（零改动即兼容未来结构） |
| CMakeLists（工作区与镜像） | 两棵几乎相同（镜像多 windeployqt 块）；`QCHART_SOURCES` 全扁平路径；AUTOMOC 仅库 target ON，demo/test/bench OFF；测试头用 `qt6_wrap_cpp` 手动 moc |
| moc 基线 | build-linux 下库 AUTOMOC 生成 **28 个 moc_*.cpp**（后续需核对集合不变） |
| 头文件名冲突 | 52 个头文件 basename 全部唯一（已审计，扁平命名空间无碰撞） |
| Windows 侧遗留 | `TestUnit/TestUnit.vcxproj`（git 跟踪）引用 `..\QChartAxis.cpp` / `..\QChartSeries.cpp` / `..\QChartAxis.h` / `..\QChartSeries.h` 共 4 处——见 §5 风险 |
| 构建环境 | build-linux：Unix Makefiles + 系统 Qt6（`/usr/lib/x86_64-linux-gnu/cmake/Qt6`）+ `/usr/bin/c++` |

---

## 2. 目标结构（骨架 + 增补）

```
include/ 与 src/ 各自镜像以下结构：
  animation/                # 动画基类与全部 2D 动画
  axes/                     # 轴基类 QChartAxis（2D/3D 共用）
    ├── 2d/                 # QValueAxis QLogAxis QDateTimeAxis QBarCategoryAxis
    └── 3d/                 # QChartAxes3D
  core/                     # 宿主对象 + 相机 + 渲染管线 + 主题/图例
  layers/                   # 图层基类 QChartLayer（2D/3D 共用）
    ├── 2d/                 # （骨架预留；当前无专属文件）
    └── 3d/                 # QChartLayer3D
  series/                   # 系列基类 QChartSeries（2D/3D 共用）
    ├── 2d/                 # QXYSeries 及派生 2D 系列
    └── 3d/                 # QChartSeries3D 及派生 3D 系列
  utils/                    # 数学/调试/数据值类型/工具箱（ProjectionToolKit 随镜像先例）
  projection/               # 【增补模块】投影家族（2D/3D 投影 + 工厂）
```

约定：**基类放模块根**（QChartAxis→axes/、QChartLayer→layers/、QChartSeries→series/），2D/3D 专属放对应子目录。

---

## 3. 全量文件 → 模块映射表（88 文件）

### 3.1 头文件（52 → include/）

| 文件 | 去向 | 文件 | 去向 |
|---|---|---|---|
| QBarAnimation.h | animation | QChartRenderer.h | core |
| QChartAnimation.h | animation | QPainterChartRenderer.h | core |
| QNumericSeriesAnimation.h | animation | QOpenGLChartRenderer.h | core |
| QViewRectAnimation.h | animation | QChartGL.h | core |
| QProjectionSwitchAnimation.h | animation | QChartHitTester.h | core |
| QChartAxis.h | axes | QChartLayer.h | layers |
| QBarCategoryAxis.h | axes/2d | QChartLayer3D.h | layers/3d |
| QValueAxis.h | axes/2d | QChartSeries.h | series |
| QLogAxis.h | axes/2d | QXYSeries.h | series/2d |
| QDateTimeAxis.h | axes/2d | QLineSeries.h | series/2d |
| QChartAxes3D.h | axes/3d | QScatterSeries.h | series/2d |
| QChartWidget.h | core | QPolygonSeries.h | series/2d |
| QChartWidget3D.h | core | QBarSeries.h | series/2d |
| QChartCamera.h | core | QRegionSeries.h | series/2d |
| QChartCamera3D.h | core | QChartSeries3D.h | series/3d |
| QChartTheme.h | core | QChartLineSeries3D.h | series/3d |
| QChartLegend.h | core | QChartScatterSeries3D.h | series/3d |
| QChartSurfaceSeries.h | series/3d | QChartProjection.h | projection |
| QChartMath.h | utils | QChartProjectionFactory.h | projection |
| QChartDebug.h | utils | QCartesianProjection.h | projection |
| QDataPoint.h | utils | QPolarProjection.h | projection |
| QDataPoint3D.h | utils | QFunctionalProjection.h | projection |
| QDataRect.h | utils | QInterpolatedProjection.h | projection |
| ProjectionToolKit.h | utils | QChartProjection3D.h | projection |
| QChartCartesianProjection3D.h | projection | QChartCylindricalProjection3D.h | projection |
| QChartSphericalProjection3D.h | projection | QChartFunctionalProjection3D.h | projection |

### 3.2 源文件（36 → src/，与 QCHART_SOURCES 一一对应）

| 文件 | 去向 | 文件 | 去向 |
|---|---|---|---|
| QBarAnimation.cpp | animation | QChartHitTester.cpp | core |
| QChartAnimation.cpp | animation | QChartGL.cpp | core |
| QNumericSeriesAnimation.cpp | animation | QChartAxis.cpp | axes |
| QViewRectAnimation.cpp | animation | QBarCategoryAxis.cpp | axes/2d |
| QProjectionSwitchAnimation.cpp | animation | QValueAxis.cpp | axes/2d |
| QChartWidget.cpp | core | QLogAxis.cpp | axes/2d |
| QChartWidget3D.cpp | core | QDateTimeAxis.cpp | axes/2d |
| QChartCamera.cpp | core | QChartAxes3D.cpp | axes/3d |
| QChartCamera3D.cpp | core | QChartLayer.cpp | layers |
| QChartTheme.cpp | core | QChartLayer3D.cpp | layers/3d |
| QChartLegend.cpp | core | QChartSeries.cpp | series |
| QChartRenderer.cpp | core | QXYSeries.cpp | series/2d |
| QPainterChartRenderer.cpp | core | QLineSeries.cpp | series/2d |
| QOpenGLChartRenderer.cpp | core | QScatterSeries.cpp | series/2d |
| QPolygonSeries.cpp | series/2d | QBarSeries.cpp | series/2d |
| QRegionSeries.cpp | series/2d | QChartSeries3D.cpp | series/3d |
| QChartLineSeries3D.cpp | series/3d | QChartScatterSeries3D.cpp | series/3d |
| QChartSurfaceSeries.cpp | series/3d | QInterpolatedProjection.cpp | projection |

> 核对：52 头 + 36 源 = 88，与根目录实际文件数一致；QCHART_SOURCES 36 项全覆盖。

---

## 4. 五个必答要点

### 4.1 ① QChartProjection 家族（含 QChartProjectionFactory）去向：**新增 projection 模块（推荐）**

共 12 头 1 源（QChartProjection.h、QChartProjectionFactory.h、QCartesianProjection.h、QPolarProjection.h、QFunctionalProjection.h、QInterpolatedProjection.h/.cpp、QChartProjection3D.h、QChartCartesianProjection3D.h、QChartCylindricalProjection3D.h、QChartSphericalProjection3D.h、QChartFunctionalProjection3D.h）→ `include/projection` + `src/projection`。

**推荐：新增模块。理由：**
1. **强内聚特性族**：同一抽象（QChartProjection / QChartProjection3D）+ 派生集 + 工厂（QChartProjectionFactory），是完整独立的 feature，与动画/图层/系列同构。
2. **core 已重**：core 候选已有 11 对文件（widget×2、camera×2、theme、legend、renderer×3、GL、hitTester），再加 12 个投影会稀释 core 职责（core 应 = 宿主对象 + 渲染管线）。
3. **镜像明示可增补**：用户提示"镜像可能不全面，可增补模块"，projection 是最大的无归属家族，正是该机制的用例。
4. **演进方向**：ROADMAP 中 3D/函数投影继续扩展，独立模块便于按特性演进，避免 core 持续膨胀。
5. **依赖方向不构成反对**：QChartWidget.h 虽直接 include QChartProjection.h（widget 深度使用投影），但 series/layers 同样被 widget 使用却各自成模块——"被谁用"不决定"放哪"。

**备选（不推荐）：放 core**——少一个模块、投影与 widget 耦合直观；但牺牲职责清晰度与演进空间。

> 附：渲染管线（QChartRenderer / QPainterChartRenderer / QOpenGLChartRenderer / QChartGL / QChartHitTester）骨架无对应模块，**推荐放 core**（它们是 widget 的渲染内核，QChartWidget.h 直接 include 它们；若未来渲染层持续膨胀可再拆 `renderer` 模块，记入后续演进建议，本次不拆）。

### 4.2 ② include 引用策略：**`target_include_directories` 加 `include/` 根 + 源内保持 `"QChartWidget.h"` 扁平写法（推荐）**

**方案 A（推荐）：** 库 PUBLIC include 目录由 `${CMAKE_CURRENT_SOURCE_DIR}` 改为 `${CMAKE_CURRENT_SOURCE_DIR}/include`，**库内 88 个文件的 #include 一行不改**（全部扁平写法经 include/ 根解析）。

**论证（A vs B：相对模块路径）：**

| 维度 | A：include/ 根 + 扁平（推荐） | B：相对模块路径 `"core/QChartWidget.h"` |
|---|---|---|
| 库内改动量 | **0 行**（52 头 + 36 源的 #include 全部不动） | 全部 ~180 处库内 include 重写 |
| 消费者改动量 | Test/TestUnit 的 `../../` → 扁平（~160 行，机械替换） | 同样要改，且需知道每个类的模块归属 |
| 出错风险 | 最低；纯搬移 + 少量机械替换 | 高；漏改/写错路径即编译失败，且与"行为零变化"相悖 |
| 与既有习惯 | 兼容（库内与 bench 本就是扁平） | 引入新约定，两套写法并存过渡 |
| 可读性/自文档 | 弱（include 处看不出模块） | 强（include 处即拓扑） |
| 命名空间冲突 | 扁平命名空间；已审计 52 头 basename 唯一；用 Q 前缀约定防未来碰撞 | 天然无冲突 |
| 定位机制 | 引号 include 先查本文件目录再查 include/；src/ 与 include/ 分离，无同目录遮蔽问题 | 引号 include 先查本文件目录，`"core/..."` 在本目录解析失败后落到 include/，同样依赖 include 路径 |

**结论**：A 改动最小、风险最低、与现状兼容，符合"纯搬移/行为零变化"红线；B 的自文档收益不值得本次一次性重写 ~180 处 include。若未来想提升可读性，可后续单独做"include 路径美化"小任务（不影响构建行为）。**消费者（Test/TestUnit）统一改为扁平写法**，经库的 PUBLIC include/ 传递可见——bench 已是扁平，恰好验证该机制。

### 4.3 ③ CMakeLists 改造点（仅两处，其余不动）

```cmake
# 1) QCHART_SOURCES：36 项全部加模块前缀，例如：
-    QChartCamera.cpp
+    src/core/QChartCamera.cpp
-    QChartAxes3D.cpp
+    src/axes/3d/QChartAxes3D.cpp
-    QInterpolatedProjection.cpp
+    src/projection/QInterpolatedProjection.cpp
     # ……（完整清单见 §3.2，逐项加前缀）

# 2) 库 include 目录：根 → include/
- target_include_directories(QChartWidget PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
+ target_include_directories(QChartWidget PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

**明确不变**：
- `AUTOMOC` 约定不变：仅库 target ON；QChartDemo / QChartTests / QChartBench 保持 OFF；`qt6_wrap_cpp` 手动 moc 测试头不变（测试头不动）。
- 头文件仍不列入 QCHART_SOURCES（AUTOMOC 经 include/ 递归扫描头文件；见 §5 风险 R1）。
- Test/TestUnit 目标的源列表与 include 目录声明**一行不改**（扁平 include 经库 PUBLIC 传递可见；多余的 `${CMAKE_CURRENT_SOURCE_DIR}` 保留无害，最小 diff）。
- 两棵树同补丁：工作区 CMakeLists 与镜像 CMakeLists 的这两处改动完全一致（镜像多出的 windeployqt 块不动）。工作区无 CMakePresets.json（Windows 镜像独有）。

### 4.4 ④ Test/TestUnit 是否随迁：**保持原位，只改 include 引用（推荐）**

**论证：**
1. **它们是消费方，不是库构件**：Test/demos（统一入口 demos.h）、TestUnit/tests（每类一测）+ main.cpp 聚合的内部结构已模块化，与库的目录布局无关。
2. **改动面最小**：只需把 `../../QChart*.h` / `../../QData*.h` / `../../ProjectionToolKit.h` 统一改扁平（经 PUBLIC include/ 解析），main.cpp 的 `"tests/..."`、demos 的 `"demos.h"`、测试头互引**全部不动**。
3. **随迁代价大且零收益**：QCHART_TEST_HEADERS、测试源列表、qt6_wrap_cpp 路径、以及 git 跟踪的 TestUnit.vcxproj 全要跟着动。
4. **红线自洽**：任务红线即"Test/TestUnit 若不动路径则不动"。
5. bench 同理：Test/bench/bench_main.cpp 保持原位，其扁平 include **天然零改动**。

### 4.5 ⑤ 执行顺序与风险（阶段 2，用户确认后执行）

**执行顺序（9 步）：**
1. **基线**：`QT_QPA_PLATFORM=offscreen ctest --test-dir build-linux` 记录基线（预期 180 PASS + 2 SKIP）；快照 `build-linux/QChartWidget_autogen/EWIEGA46WW/moc_*.cpp` 集合（28 个）。
2. **建目录**：`mkdir -p include/{animation,axes/2d,axes/3d,core,layers/2d,layers/3d,series/2d,series/3d,utils,projection} src/{同}`。
3. **搬移**：按 §3 映射 `mv`（**纯 mv，不用 git mv**；git 由用户操作）。
4. **改 CMakeLists**：§4.3 两处补丁。
5. **改消费者 include**：Test/test.cpp（`../QChartWidget.h`→扁平）、Test/demos/*.cpp 与 TestUnit/tests/*.cpp 的 `"../../X.h"`→`"X.h"`（仅库头；`demos.h`/`tests/...` 不动）。全部机械替换。
6. **全树 grep 校验**：
   - 无残留 `#include "\.\./` 指向库头（Test/TestUnit/src/include 范围）；
   - src/include 内每个 `#include "Q*.h"` 都能在 include/ 下解析（脚本逐一 test -f）；
   - 根目录不再有库 .h/.cpp。
7. **重建**：`cmake -S . -B build-linux`（重新生成）→ `cmake --build build-linux -j$(nproc)` → **0 error / 0 warning**；核对 moc 集合 == 基线（28 个）。
8. **ctest**：`QT_QPA_PLATFORM=offscreen ctest --test-dir build-linux --output-on-failure` → **180 PASS + 2 SKIP**。
9. **GL 冒烟**：wayland/xcb 会话下跑 3D demo（surface3d / scatter3d / line3d），验证 GL 上下文创建与渲染无崩溃。

**风险表：**

| # | 风险 | 缓解 |
|---|---|---|
| R1 | AUTOMOC 对 include/ 子目录头文件的发现（Q_OBJECT 头移到子目录） | 步骤 7 核对 moc 集合与基线一致（28 个）；若缺失，兜底：把 Q_OBJECT 头显式列入库 target 源（CMake 允许头文件入源列表，AUTOMOC 必扫） |
| R2 | 消费者相对 include 漏改（`../../`、`../`） | 步骤 6 三重 grep；范围含 Test/、TestUnit/、src/、include/、scripts/ |
| R3 | 扁平命名空间未来撞名 | 已审计 52 头唯一；约定"Q 前缀 + 职责名"防碰撞 |
| R4 | Windows 侧 TestUnit.vcxproj（git 跟踪）引用 `..\QChartAxis.cpp` 等 4 处旧路径 | Linux 构建不受影响；**按红线我们不碰**，用户同步到 Windows 时需更新（或改由 CMake 生成）——在交付消息中明确提示 |
| R5 | build-linux 缓存陈旧（源增删） | 重新 configure 即可（CMake 自动感知）；异常时用全新目录验证一次 |
| R6 | 文档（README/ROADMAP/design*.md）可能提及旧扁平路径 | 纯文档、非行为；本次不改（可选后续小任务） |
| R7 | 搬移途中误伤（漏移/错移） | 步骤 6 的根目录核对 + 步骤 7 编译兜底；映射表 §3 即为唯一依据 |

---

## 5. 验收标准（硬性）

1. 根目录库文件清零（仅保留 Test/ TestUnit/ docs/ scripts/ build-*/ 等非库文件）。
2. 库内 #include **零改动**（除搬移外无任何逻辑差异，行为零变化）。
3. 增量构建 **0 error / 0 warning**；moc 集合与基线一致（AUTOMOC 仅库 target）。
4. ctest **180 PASS + 2 SKIP**（offscreen）。
5. wayland GL 冒烟通过（3D demo 正常渲染）。
6. `git status` 仅显示搬移导致的增删（git 由用户确认后自行操作，我们不执行）。

---

## 6. 待用户确认清单

- [ ] ① projection 家族 → **新增 projection 模块**（而非 core）？
- [ ] ② include 策略 → **include/ 根 + 扁平写法**（库内零 include 改动，消费者 `../../`→扁平）？
- [ ] ③ CMakeLists 仅改两处（QCHART_SOURCES 前缀 + PUBLIC include 目录），AUTOMOC/moc 约定不变？
- [ ] ④ Test/TestUnit 保持原位、只改 include 引用？
- [ ] ⑤ 执行顺序 9 步 + 验收标准（0 error/0 warning + 180 PASS/2 SKIP + GL 冒烟）？
- [ ] 附注：Windows 侧 TestUnit.vcxproj 4 处旧路径由用户同步时处理（Linux 侧不受影响）。

> 确认后执行阶段 2。以上方案由架构监督员产出，请队长转用户确认。

---

## 7. 阶段 2 执行记录（用户确认后追加）

**执行结果**：88 文件全部按 §3 映射搬移完成（52 头 + 36 源）；消费者 32 文件 159 处 `../../` 相对 include 机械改扁平；三重 grep 校验通过（无相对 include 残留 / src+include 内 157 处扁平 include 全部可解析 / 根目录库文件清零）。

**方案修正两处（实现细节，行为与已确认决策一致）**：
- **修正 1（include 目录）**：§4.3 原写 "PUBLIC include 目录改 `${CMAKE_CURRENT_SOURCE_DIR}/include`"。执行中发现：头文件按模块放入 `include/<module>/` 子目录后，编译器对 `-I include` **不递归**，扁平 `#include "QChartAxis.h"` 无法解析 `include/axes/QChartAxis.h`。修正为 PUBLIC include 目录 = `include/` 根 + **13 个模块目录显式列出**（animation、axes、axes/2d、axes/3d、core、layers、layers/2d、layers/3d、series、series/2d、series/3d、utils、projection）。仍保持"库内扁平 include 零改动 + 消费者扁平化"的已确认决策；全部 52 个头文件 basename 唯一（已审计），多目录 -I 无遮蔽风险。
- **修正 2（R1 兜底实测生效）**：首次重建链接失败——undefined reference 指向 moc 符号。诊断：CMake 3.28 automoc 对"被源文件引用的头文件"**只解析相对源文件目录**，无法经 include 目录命中 `include/<module>/` 子目录头（deps 中 0 个 include/ 路径；moc 文件为搬移前旧产物，内容引用旧根路径）。按方案 R1 兜底：新增 `QCHART_HEADERS`（52 头显式列入），`add_library` 源含 `${QCHART_SOURCES} ${QCHART_HEADERS}`——automoc 对列入目标源的头必扫。moc 所有权不变（AUTOMOC 仍仅库 target ON，demo/test/bench OFF）。

**最终验证（全部通过）**：
- 全量重建（`--clean-first`）**0 error / 0 warning**；三个可执行（QChartDemo / QChartTests / QChartBench）+ 静态库全部链接成功。
- **moc 集合 28 == 基线**（同名集合；CMake 3.28 按源目录哈希分目录存放），内容引用**新路径**（如 `include/axes/QChartAxis.h`、`include/core/QChartWidget.h`）；mocs_compilation.cpp 包含 28 个 moc。
- **ctest：180 PASS + 0 FAIL + 2 SKIP**（20 测试类，offscreen，与基线一致）。
- **wayland GL 冒烟**：scatter3d / line3d / surface3d 各运行 ≥10s 无崩溃（exit=124 超时正常结束），"已启动演示"均记录，渲染活动正常；环境无 GPU（libEGL 回退软件渲染，与基线测试备注一致）。
- 红线：未碰 git；Test/TestUnit 路径未动；moc 约定不变；库内 #include 零改动（行为零变化，纯搬移）。
