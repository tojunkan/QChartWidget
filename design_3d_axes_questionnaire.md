# 《3D 轴/网格控制流》设计问卷（Phase 2 补项）

> 背景：Phase 2 按 Q14/D-3D-13 排除了 3D 轴刻度/网格，用户实测后判定「只有数据计算流、没有控制流（三维轴/网格/刻度/标签）是严重缺口」，升级为必补项。
> 现状锚点：2D Axis 职责 = 刻度生成（tickValues）+ 标签格式化（tickLabels）+ 绘制（drawAtEdge/drawAtPosition）；QChartLayer3D 现有 gridFloor（y=0 平面世界线段，与系列混排深度降序）；渲染 3D 路径 = collect → depth 降序 → 绘制 → 2D overlay 后画；worldBounds 已在 QChartScene 随 fit 更新。
> 每问：候选选项 + ★推荐项（置首，一句话理由）。共 10 题，可快速拍板。

## Q1. 3D 轴形态（脊柱怎么摆）？
- ★A. matplotlib 式盒状边框脊柱：X/Y/Z 三 spine 沿数据包围盒（worldBounds）低端三边汇聚成盒角，各轴可单独隐藏。理由：3D 图表最熟悉的参照系，随包围盒 fit 自动贴合数据，远离原点的数据（如 u∈[100,200]）也不会悬空。
- B. 过原点脊柱：三轴交于 (0,0,0)（2D 数据主脊风格）。理由备选：数据总在原点附近时简洁；但数据域偏离原点时脊柱与数据脱节，观感差。
- C. 可配置模式（盒/原点/自定义位置枚举）：灵活但 Phase 2 过度设计，放 Phase 3。

## Q2. 网格面：画几个面、现有 gridFloor 去留？
- ★A. 地板升级为 tick 对齐网格（与 spine 刻度位置一致，视觉锚定）：保留 gridFloorVisible 开关语义，半宽改由 worldBounds 派生（可手动覆盖）；XY/YZ 面留开关、默认关。理由：一面对齐刻度的网格即够方向感（matplotlib 默认同思路），信息密度可控；现有 gridFloor 并入新机制，不留两套。
- B. 三坐标面（XZ/XY/YZ）tick 网格全画：立体感强但线条量 ×3、易杂乱。
- C. 保持现状 gridFloor（仅地板、不与刻度对齐），另加独立 tick 网格：两套机制并存，不推荐。

## Q3. 刻度 tick 标记怎么画（Axis 刻度生成不变，只定 3D 编排）？
- ★A. 世界空间短线：tick 位置 = Numeric→toWorld→投影成世界单位短线（长度≈包围盒对角线 2%，可配），生成图元、渲染层次按 Q5 定案。理由：与 spine/网格同一几何管线、近大远小自然，tick 随数据/相机联动。
- B. 屏幕空间固定像素短线：投影 tick 位置后在屏幕上画定长短线，不入几何管线。理由备选：始终清晰；但与管线割裂、观感"贴片"。
- C. 混合：短线屏幕空间、spine 走图元管线：两套路径，实现与测试翻倍。

## Q4. 刻度标签（含轴标题）怎么画？
- ★A. billboard 屏幕空间文本：投影 tick 位置 → drawText 固定像素字号（恒面向相机、永不形变），沿 spine 方向偏移；颜色/字号走主题；标签内容复用 tickLabels；轴标题（u/v/w 名称）billboard 画在 spine 端点，可开关。理由：QPainter 无真 3D 文本管线，billboard 是唯一务实解且与 2D 标签观感一致；Phase 3 GL 仍用 billboard（标准做法）。
- B. 世界空间文本（随 3D 旋转/缩放）：QPainter 做不到真 3D 文本，只能伪近似，伪需求。
- C. 不画标签（只画 tick 线）：信息不足，无法读数。

## Q5. 深度语义：轴/网格参与 painter's algorithm 还是参考线？（核心张力 b）
- ★A. 参考线语义分层：网格线 = 背景层（先画，系列盖在网格上）；spine/刻度/标签 = 前景装饰层（后画，恒盖在系列上）；两者都不与系列做深度比较。理由：轴/网格是"参照系"不是"场景物体"，与数据混排会出现轴被数据遮断、刻度残缺的观感（matplotlib 3D 同样不做轴深度裁剪）；分层兼顾"网格不干扰数据"与"轴恒可读"。代价：轴穿入曲面内部仍可见——3D 图表惯例接受的近似。
- B. 全部作为 QChartPrimitive 参与深度降序混排：实现最省（并入现有管线），但轴/刻度常被系列遮挡，参照系失效。
- C. 全部恒后画在同一层：网格会盖住数据，干扰读数，不可取。

