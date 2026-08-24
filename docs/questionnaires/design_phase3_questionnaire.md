# 《Phase 3 GPU 实时渲染》设计问卷

> 背景：用户因 QtGraphs（QML/Quick 引擎 100MB+ 内存）才自研本库——**内存足迹是最敏感点**；Phase 2 基准结论=瓶颈在 O(N) 无条件投影（culling 收益仅 2.7%~5.3%）；Phase 2 完成态已备齐 GPU 落点（viewCube 主状态相机/viewProjectionMatrix、图元列表=命令缓冲雏形、深度数组=z-buffer 降级路径、QChartPrimitive::Layer 分层、billboard 标签、QChartRenderer 接口参数化于 QPaintDevice）。
> 队长语言调研结论（已反馈用户）：推荐 **OpenGL 3.3 Core**（QOpenGLWidget + QOpenGLShaderProgram + VBO/VAO）——Qt 6.2+ 零升级、QWidget 原生集成 + QPainter overlay、Phase 2 资产全部直接对应（viewProjectionMatrix→uniform、projectBatch→顶点着色器、图元列表→VBO、painter's algorithm→z-buffer）、翻译器计划 GLSL 目标最简、Qt 6 未弃用 OpenGL（弃用的是内部 RHI 迁移）。
> 每问：候选选项 + ★推荐项（置首，一句话理由）。共 10 题，可快速拍板。

## Q1. 语言/技术栈定案 + Qt 最低版本？
- ★A. **OpenGL 3.3 Core**（QOpenGLWidget + QOpenGLShaderProgram + VBO/VAO），**保持 Qt 6.2+**。理由：Qt 6.2 零升级兼容（Linux 6.4.2 现成）、QWidget 原生集成、Phase 2 资产逐一对应、GLSL 330 是翻译器最简目标、Qt 未弃用 OpenGL。
- B. QRhi + QRhiWidget：需 Qt ≥6.9（unstable API）、升级整条工具链；红利是未来 Qt 内部一致，但当下成本高。
- C. Vulkan：QWidget 集成差、SPIR-V 编译链成本高，生态成本不值。

## Q2. Shader 语言？
- ★A. **GLSL 330 字符串**（QOpenGLShaderProgram 直接编译）。理由：调试直观（InfoLog）、无编译链工具依赖、是用户「shader 翻译器」构想的最简输出目标。
- B. SPIR-V / qsb 打包：引入离线编译链 + 工具链依赖，Phase 3 无收益。

## Q3. 宿主方式（含多实例共享上下文）？
- ★A. **QChartWidget3D 内嵌 QOpenGLWidget 子部件（组合）**：GL 部件覆盖 plotArea，paintGL 走 QOpenGLChartRenderer；QPainter overlay（billboard 标签/图例）画在 GL 部件合成层；多实例经 QOpenGLContext 共享（shareContext）复用 shader/program。理由：QChartWidget3D 现有继承链（: QChartWidget）与交互/联动/导出全保留，GL 是局部实现细节，多实例共享资源成本低。
- B. QChartWidget3D 化身 QOpenGLWidget（继承）：与现有继承链冲突，2D overlay/导出路径重构面大。
- C. 渲染到 FBO 再 QPainter 上屏：每帧 readback 是 CPU-GPU 同步瓶颈，性能最差，否决。

## Q4. 内存预算（用户最敏感点，必须量化验收）★ 推荐下表与口径：
| 资源 | 预算 | 核算 |
|---|---|---|
| GL 上下文 + 程序对象 | ≤ 6MB | QOpenGLWidget 上下文 + GLSL 330 程序 ×3~5；**零纹理**（无字形 atlas） |
| 深度缓冲 | widget 尺寸 × 4B | 1280×720 ≈ 3.7MB；按尺寸分配一次，resize 重建，不逐帧分配 |
| 简单 3D 图 VBO（1 万散点+线+曲面 64×64+轴网格） | ≤ 3MB | ~1.2 万顶点 × 16B ≈ 0.2MB + 批次余量 |
| 1M 点 VBO（World float3 + rgba） | ≤ 24MB | 16B/点（12B pos + 4B color）；**静态上传一次**，数据变化才增量 glBufferSubData |
| 1M 点 CPU 数值预转换缓存（float3） | ≤ 12MB | 12B/点；与 VBO 源合并（同一数组），**无第二份拷贝** |
| billboard 标签/图例 | ≈ 0 | QPainter 字体管线复用，无 glyph atlas/无 QML/Quick 栈 |
| **合计（RSS 增量上限）** | **简单图 ≤ 25MB；1M 点 ≤ 60MB** | 对比 QtGraphs ~100MB+，目标为其 1/3~1/2 |

