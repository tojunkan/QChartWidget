# semantic_vs_implementation_audit.md —— 语义 vs 实现差异审计清单

> t54 审计产出（只审计，未改任何代码/文档）
> 方法：按 5 类核查（①声明未实现 ②实现与文档语义偏差 ③文档未记录的实现决策 ④死代码/遗留注释 ⑤观察项核对 O1~O4），逐条对照设计文档（docs/design/*）、ROADMAP、代码（include//src/）与 t53 文档套件。
> 结论摘要：**5 类共 16 项发现**（①:2 ②:4 ③:5 ④:5 ⑤:核对表 16 项，其中 4 项未落入文档）；另有 4 项一致性核查**通过**（R6 平移、统一后端、Lattice、GL overlay）。建议分布：改文档 7 处 / 改代码注释 4 处 / 维持+记录 5 项。

---

## ① 声明未实现（declared but not implemented）

| # | 位置 | 设计声称 | 实际行为 | 差异类型 | 建议 |
|---|---|---|---|---|---|
| A1 | ROADMAP:241「3D tooltip → Phase 3+」；QChartWidget3D.h:82「不弹 tooltip（D-3D-13）」 | 3D tooltip 后置 Phase 3+ | 确实未实现，且**文档明确**（D-3D-13 用户定案不弹 tooltip） | ①（但为文档化的未来项） | **维持+记录**（符合文档，非缺口） |
| A2 | design_3d_axes §2.3 + QChartMath.h `unproject` | 射线拾取 Phase 3+ 预留 | `unproject` 已实现并单测，但**无调用方**（功能未接线；R5 反算明确"无逆矩阵/unproject"） | ①（API 已实现、功能未启用） | **维持+记录**（预留 API 就绪，按路线图后置；建议 t53 文档已注明"Phase 3 预留"） |

> 核查排除：GL overlay 标签（GlHost::paintGL 已实现 QPainter overlay，见 §4 通过项）、Lattice 网格（collectPrimitives else 分支已实现晶格三族）、非数值 Axis 转换（D19 明确 Phase 2 边界不变，QVariant 路径保留——符合文档）。

## ② 实现与文档语义偏差

| # | 位置 | 设计声称 | 实际行为 | 差异类型 | 建议 |
|---|---|---|---|---|---|
| B1 | docs/design/design_3d.md §11.1:772「viewDepth_viewSpaceZ：viewMatrix 下前方点 depth>0、**越近越大**」 | depth 语义=越近越大 | 实现（QChartMath.h `viewDepth`）+ §3:121 = `−viewZ`，**越大越远**（排序降序=D16）；§11.1 措辞矛盾（t6 审查已知，未修） | ②（文档措辞与实现/同文档他节矛盾） | **改文档**（§11.1 改为"越远越大"，与 §3/实现一致） |
| B2 | docs/design/design_phase3.md §7.3「程序池：按 kind 缓存（**share group 内复用**）」；QChartGL.h:5 同措辞 | 程序池 share group 内复用 | 实现 = `QHash<QOpenGLContext*, QHash<int, program>>`（**按上下文建池**，t44 落实 t43 O2：Qt 6.4.2 无 setShareContext → 正确性优先） | ②/③（实现决策与文档描述不符） | **改文档**（§7.3 补"按上下文建池；Qt≥6.5 共享后程序跨上下文可用，池按上下文冗余=正确性优先"） |
| B3 | docs/design/design_phase3.md §7.1 预算表 | 1M 点行仅列「CPU 数值预转换缓存（World float3）≤12MB」单行；合计 66.5MB | t49 实测构成 = numericCache **12** + worldCache **12** + VBO 16 + pickTable 16 + 开销 ≈ **65.9MB**（两缓存各 12MB；表未分列；合计条目口径不同：表含 FBO 14.5+GL 6，实测不含） | ②/④（预算条目口径不一致） | **改文档**（分列 numericCache/worldCache 各 12MB，注明 RSS 口径差异）或**维持+记录**（验收 65.9≤70 达标，差异为条目口径） |
| B4 | docs/core/module_core.md §2 类清单表（t53） | QChartGL / QOpenGLChartRenderer / QChartTheme 标 Q_OBJECT=✓ | 三头注释明确"非 Q_OBJECT"；moc 集合 28 个无对应文件（QChartTheme 纯数据、GL 类无信号/槽） | ②（t53 文档与实现偏差——自查发现） | **改文档**（module_core.md 表格三行改为 —） |

## ③ 文档未记录的实现决策

| # | 位置 | 实现决策 | 文档状态 | 建议 |
|---|---|---|---|---|
| C1 | QChartHitTester.h:17-18（头注释） | `HitResult.series` **非 const**（任务描述草稿的 const 按此修正：既有调用方 `QChartSeries* s = result.series` 赋值零改动） | 仅头注释记录；design_phase3 §8.1 与 t53 文档未提 | **维持+记录**（头注释已充分）或改文档补一句 |
| C2 | QChartGL.cpp `s_programs`（t44） | 程序池按上下文建池（含 Qt 版本守卫语义） | 见 B2 | 改文档（并入 B2） |
| C3 | QOpenGLChartRenderer.cpp 顶点着色器（t44） | 主 pass line/point 程序 `u_baseId/u_vertPerPrim` 被链接器优化（v_primId 未被片元消费；setUniformValue 对缺失 uniform 静默 no-op）——t45 O2 | 未落入文档 | **维持+记录**（低优先级，无功能影响；如需主 pass 调试 ID 可让片元消费 v_primId） |
| C4 | QChartGL.cpp 程序池（t45 O3/O4 + t47 O4） | ①上下文键地址复用理论风险（建议 aboutToBeDestroyed 清理或代际校验）②releasePrograms 无 current 时 qDeleteAll（glDeleteProgram no-op，安全但非规范） | 未落入文档（t50 已列为观察项） | **维持+记录**（低概率；t50 保持观察，后续加固） |
| C5 | QOpenGLChartRenderer.cpp `pickIdAt` y 翻转（t47 O3） | `pa.height()-1-pos.y` 与 GL 视口像素存在半像素对齐差异（线宽 1px 栅格化 2 行）；拾取带内稳定命中 | t53 deepdive_picking_idframe 仅提 y 翻转，未提半像素细节 | **维持+记录**（非缺陷；t50 验收交互手感留意）或改文档补一句 |

## ④ 死代码/遗留注释

| # | 位置 | 内容 | 差异类型 | 建议 |
|---|---|---|---|---|
| D1 | include/projection/QChartProjection.h:12 | `//Q_LOGGING_CATEGORY(logProjection, "chart.projection")` 注释掉死行（Phase 0/1 遗留；logProjection 已在 QChartDebug.h 正式声明） | ④ | **改代码**（删行）或维持 |
| D2 | include/projection/QChartProjection.h 全文件 | `<summary>` XML 风格注释 + 制表符/空格混排（Phase 0/1 风格遗留，其余头为 // 风格） | ④（风格，非行为） | **维持**（低优先级；如需统一另行处理） |
| D3 | include/core/QChartGL.h:5-6 头注释 | 「shader 编译属实现③ t44，本任务留接口 + 引用计数骨架，**program() 暂返回 nullptr 占位**」——t42 时代注释；t44 已实现 program() | ④（过时注释） | **改代码**（更新头注释为现状） |
| D4 | src/core/QOpenGLChartRenderer.cpp:100 注释 | 「③ Overlay（billboard 标签/图例，§6）**属 t46**」——t46 已完成且 overlay 已在 GlHost::paintGL 实现（QChartWidget3D.cpp:53-64） | ④（过时注释） | **改代码**（更新注释，或删除该行） |
| D5 | ROADMAP.md:232/236（Phase 2 完成项验收文字） | :232「dolly/**panTarget/fitToBounds**、Q_PROPERTY(**position/lookAt/fovY**)」——R5 前 API 名（现为 panViewCube/setViewCubeToFit + viewCubeCenter/Size/yaw/pitch/fovY）；:236「交互（左键 orbit / 滚轮 dolly / **右键 pan**）」——R6 无平移手势后过时 | ④（过时表述） | **改文档**（按 R5/R6 现状更新；:249 同节已是正确 R6 表述） |
| D6 | Test/demos/demo_line3d.cpp:1 | 「3D 参数螺旋线（动态：**相机沿路径移动**）」——措辞与 R5 派生相机模型略松（动画实为 viewCubeCenter+yaw，:4-5 已正确说明） | ④（措辞） | **维持+记录**（第 4-5 行已澄清）或改注释 |
| D7 | ROADMAP.md:157 历史差距表 | 「图例（仅接口注释，未实现）· 主题/调色板（硬编码）」——Phase 1 起点历史记录，早已实现 | ④（历史存档） | **维持**（历史记录；如需可加"（历史起点，已实现）"注） |

## ⑤ 观察项核对（t43/t45/t47/t49 的 O1~O4 是否已落入文档）

| 观察项 | 内容 | 落入文档? |
|---|---|---|
| t43 O1（环境） | xcb 不可达 / wayland llvmpipe 基线 / demo EGL 软渲染 A9 降级 | ✓ design_phase3 §10.1/§2.2/§13.2（llvmpipe 冒烟口径 + A9） |
| t43 O2 | 程序池按上下文建池（6.4.x 无 setShareContext） | ✗ 未落入（→ B2/C2） |
| t43 O3 | pickTable 对齐依赖发射序；Q_ASSERT Release 失效 | ✓ 已落入 t53 deepdive_picking_idframe §3（对齐断言）；"Release 失效"细节未提（可选补） |
| t43 O4 | 未跟踪 bench_results.csv | ✗ 工程杂物 → 建议 .gitignore 或清理（非文档项） |
| t45 O1（环境） | 同 t43 O1 | ✓ |
| t45 O2 | u_baseId/u_vertPerPrim 链接器优化 | ✗ 未落入（→ C3） |
| t45 O3 | 程序池上下文地址复用理论风险 | ✗ 未落入（→ C4；t50 保持观察） |
| t45 O4 | releasePrograms 无 current 写法 | ✗ 未落入（→ C4） |
| t47 O1（环境） | 同 t43 O1 | ✓ |
| t47 O2 | 16ms 闸 vs llvmpipe 31-59ms 交互 | ✓ **已落入** t53 deepdive_picking_idframe §5（软渲染下闸为防御） |
| t47 O3 | 半像素对齐（线宽 1px） | ✗ 未落入（→ C5） |
| t47 O4 | 程序池风险（t45 O3/O4 遗留） | ✗ 未落入（→ C4） |
| t49 O1（环境/A8） | llvmpipe 仅冒烟不作基准 | ✓ design_phase3 §10.1/§10.2 |
| t49 O2 | pickTable 16MB 常驻（拾取必需） | ✓ **已落入** design_phase3 §7.1（"t50 观察项：按需/分片降载"） |
| t49 O3 | bench CSV 整体重写 | ✗ 工程行为（非缺陷）→ 维持，无需文档 |
| t49 O4 | GL 窗口闪关正常 + **offscreen demo 崩溃（Qt 6.4.2 平台缺陷）** | ✗ offscreen 崩溃未落入 design_phase3 §13 → 建议补记录或维持（环境已知项） |

## 一致性核查通过项（无差异，记录备查）

| 项 | 核查结果 |
|---|---|
| R6 无平移手势一致性 | design_3d（R6 注/D-3D-11/§8.3/§8.3 代码注释）、design_3d_axes:577、QChartWidget3D.h:118-123/.cpp:686-687 全部一致（"平移仅 API，无鼠标手势"）；**唯一例外 ROADMAP:236（见 D5）** |
| 统一后端原则（D26） | demo_scatter3d:57/demo_surface3d:126 `setRenderBackend(OpenGL)` 注释"QCHART_GL=0 由 Widget 兜底" ✓；bench_main backend 列（qpaint/gl）✓；design_phase3 §2.2 + t53 overview §5 一致 |
| Lattice 网格模式 | 头文件声明 + collectPrimitives else 分支实现（晶格三族 U/V/W）✓（"仅声明未实现"疑点排除） |
| GL overlay 标签 | GlHost::paintGL 已实现 QPainter overlay（QChartWidget3D.cpp:53-64，labels() 出参同源）；design_phase3 §6 声明已落实；仅 QOpenGLChartRenderer.cpp:100 注释过时（D4） |

## 建议汇总

- **改文档（7 处）**：B1（design_3d §11.1 viewDepth 措辞）、B2（design_phase3 §7.3 程序池按上下文）、B3（§7.1 预算分列两缓存+口径）、B4（module_core.md Q_OBJECT 三行）、D5（ROADMAP 232/236 R5/R6 表述）、C5 与 ⑤-O4（可选补句/补记录）。
- **改代码注释（4 处）**：D1（QChartProjection.h 删死行）、D3（QChartGL.h 头注释更新）、D4（QOpenGLChartRenderer.cpp:100 注释更新）、D6（demo_line3d 措辞，可选）。
- **维持+记录（5 项）**：A1、A2、C1、C3、C4（均为文档化/低风险/预留项；t50 观察项保持）。
- **工程建议（非文档）**：⑤-O4（bench_results.csv 加 .gitignore 或清理）。

> 本清单呈交队长转用户商讨；改文档/改代码均需另行派任务执行（本任务红线：只审计）。
