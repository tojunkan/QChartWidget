# 《Phase 2 3D 数学先行》设计问卷（v1）

> 范围：ROADMAP §4 Phase 2 —— QVector3D/QMatrix4x4、相机升 3D、3D 系列（散点/参数曲线/曲面线框）、QPainter 线框 + painter's algorithm 验证透视与相机数学；不动 GPU。
> 验收（D6）：Linux 构建绿 + ctest 全绿（旧 69 例零回归）+ 8 demo 不回归；旋转透视正确、无万向锁、参数曲面 demo 可交互旋转。
> 每问：候选选项 + 推荐项（★ 放首位，一句话理由）。共 14 题，可快速拍板。

## 一、3D 数学库形态（Q1–Q2）

**Q1. 3D 数学类型：直接用 Qt 还是包一层？**
- ★A. 直接用 Qt 的 QVector3D/QMatrix4x4（属 QtGui，库已链接、无新依赖），仅新增轻量 `QChartMath.h`（纯 inline 工具：视口变换、frustum 参数、深度辅助）。理由：Qt 数学类型成熟且 Q_PROPERTY/动画插值生态现成，包一层只增维护成本。
- B. 包一层 QChartVec3/QChartMat4（D11 前缀）：公共 API 自控，但重复造轮子，插值/序列化全要自己补。
- C. typedef 别名 + QChartMath 命名空间：收益≈A、成本高于 A。

**Q2. 2D 链路（QPointF/QRectF）如何过渡到 3D？**
- ★A. 双链路并行：现有 2D 五空间链路原样保留，3D 新增独立链路（QVector3D + World→Camera→Clip→NDC→Screen）；一致性用单测锁「3D 正交俯视相机 ≡ 2D cartesianToPixel」。理由：零回归（D6），不触碰现有 22 个 .cpp 与 69 用例。
- B. 2D 整体迁移到 QVector3D/矩阵（z=0 正交）：理论最统一，但全库重写、回归风险大。
- C. 2D 数据槽改存 QVariant 包 QVector3D：保持基类签名，但丢类型安全。

## 二、相机升 3D（Q3–Q5）

**Q3. QChartCamera 如何承接 3D（D1：2D 是 3D 退化特例）？**
- ★A. 扩展现有类：加 position/lookAt 目标/up/FOV/near/far + 投影模式（Perspective/Orthographic）+ viewProjectionMatrix()；viewRect/center/zoom/fit 保留为「正交模式下的 2D 视图」，透视模式下由目标点+距离派生（zoom 语义≈距离）。理由：D1 的唯一真解，现有 9 例相机测试语义不变。
- B. QChartCamera3D 继承 QChartCamera：2D 方法在 3D 下语义不兼容，继承易踩坑。
- C. 平级新类、Widget 二选一持有：两套状态两套逻辑，与 D1 相悖。

**Q4. orbit/pan/dolly 交互归属？**
- ★A. Widget 事件层（延续现状 mouse/wheel handler）识别手势 → 调 Camera 几何运算（orbit(deltaYaw,deltaPitch)/dolly(factor)/panTarget(dx,dy)），Camera 只做几何不碰事件。理由：与现状「Widget 交互、Camera 几何」分工一致，动画/单测可直接驱动 Camera。
- B. Camera 自带交互（接收 QMouseEvent）：Camera 变重、难单测。
- C. 独立 QChartInteraction 类：Phase 2 规模下过度设计。

**Q5. Q_PROPERTY 如何支撑动画（D3：优先 QPropertyAnimation）？**
- ★A. position（QVector3D）、lookAt 目标（QVector3D）、FOV（qreal）三个加 Q_PROPERTY+NOTIFY→viewChanged；up/near/far 普通 setter。理由：动画只动位置/朝向/视野三样；QVector3D 若无内建插值器，一行 qRegisterAnimationInterpolator 补齐。
- B. 六个属性全加：near/far/up 动画化无场景，属性冗余。
- C. 不加 Q_PROPERTY，相机动画全走自定义 QChartAnimation：丢 QPropertyAnimation 生态，与 D3 相悖。

## 三、3D 系列 API（Q6–Q8）

**Q6. 3D 系列类形态与命名（D11）？**
- ★A. 新增 QChartSeries3D : QChartSeries 基类（QVector3D 数据 + append/at/count），下分 QChartScatterSeries3D / QChartCurveSeries3D（参数曲线）/ QChartSurfaceSeries（曲面）；2D QXYSeries 不动。理由：继承基类白捡 name/visible/opacity/color/主题色/图例，命名呼应现有 QChartScatterSeries。
- B. 三个平级类各自继承 QChartSeries：数据管理代码三份重复。
- C. 复用 QXYSeries 加 3D 模式开关：QDataPoint 是二维语义，破坏「只存数据」。

**Q7. 3D 数据组织？**
- ★A. 散点/曲线：QVector<QVector3D>；曲面：行主序连续 QVector<QVector3D> + rows×cols。理由：连续内存，Phase 3 可直接批量喂 VBO/批量投影。
- B. 曲面用 QVector<QVector<QVector3D>>（行数组）：直观但碎片化、批量投影要逐行遍历。
- C. 统一扁平 QVector<float> 交错 xyz：最快但 API 不友好、类型安全差。