- ★A. 按上表（预算 + 验收口径：真实 GL 环境实测 RSS 增量，简单图 ≤30MB、1M 点 ≤70MB = 预算 +20% 容差；Linux 用 /proc/self/status VmRSS、Windows 任务管理器/GetProcessMemoryInfo；惰性初始化——首个 GL 部件出现才建上下文；GL 资源随 widget 析构释放、共享上下文引用计数）。理由：用户痛点量化闭环，预算可测可验收。
- B. 只定总量（≤60MB）不定分项：不可拆解定位超标来源。
- C. 不做内存验收（只测帧率）：用户核心关切悬空，否决。

## Q5. 渲染架构：QOpenGLChartRenderer 如何消费 QChartScene？
- ★A. **图元列表→VBO 批次 + z-buffer 替换深度排序**：Point→GL_POINTS、LineSegment→GL_LINES（Phase 2 图元列表直接映射）；Grid/Series 批次开深度测试（网格批次加深度偏置——glPolygonOffset(GL_LINE) 或 shader uniform，同 Phase 2 kGridDepthBias 语义）；**ForegroundDecor（盒边/spine/刻度点）关深度测试后画（保持 A7 恒可见语义）**；标签/图例走 QPainter overlay（billboard 文本 drawText，字体管线复用）。理由：Phase 2 资产（图元列表/分层/深度语义）全部直接对应，painter's algorithm 自然退休。
- B. 每帧全量重建图元并重传：实现简单但 CPU 投影+上传瓶颈保留（基准已证）。
- C. 引入完整场景图/命令缓冲抽象：超出 Phase 3 范围，过度设计。

## Q6. 投影上 GPU 的路径 + 范围边界（翻译器后置）？
- ★A. **CPU 一次性数值预转换缓存产出 World float3（attribute）→ 顶点着色器仅 viewProjectionMatrix uniform（GPU 逐顶点投影）**；固定投影硬编码 GLSL（本阶段范围）；「数学积木 + shader 翻译器」记入 **Phase 3.5**（用户构想，之后再说）。理由：静态 VBO 上传一次、相机交互只更新 64B uniform、每帧零 CPU 投影——直接消灭 Phase 2 基准瓶颈；与数值预转换缓存（D-3D-10 性能项）天然合并；硬编码 shader 先行、翻译器后置，范围干净。
- B. attribute=Numeric 坐标 + toWorld 进 GLSL（每类投影一个顶点着色器）：CPU 完全零投影，但固定投影硬编码 shader 数=投影类数，翻译器落地前收益不划算。
- C. CPU 预投影传屏幕坐标：复用现有闭包，但每帧 O(N) 投影 + 动态 VBO 重传，瓶颈保留，否决。

## Q7. 光照范围？
- ★A. **线框 + 深度先行，光照后置**（Phase 3 验收=性能/交互目标：10 万点 60fps、1M 可交互；光照是视觉增强，不影响验收线）。理由：Phase 2 视觉基线（线框+深度）延续，光照另立子项不阻塞主验收。
- B. 含简单方向光（lambert）逐顶点：shader 复杂度+，但视觉提升有限（线框为主）。
- C. 多光源/阴影：超范围，否决。