## Q6. 编排归属：谁负责把 Axis 的刻度变成 3D 几何？（核心张力 a）
- ★A. 轻量编排器 QChartAxis3D（非 Q_OBJECT，组合持有现有 QChartAxis*）：复用 tickValues/tickLabels/样式/主题色（setColor/setTickCount 等经原 Axis 对象配置），新增 3D 专属配置（spine 位置枚举、tick 长度、标签偏移）；QChartLayer3D 持有三个（或 QChartAxes3D 集合）。理由：Axis「刻度生成+标签格式化」纯函数原地复用、绘制编排集中可测；非 Q_OBJECT → moc/CMake 红线零扰动；组合优于继承（drawAtEdge/drawAtPosition 的 2D 绘制语义与 3D 根本不兼容）。
- B. 新 Q_OBJECT 类族 QChartAxis3D : QChartAxis：继承会带进 2D 绘制接口（drawAtEdge/drawAtPosition 语义误导）+ 新增 Q_OBJECT/moc 面，得不偿失。
- C. Widget3D 直管：编排逻辑塞进 widget3D，可测性差、样式配置 API 爆炸。

## Q7. 与 2D 联动场景协调（左 3D 球面 + 右 2D (u,v) 平面）？
- ★A. 3D 侧加 spine/刻度/标签（空间参照：方位感），右 2D 平面保留完整 2D 轴（精确读数区）——两处刻度**不冗余**、分工明确；demo 里 3D 轴默认开、按键 'A' 可关；3D 标签数量用 tickCount 压到 2~3 个避免拥挤。理由：3D 参照系与 2D 读数区是不同用途；联动语义（(u,v) 数值一致）不受影响。
- B. 3D 侧只画 spine+网格、不画标签（读数全靠右 2D 平面）：省事但左图缺方位信息。
- C. 3D 侧完全复制 2D 的满刻度：两侧视觉重复拥挤。

## Q8. 范围与排期：补项何时做、demo 是否同步？
- ★A. Phase 2 补项立即实现（拍板后走 设计→实现→审查 流程），范围 = spine + tick 网格 + 刻度 + billboard 标签 + 轴标题（按 Q1~Q6 定案）；demo_surface3d/line3d 同步加轴/网格，scatter3d 可加可不加。理由：用户已判定严重缺口，拖到 Phase 3 推迟可见价值；实现量约 1~2 个 engineer 任务，可控。
- B. 最小集先行：spine + 地板 tick 网格 + 刻度线（无标签），标签/轴标题并入 Phase 3：先解"没有参照系"的燃眉之急，但用户可能再次反馈"无标签"。
- C. 并入 Phase 3 统一做：与 GPU/文本管线一起设计更彻底，但 Phase 2 验收后 3D 无轴期过长。

## Q9. 测试策略？
- ★A. 新增 TestQChartAxis3D（编排器单测：spine 图元 3 条、tick 图元数 = 各轴 tickValues 数、tick 世界位置 = toWorld(tick) 手算对照、标签 billboard 偏移坐标）+ renderer3d 扩展 3 例像素断言（网格在系列下、spine/标签在系列上、轴隐藏开关生效）；Axis 复用零回归 = 旧 69 例不动 + 断言 tickValues/tickLabels 输出不变。理由：编排器纯函数可单测、像素断言锁渲染层次（Q5 定案）、Axis 复用回归锁死 2D 职责不变。
- B. 只加渲染像素断言、不单测编排器：像素失败难定位到 spine/tick/标签哪一环。
- C. 只单测编排器、不加像素断言：渲染层次（Q5）无锁，回归看不见。

## Q10. 交互关联：hover 命中与相机 fit？
- ★A. hover 不命中轴/网格（dataIndex=-1 图元直接排除，参考线语义）；spine/tick 网格随 worldBounds（fitWorld/投影切换时重算，读取 QChartScene.worldBounds）。理由：hover 语义是"数据点"，轴命中无意义（matplotlib 同样不响应）；参照系贴合数据、范围变化不悬空（demo 按键切球面/莫比乌斯即验证）。
- B. 轴/网格参与 hover：命中返回轴名——新语义无消费方，白做。
- C. spine 固定世界坐标不随包围盒：数据范围变化后脊柱悬空，参照系失效。

---

> 备注：Q1/Q5/Q6/Q10 分别对应队长提示的三个核心张力——a) Axis 刻度/标签复用、绘制编排归 3D 编排器（Q6）；b) 深度排序 vs 参考线语义分层（Q5）；c) 脊柱随包围盒 fit 驱动（Q1/Q10）。全部题目均基于 design_3d.md §2/§5/§7/§8/§10 与当前实现（QChartLayer3D gridFloor、渲染降序管线、demo_surface3d 联动）出题，选项具体到可拍板。