**Q8. 「Series 只存数据+注入变换闭包」如何延续（全库最值得保留的设计）？**
- ★A. 同构延续：QChartSeries3D::draw(painter, std::function<QPointF(QVector3D)> projectFn)，projectFn（World→Pixel）由 Renderer/Widget 组装注入，Series 零耦合于相机/矩阵。理由：与 2D toPixel 完全同构，解耦红线原样保住。
- B. 注入 viewProjectionMatrix() 让 Series 自己投影：Series 被迫懂矩阵与视口变换，耦合升高。
- C. Series 直接持 camera 指针：反向依赖，破坏零耦合。

## 四、painter's algorithm 与线框绘制（Q9–Q10）

**Q9. 深度排序归属？**
- ★A. Renderer（QPainterChartRenderer 的 3D 子路径）：展开 3D 系列为「投影图元列表」（点/线段 + 深度），统一排序后绘制。理由：跨系列遮挡需全局视野，Series 各自排序解决不了系列间遮挡；Renderer 本就是绘制编排者。
- B. 各 Series 内部排序：只能保证系列内正确，系列间仍错。
- C. Layer 层排序：Layer 只组装变换闭包，不适合做全局深度管理。

**Q10. 排序粒度 + 2D/3D 渲染路径如何共存？**
- ★A. 图元级排序：图元=点/线段（曲面线框拆线段），按深度从远到近绘制；QChartScene 增 3D 段（3D 相机 + 3D 系列），Renderer 检测到 3D 内容走 3D 子路径，2D overlay（图例/标注）后画、不参与深度。理由：Phase 2 图元规模排序开销可忽略、粒度足够正确；图元列表结构 Phase 3 GL 可直接复用。
- B. 多边形级（曲面按面片排序）：更精确但线框阶段过度设计。
- C. 系列级（按质心深度）：最快但系列穿插时错误明显。

## 五、demo 形态（Q11）

**Q11. 参数曲面交互 demo 怎么落？**
- ★A. 扩展现有入口：Test/demos/ 新增 demo_surface3d.cpp，test.cpp argv 增加 "surface3d"（无参=全部自动含）；交互=左键拖拽 orbit + 滚轮 dolly + 右键拖拽 pan。理由：零新入口成本，8 个 demo 冒烟框架与验收流程白捡。
- B. 新增独立可执行 target：动 CMake 目标结构，触碰红线且重复框架。
- C. 复用现有 demo_camera 改名扩展：职责混乱。

## 六、测试策略（Q12）

**Q12. 新增单测范围与旧 69 例零回归保障？**
- ★A. 新增 4 个测试类：test_qchartmath（矩阵/向量、透视矩阵性质）、test_qchartcamera3d（lookAt 正交性、投影矩阵、orbit/dolly 几何、正交俯视≡2D 一致性）、test_qchartrenderer3d（World→Screen 往返、深度排序近者后画）、test_qchartsurface3d（数据组织、变换闭包注入）；旧 69 例一行不改，同一 QChartTests + ctest 全量跑。理由：与 Phase 0/1 粒度一致，旧用例即回归锁；2D 代码路径「只增不改」是零回归硬保证。
- B. 只加 2 个类（数学+相机）：深度排序与系列无覆盖，验收项悬空。
- C. 加渲染器像素级断言（QImage 对拍）：脆弱易碎，收益低。

## 七、Phase 3 GPU 预留（Q13）

**Q13. 预留哪些接口避免 Phase 2 堵死 Phase 3？**
- ★A. ①相机直接产出 viewProjectionMatrix()（World→Clip 合并），Clip→NDC→Screen 显式拆成纯函数（可测可复用）；②渲染层预留「批量投影入口」：project(QVector<QVector3D>, matrix) → 屏幕点数组 + 深度数组；③Q9 的「投影图元列表」即 Phase 3 命令缓冲雏形，深度数组可演进为 z-buffer 降级路径。理由：矩阵流水线与原语列表是 GL 后端唯一需要的两样东西，现在定好 Phase 3 零返工（基准已证瓶颈在投影：O(N) 无条件 toPixel）。
- B. 只做相机矩阵，批量/原语接口 Phase 3 再设计：届时 Renderer 重构面大。
- C. 现在就上完整场景图/命令缓冲抽象：超出 Phase 2 范围，过度设计。

## 八、范围边界（Q14）

**Q14. 3D 轴/网格/主题/图例/拾取在 Phase 2 的边界？**
- ★A. Phase 2 只做：数学库 + 3D 相机 + 3 系列 + painter 线框 + 参数曲面 demo + 可选辅助网格地板（World 空间线段，走同一 3D 路径，给 demo 方向感）；不做 3D 轴刻度、射线拾取/hover、光照；3D 系列自动继承 2D 基类的 color/opacity/visible/主题色/图例。理由：与 ROADMAP §4 Phase 2 范围逐字一致，防蔓延。
- B. 把 3D 轴（三轴 spine+刻度）纳入 Phase 2：独立大块，显著拉长周期。
- C. 把射线拾取/hover 纳入 Phase 2：需新数学（射线求交），非本阶段目标。