## Q8. 拾取范围？（用户：GPU 拾取正是 GPU 的好机会）
- ★A. **GPU 颜色编码拾取**（用户方向）：主 pass 加**第二颜色输出**（fragment shader 输出图元 ID 编码色，零额外几何开销；ID=(layer,series,dataIndex) 编码进 RGB24），光标处 `glReadPixels(1×1)` 读回 → 解码查表 → (series, dataIndex)。**交互改动清单**：①`QChartRenderer` 接口加 `pick(QPoint)` 虚方法（QPainter 后端=现 CPU 近邻，GL 后端=颜色编码读回）；②`QChartWidget3D::updateHover` 按后端分支（信号 `uvHovered/uvSelected/uvHoveredEnd` 与联动逻辑**零改动**）；③拾取 pass 开 depth test → 命中「实际可见」图元（比 CPU 近邻更正确，遮挡语义天然成立）；④仅鼠标移动时触发（非每帧），1 像素读回 µs~ms 级；⑤轴/网格图元（dataIndex=-1）不编码天然排除；⑥数据变化时 ID 表重建。理由：交互事件流与信号不变，只换命中实现；GPU 深度测试让拾取比 CPU 近邻更正确。
- B. 屏幕近邻保留（CPU 遍历，不做 GPU 拾取）：GL 下系列在 GPU、CPU 近邻需每帧投影，回到瓶颈。
- C. 射线拾取（unproject 已预留）：Phase 4 再上，本阶段不做。

## Q9. 验收与基准（量化口径 + **硬件基线，用户定案：到这一层不考虑硬件不行**）？
- ★A. **口径量化 + 硬件基线绑定**：
  - **基线平台（验收必须记录并复测于同硬件）**：CPU/GPU/驱动版本/Mesa 或厂商驱动、分辨率（默认 1920×1080，可加 1280×720 档）、**构建类型 Release**（Phase 2 教训：MSVC Debug 比 Linux 慢一个量级，必须同构建类型对比）、Qt 版本、后端（WSLg/桌面原生）；**双工具链（MSVC + MinGW）各测一遍**（Phase 2 教训）。
  - **性能口径**：10 万点中位帧 ≤16.7ms（≥55fps）且交互延迟 ≤50ms；1M 点中位帧 ≤50ms（≥20fps）且拖拽无卡死；帧耗时 = 相机旋转连续 200 帧的中位（排除首帧 shader 编译）；交互延迟 = 输入事件 → 画面更新。
  - **内存口径**：按 Q4 预算表实测 RSS 增量（Linux /proc/self/status、Windows GetProcessMemoryInfo），同硬件同构建类型。
  - QChartBench 扩展 GL 场景（VBO 上传/相机旋转帧耗时/RSS 增量），输出 CSV 落 build 目录。
  - 软渲染兜底说明：llvmpipe 软件 GL 只作 CI/无 GPU 环境冒烟，不作性能验收基准。
  理由：硬件/构建类型/分辨率/工具链全部绑定后，数字才有复测可比性（Phase 2 已两次踩环境负载与 Debug/Release 差异的坑）。
- B. 只测「能跑 60fps」不写硬件：无法复测、无意义。
- C. 硬件随意、只测帧耗时：内存与可复现性缺失。

## Q10. demo/回归策略？
- ★A. **后端可切换**（编译/运行开关：GL 默认、QPainter 保底）；3D demo 切 GL 后端（交互/联动/轴网格观感不变）；**2D 路径与导出（PNG/SVG/PDF）仍走 QPainter renderUncached**（GL 不参与导出）；**旧 158 用例锁 QPainter 路径零回归**；GL 新测试在真实 GL 环境跑（offscreen 平台无 GL → 条件 skip）。理由：GL 只替换 3D 屏显路径，2D/导出/测试基线全部不动，回退开关兜底。
- B. GL 全面接管（含 2D/导出）：重构面大、导出矢量语义破坏，否决。
- C. 删 QPainter 3D 后端：失去对照与回退，否决。

---

> 备注：①队长语言调研结论作为 Q1/Q2 的 ★ 基础，仍待用户拍板；②Q4 内存预算为建议值（简单图 ≤30MB / 1M 点 ≤70MB RSS 增量），用户可调整上限；③FunctionalProjection 数学积木 + shader 翻译器明确记入 Phase 3.5（Q6 范围边界），本阶段只做固定投影硬编码 GLSL。
