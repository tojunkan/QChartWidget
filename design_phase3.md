# design_phase3.md —— Phase 3 GPU 实时渲染 设计文档

> **读者**：engineer（照此实现）、reviewer（照此审查）、captain（排期）。
> **依据**：用户拍板问卷（design_phase3_questionnaire.md 10 题全按 ★，含 Q8 GPU 拾取、Q9 硬件基线修订）+ A1~A9 定案约束（t39 任务描述，逐条落实不得回退）+ Phase 2 基准结论（O(N) 投影瓶颈、44.5ms 基线、环境负载教训）+ Phase 2/补项完成态（viewCube 相机/图元列表/三层分离/ProjectFn3D 闭包、Layer 分层/billboard/深度偏置、QChartHitTester 已前置落地）。
> **范围**：QOpenGLChartRenderer + VBO 批次 + z-buffer + GPU 拾取（ID 帧）+ 数值预转换缓存落地；纯线框+深度先行，光照后置；「数学积木+shader 翻译器」记入 Phase 3.5。
> **红线遵守**：本文档为新增文件，不改任何代码/CMake/既有文档。

---

## 1. 定案约束确认（A1~A9 → 落点）

| # | 约束 | 落点 |
|---|---|---|
| A1 | OpenGL 3.3 Core（QOpenGLWidget+QOpenGLShaderProgram+VBO/VAO）；Qt 6.2+ 零升级；GLSL 330 字符串运行期编译 | §2/§4 |
| A2 | QChartWidget3D 内嵌 QOpenGLWidget（组合）；继承链/交互/联动/导出保留；多实例共享上下文；否决 FBO readback 上屏 | §2/§7 |
| A3 | 内存预算：简单图 RSS 增量 ≤25MB、1M 点 ≤60MB（验收 +20% 容差）；零纹理、零 QML/Quick；惰性初始化；VBO 静态上传+增量 SubData；深度缓冲按尺寸分配一次 | §7 |
| A4 | QOpenGLChartRenderer : QChartRenderer 消费 QChartScene；图元→VBO 批次；Grid/Series 开深度+网格偏置、ForegroundDecor 关深度后画；z-buffer 替换 painter's algorithm；标签/图例 QPainter overlay；2D/导出仍走 QPainter renderUncached | §3/§5/§6 |
| A5 | CPU 数值预转换缓存（D-3D-10 落地）→ World float3 VBO attribute → 顶点着色器仅 viewProj uniform（每帧零 CPU 投影）；固定投影 toWorld 本阶段 CPU（缓存），硬编码 GLSL 为 Phase 3.5 翻译器目标形态；翻译器 Phase 3.5 | §4/§9 |
| A6 | GPU 拾取：ID 编码 RGB24 → 光标 glReadPixels(1×1) → 解码查表 → QChartHitTester::HitResult；开 depth test=命中可见图元；仅鼠标移动触发；轴/网格 dataIndex=-1 不编码；交互层按后端分支，uvHovered/uvSelected 零改动 | §5/§8 |
| A7 | 纯线框+深度先行；光照（方向光 lambert）后置；屏幕近邻保留为 QPainter 后端实现；射线拾取 Phase 4 | §5/§13 |
| A8 | 硬件基线表（CPU/GPU/驱动/1920×1080/Release/Qt 版本/后端 WSLg 或桌面）；MSVC+MinGW 双工具链各测；性能口径（10 万点中位帧 ≤16.7ms+交互延迟 ≤50ms；1M 点 ≤50ms 无卡死；帧耗时=相机旋转 200 帧中位、排除首帧 shader 编译）；内存口径（A3 实测）；llvmpipe 仅 CI 冒烟；QChartBench 扩 GL 场景 CSV 落 build | §10/§11 |
| A9 | 后端可切换（GL 默认、QPainter 保底）；3D demo 切 GL 观感不变；旧 158 用例锁 QPainter 零回归；GL 新测试真实 GL 环境（offscreen 条件 skip） | §12/§13 |

---

## 2. 宿主与渲染器骨架（A1/A2）

### 2.1 QOpenGLChartRenderer API

```cpp
// QOpenGLChartRenderer.h —— GL 后端渲染器（A1/A4；: QChartRenderer 与 QPainter 后端并列）
// ⚠ 职责边界：GL 只做 3D 屏显；导出（PNG/SVG/PDF）与 2D 路径一律走 QPainterChartRenderer
//   （renderUncached），本类不参与（A4）。
class QOpenGLChartRenderer : public QChartRenderer {
public:
    explicit QOpenGLChartRenderer(QWidget* host = nullptr);  // 宿主 = 内嵌 QOpenGLWidget（A2）
    ~QOpenGLChartRenderer() override;

    // ===== QChartRenderer 接口（与 QPainter 后端并列）=====
    /// ⚠ device 必须是宿主 QOpenGLWidget（GL 渲染无通用 QPaintDevice 目标）；否则 qWarning 返回
    void render(const QChartScene& scene, QPaintDevice* device) override;
    /// ⚠ GL 不参与导出：qWarning 提示改用 QPainterChartRenderer（A4）
    void renderUncached(const QChartScene& scene, QPaintDevice* device) override;
    void invalidateBackground() override;    // 场景/数据脏标记（触发 VBO 重建，视图变化不触发）
    void invalidateForeground() override;    // 同（GL 无 QPixmap 缓存；数据/样式变化 → VBO 重建）
    void setCachingEnabled(bool) override;   // GL 无 QPixmap 缓存：no-op（保留接口一致）
    bool isCachingEnabled() const override;  // 恒 true（GL 无缓存语义）

    // ===== GL 生命周期（宿主 QOpenGLWidget 回调驱动；A3 惰性）=====
    /// 首帧/首 show：编译 3 个 shader 程序（§4）、建批次池；此后零分配
    void initializeGL();
    /// 主渲染入口（宿主 paintGL 调用；scene 由 QChartWidget3D::buildScreenScene 提供）
    void paintGL(const QChartScene& scene);
    /// 视口/深度随尺寸（宿主 resizeGL 调用；深度缓冲按尺寸分配一次，A3）
    void resizeGL(int w, int h);

    // ===== 拾取（A6；渲染执行在本类，解码在 QChartHitTester）=====
    /// 鼠标移动时：渲染 ID 帧（复用默认 FBO 与深度，§5.3）→ 光标 1×1 readback → 返回 RGB24
    QRgb pickIdAt(const QPoint& pos, const QChartScene& scene);

private:
    void buildBatches(const QChartScene& scene);   // 图元列表 → VBO 批次（数据/样式变化才重建，A5）
    void drawPass(const QChartScene& scene, ShaderKind kind);  // 主 pass / ID pass 共用批次遍历
    QWidget* m_host = nullptr;
    BatchPool m_batches;        // §3
    bool m_glReady = false;     // 惰性初始化标记（A3）
};
```

### 2.2 宿主：内嵌 QOpenGLWidget（A2）

```cpp
// QChartWidget3D 扩展（GL 后端；组合，继承链/交互/联动/导出保留）
public:
    enum class RenderBackend { OpenGL, QPainter };   // A9：GL 默认、QPainter 保底
    void setRenderBackend(RenderBackend b);
    RenderBackend renderBackend() const;
    /// ⚠ 统一后端原则（用户定案）：本开关同时决定渲染与拾取——CPU→渲染+拾取全 CPU、
    ///   GPU→渲染+拾取全 GPU，禁止混搭（§8.2）
    /// 环境变量 QCHART_GL=0 → 强制 QPainter（兜底，A9）
private:
    class GlHost : public QOpenGLWidget {           // 内嵌子部件，覆盖 plotArea
        void paintGL() override;                    // GL 主 pass + QPainter overlay（§6）
        void resizeGL(int, int) override;           // renderer->resizeGL
        // ⚠ 不重写 paintEvent（QOpenGLWidget 官方模式：GL 内容画在 paintGL；
        //   QPainter overlay 在 paintGL 内以 widget 为 QPaintDevice 直接画——
        //   QOpenGLWidget 支持该混合模式，合成由 Qt 处理）
    };
    std::unique_ptr<GlHost> m_glHost;
    QOpenGLChartRenderer* m_glRenderer = nullptr;   // GL 渲染器（GlHost 生命周期内）
```
- 事件路由：鼠标/滚轮事件由外层 QChartWidget3D 处理（现有 handler 不变），经 QChartHitTester 后端分支（§8）；GlHost 设 `setAttribute(Qt::WA_TransparentForMouseEvents)`（事件全部透传给外层）。
- 多实例共享上下文（A2）：`QChartGL::sharedContext()`（§7.3）——GlHost 构造时向共享组注册；shader/程序池按 share group 引用计数复用。

---

## 3. VBO 批次结构与图元→顶点映射（A4/A5）

### 3.1 批次结构

```cpp
// QOpenGLChartRenderer 内部
struct GLBatch {
    GLuint vao = 0;
    GLuint vbo = 0;                 // interleaved 顶点（§3.2）
    GLenum primitive = GL_POINTS;   // GL_POINTS / GL_LINES
    GLsizei vertexCount = 0;
    int baseId = 0;                 // 图元 ID 基址（id = baseId + primIndex，§5.3）
    QChartPrimitive::Layer layer;   // Grid/Series/ForegroundDecor（深度语义，§5.2）
    bool depthTest = true;          // ForegroundDecor = false（A4 恒可见）
    qreal depthBias = 0.0;          // Grid 批次 > 0（u_depthBias uniform，§5.2）
    GLuint ebo = 0;                 // 可选索引缓冲（曲面线框顶点去重，Phase 3 优化项）
};
struct BatchPool {                  // 批次池（数据变化重建，A5）
    QVector<GLBatch> batches;
    QVector<PickRecord> pickTable;  // 图元 ID → (series, dataIndex, layer)，§8.1
    bool dirty = true;              // invalidateForeground 置位
};
```

### 3.2 顶点布局（interleaved，16B/顶点；A5：World float3 + 颜色）

```cpp
// stride 16B：attribute0 = vec3 position（World，float）；attribute1 = vec4 color（UNSIGNED_BYTE normalized）
struct GLVertex { float x, y, z; uint8_t r, g, b, a; };   // 12B + 4B（对齐到 16B）
```

图元→顶点映射（图元列表→批次，**每批次一种 Layer + 一种 primitive**）：
| 图元 | 顶点数 | GL 原语 | 说明 |
|---|---|---|---|
| Point（系列散点/刻度点） | 1 | GL_POINTS | 点尺寸 = 批次 uniform u_pointSize（同 series 内 markerSize 一致） |
| LineSegment（折线/曲面线框/网格/盒边/spine） | 2 | GL_LINES | 每采样段 2 顶点 |
- 合并规则：同 Layer、同 primitive 的图元合并进同一批次（顶点连续追加）；批次顶点上限 64K 分片。
- 颜色：收集时按系列主题/override 展开的图元色（Phase 2 图元已有 color 字段）→ 顶点颜色。
- **拾取 ID 不占顶点内存**：`id = u_baseId + gl_VertexID / u_vertPerPrim`（Point: 1、Line: 2），顶点着色器算，零 attribute（A3 内存关键）。
- 静态上传：数据/样式变化（invalidateForeground）才重建批次并 glBufferData；**视图变化只更新 uniform**（u_viewProj 64B，A5）。
- ⚠ GL 路径缓存 Layer3D collectPrimitives 输出：首次 dirty 时收集一次（图元列表 + pickTable），此后复用；数据/样式/轴盒变化才重收集（避免每帧 CPU 收集——网格/轴批次是 World 静态几何，相机变化不触发重建，A5）。

---

## 4. Shader 程序清单（A1/A5，GLSL 330 字符串，运行期编译）

```cpp
enum class ShaderKind { Line, Point, Pick };   // 程序池键（QChartGL，§7.3）
```

| 程序 | 顶点属性 | Uniform | 输出 | 用途 |
|---|---|---|---|---|
| **lineShader** | position(vec3)、color(vec4) | u_viewProj(mat4)、u_depthBias(float)、u_baseId(int)、u_vertPerPrim(int) | vary: v_color、flat v_primId | 主 pass 线框（折线/网格/盒边/spine） |
| **pointShader** | position(vec3)、color(vec4) | u_viewProj、u_pointSize(float)、u_baseId、u_vertPerPrim | vary: v_color、flat v_primId、gl_PointSize | 主 pass 散点/刻度点（GL_PROGRAM_POINT_SIZE；片段圆化 discard） |
| **pickShader** | position(vec3) | u_viewProj、u_depthBias、u_baseId、u_vertPerPrim | flat v_primId（编码 RGB24） | ID pass（§5.3）；顶点数据复用主 pass 批次 |

- 顶点着色器核心（line/point 共用逻辑，A5：**仅 viewProj**——toWorld 已在 CPU 缓存完成，World 坐标直接进 attribute）：
```glsl
#version 330 core
layout(location=0) in vec3 a_pos;      // World
layout(location=1) in vec4 a_color;    // UNSIGNED_BYTE normalized
uniform mat4 u_viewProj;
uniform float u_depthBias;             // Grid 批次 > 0（NDC z 偏置，等价 kGridDepthBias 语义）
uniform int  u_baseId;
uniform int  u_vertPerPrim;
out vec4 v_color;
flat out int v_primId;
void main() {
    vec4 clip = u_viewProj * vec4(a_pos, 1.0);
    clip.z += u_depthBias * clip.w;    // 网格偏置：同深度处"更远"，系列优先（与 QPainter 路径一致）
    gl_Position = clip;
    v_color = a_color;
    v_primId = u_baseId + gl_VertexID / u_vertPerPrim;
}
```
- 片段（line）：`out vec4 = v_color`；片段（point）：圆化 `if (length(gl_PointCoord - 0.5) > 0.5) discard;`。
- 拾取（pick）：片段输出 `vec4(rgb24(id), 1.0)`。
- ⚠ 深度偏置等价性（A4）：QPainter 路径 `kGridDepthBias = 1e-3`（depth 减偏置）；GL 路径 `clip.z += u_depthBias*clip.w`（NDC 偏置）——两后端语义一致（同深度处系列优先），单测对照（§13.2）。

---

## 5. z-buffer 与分层绘制流程（A4/A6/A7）

### 5.1 主 pass（paintGL）

```
paintGL(scene)：
1. buildBatches（仅 dirty 时，§3.1）——数据/样式变化才重建（A5）
2. glViewport(plotArea)（视口=plotArea，A3 深度缓冲按 widget 尺寸）
3. 主 pass → 默认帧缓冲（QOpenGLWidget 自带 color+depth）：
   a. Grid 批次：depth test ON（u_depthBias>0）→ Series 批次：depth test ON（u_depthBias=0）
      ——z-buffer 保证正确遮挡（painter's algorithm 退休，A4）；批次间顺序只影响同深度 tie
   b. ForegroundDecor 批次：depth test OFF，最后画（盒边/spine/刻度点恒可见，A4/A7 语义保持）
4. Overlay：QPainter 画 billboard 标签 + 图例（§6）
5. 交换（QOpenGLWidget 自动）
```

### 5.2 深度语义（A4/A7，与 Phase 2 等价）

| 层 | GL 语义 | 等价 QPainter 路径（Phase 2） |
|---|---|---|
| Grid（网格/晶格） | depth test ON + u_depthBias>0（同深度系列优先） | depth 减 kGridDepthBias（1e-3）后统一降序 |
| Series | depth test ON（正确遮挡：前方遮挡后方） | depth 降序 painter's algorithm |
| ForegroundDecor（盒边/spine/刻度点/标签） | depth test OFF 后画（恒可见） | 前景层恒后画 |
- ⚠ 与 Phase 2 差异（有意为之，A7 定案延续）：轴/装饰穿入数据仍可见（3D 图表惯例）。

### 5.3 拾取：ID 帧（A6；★调和方案——零额外 FBO，A3 内存优先）

**实现细节偏离「主 pass 第二颜色输出」的说明**：MRT 双输出要求主 pass 渲染到离屏 FBO（color0+color1+depth，1920×1080 ≈ 25MB），与 A3 内存预算（简单图 ≤25MB）直接冲突。**调和定案：独立 ID 帧**——语义与 A6 完全一致（命中可见图元、仅鼠标移动触发、glReadPixels(1×1) O(1)），但**复用默认帧缓冲与深度，零额外 FBO 内存**：

```
pickIdAt(pos)（仅鼠标移动、位移 ≥1px、距上次拾取 ≥16ms（一帧预算限流）且 m_glReady 时触发）：
1. 渲染 ID 帧：pickShader 重放全部批次到默认帧缓冲——**自包含**（清 depth + 写 depth，
   与主 pass 同批次顺序/深度语义：Grid 偏置、Series 深度、Decor 关深度后画）
   ——不依赖主 pass 的 depth 状态，与 paint 时序完全解耦（快速晃动鼠标无陈旧深度问题）
2. glReadPixels(pos, 1×1) → RGB24 → 返回 QRgb
3. 解码（QChartHitTester::hitTestGPU，纯函数，§8.1）：id → pickTable → HitResult
```
- ID 编码：RGB24（2^24 ≈ 1677 万图元，足够）；`id = r | g<<8 | b<<16`。
- 轴/网格图元 dataIndex=-1 **不编码**（不加入 pickTable；ID pass 仍渲染它们以维持深度语义，但其片段 ID 写入保留字段——定案：dataIndex=-1 图元片段输出 ID=0xFFFFFF（哨兵），解码为未命中，A6）。
- 重建时机：批次重建（invalidateForeground）时同步重建 pickTable。
- ⚠ 限流与守卫（快速晃动鼠标，用户关切）：①Qt 事件循环合并连续 mouse move（只投递最新位置）——第一道闸；②拾取限流 ≥16ms + 位移 ≥1px——第二道闸（避免晃动时每事件一次全场景重放）；③`m_glReady==false`（惰性初始化前）跳过拾取——第三道闸；hover 信号延迟最多一个事件周期，标准可接受。

---

## 6. Overlay 合成（A4）

- billboard 标签（`QVector<QChartTextLabel>`，Phase 2 产物）+ 图例（scene.legend）：**QPainter 绘制**（A4 字体管线复用、零字形纹理、A3 零纹理约束）。
- 合成方式：GlHost::paintEvent = paintGL（GL 渲染）→ QPainter（overlay，QOpenGLWidget 合成层支持）→ 文本按 QChartTextLabel{screenPos, text, anchor, fontSize, color} drawText。
- 顺序：GL 3D 在最底、overlay 文本最上（与 Phase 2 前景层语义一致）。

---

## 7. 上下文与资源生命周期（A2/A3）

### 7.1 内存预算表（A3，分辨率变量——1920×1080 基准）

| 资源 | 预算（@1920×1080） | 核算 |
|---|---|---|
| 默认帧缓冲（QOpenGLWidget 自带 color+depth） | ≈14.5MB | 8.3MB color(RGBA8) + 6.2MB depth(24bit)；按 widget 尺寸，**在账内** |
| GL 上下文 + 程序池（3 程序） | ≤6MB | GLSL 330 字符串 ×3；零纹理 |
| 简单 3D 图 VBO（1 万散点+线+曲面 64×64+轴网格） | ≤1MB | ~1.2 万顶点 × 16B |
| 1M 点 VBO | ≤16MB | 1M × 16B；静态上传一次，增量 SubData（A3） |
| 1M 点 CPU 数值预转换缓存（World float3） | ≤12MB | 12B/点；**替代 QVariant 三元组存储**（§9，达成预算的必要条件） |
| 拾取 | ≈0 | ID 帧复用默认 FBO 与深度（§5.3），无独立 FBO |
| billboard 标签/图例 | ≈0 | QPainter 字体管线，无 glyph atlas |
| **合计（RSS 增量）** | 简单图 ≈26.5MB；1M 点 ≈66.5MB | 名义预算 ≤25/≤60MB，**验收口径 = 预算 +20% 容差 = ≤30/≤70MB（A3）** |

⚠ 名义 25MB 与 1080p 默认 FBO 固有成本（14.5MB）的差距由容差覆盖；若用户需严格 ≤25MB，可降 widget 分辨率或缩小视口（预算表按 7B/px 线性缩放）。

### 7.2 生命周期规则

- **惰性初始化（A3）**：首帧/首 show 的 initializeGL 才编译 shader、建批次池；无 GL 部件实例时零 GL 资源。
- **静态 VBO**：数据/样式变化 → 整批重建或 glBufferSubData 增量；视图变化只更新 uniform（A5）。
- **深度缓冲**：默认 FBO 自带（resize 重建）；无独立拾取 FBO（§5.3）。
- **释放**：GlHost 析构 → QOpenGLWidget context 析构（Qt 管理）→ VBO/VAO glDelete；程序池引用计数归零释放（§7.3）。

### 7.3 共享上下文与程序池（A2）

```cpp
// QChartGL.h —— GL 资源池（A2/A3：共享/惰性/引用计数）
class QChartGL {
public:
    /// 共享上下文（惰性）：首实例创建并设为共享根，后续 QOpenGLWidget share 之；
    /// 无 GL 部件实例时返回 nullptr（零资源，A3）
    static QOpenGLContext* sharedContext();
    /// 程序池：按 kind 缓存已编译程序（share group 内复用，引用计数随实例增减）
    static QOpenGLShaderProgram* program(ShaderKind kind);
    static void releasePrograms();   // 最后实例析构时
};
```
- GlHost 构造：`QSurfaceFormat` 统一（3.3 Core、depth 24、vsync 默认）；context 注册到共享组（A2 资源池化——多实例 shader/program/VAO 模板复用）。

---

## 8. 拾取：ID 表、解码与交互集成（A6，归属 QChartHitTester）

### 8.1 QChartHitTester 第三种实现（任务 0 预留落地）

```cpp
// QChartHitTester.h 追加
    /// GPU 拾取解码（纯函数，无 GL 依赖，可单测）：光标 1×1 读回 RGB24 → ID → 查表 → HitResult
    static HitResult hitTestGPU(uint8_t r, uint8_t g, uint8_t b,
                                const QVector<PickRecord>& pickTable);

// PickRecord：图元 ID → 命中结果（与批次同步构建）
struct PickRecord {
    QChartSeries* series = nullptr;   // 系列（series 层）；轴/网格不编码（dataIndex=-1）
    int dataIndex = -1;               // 数据索引（同 2D HitResult.dataIndex 语义）
    QChartPrimitive::Layer layer;     // 备用（调试/断言）
};
```
- 解码：`id = r | g<<8 | b<<16`；id == 0xFFFFFF（哨兵）或越界 → 返回空 HitResult。

### 8.2 交互层后端分支（uvHovered/uvSelected 零改动，A6）

```
QChartWidget3D::updateHover(pos)（现有，R5 语义不变；⚠ 后端统一原则：渲染与拾取必须同后端，禁止混搭）：
  if (renderBackend() == OpenGL) {
      QRgb id = m_glRenderer->pickIdAt(pos, scene);      // ID 帧（自包含，限流+守卫，§5.3）+ readback
      result = QChartHitTester::hitTestGPU(qRed(id), qGreen(id), qBlue(id), pickTable);
  } else {
      result = QChartHitTester::hitTest(pos, primitives, 8.0);   // QPainter 后端 CPU 近邻（A7 保留）
  }
  result 命中 → uvHovered(u,v)（series.at(dataIndex) → Data → toNumeric，现逻辑不变）
```
- **后端统一原则（用户定案）**：Phase 3 不再是「渲染后端」，而是**统一后端**——`setRenderBackend` 同时决定渲染与拾取：CPU（QPainter 渲染 + CPU 近邻拾取）、GPU（GL 渲染 + ID 帧拾取）；无任何「GL 渲染 + CPU 拾取」或反向的混搭路径。

---

## 9. 数值预转换缓存 API 落点（A5/D-3D-10 落地）

```cpp
// QChartSeries3D 扩展（Phase 3）
public:
    /// 数值预转换缓存（A5；append 数值型时构建，绕开每帧 QVariant 解包）：
    /// 数值型路径（append(qreal×3) 便捷重载）内部直接存 float3（权威存储，12B/点）；
    /// QVariant 路径（append(QVariant×3)）保持 QDataPoint3D 列表（非数值 Axis 回退，D-3D-10 确认点不变）
    const QVector<QVector3D>& numericCache() const;   // Numeric 三元组（数值型有效）
    bool numericCacheActive() const;                  // 全部数值型 append → true

    /// World 缓存（VBO 源；Layer3D 渲染时填充——series 零耦合红线：不持 Axis/Projection 引用）
    /// 数值型：worldCache = toWorld(numericCache)；现有 QChartSurfaceSeries::worldCache 模式扩展到基类
    const QVector<QVector3D>& worldCache() const;
    QVector<QVector3D>& worldCache();                 // Layer3D 填充入口（内部）

// QChartLayer3D（collectPrimitives 扩展）：数值型系列渲染时填充 worldCache（toWorld 逐点，一次性；
//   投影/数据变化才重建）→ VBO 上传 worldCache（A5：World float3 attribute）
```
- ⚠ **达成 A3 内存预算的必要条件**：1M 点 QVariant 三元组 ≈50~70MB（QVariant 开销），float3 权威存储 ≈12MB——预算表（≤60MB）依赖数值型大系列以 float3 为权威存储、QVariant 仅按需物化视图（points()/at()/count() API 语义不变）。此为 A3 与 D-3D-10 的交汇点，实现前置条件（消息中向队长/用户说明）。
- 失效策略：数据变化（append/insert/remove/replace/clear）→ numericCache 增量维护（append 追加）；Axis/投影变化（数值型 toNumeric 恒等时仅投影变化）→ worldCache 重建（Layer3D 置脏）；QVariant 路径无缓存（回退，Phase 2 边界）。

---

## 10. 硬件基线表与验收流程（A8，用户定案）

### 10.1 基线平台表（实现时填写实测值）

| 平台 | CPU | GPU | 驱动 | 分辨率 | 构建 | Qt | 后端 |
|---|---|---|---|---|---|---|---|
| Linux/WSLg | （记录型号） | （记录：直通 GPU 或 llvmpipe） | mesa | 1920×1080 | Release | 6.4.2 | WSLg |
| Windows | （记录型号） | （记录型号） | 厂商驱动 | 1920×1080 | Release | 6.11.x | 桌面 |

### 10.2 验收流程（性能 + 内存 + 双工具链）

1. 构建 Release + QChartBench GL 场景（§11）→ CSV 落 build。
2. **性能**：10 万点——相机旋转 200 帧**中位帧耗时 ≤16.7ms（≥55fps）**且交互延迟 ≤50ms；1M 点——中位帧 ≤50ms（≥20fps）无卡死。⚠ 帧耗时**排除首帧**（shader 编译/惰性初始化，A3/A8）。
3. **内存**：RSS 增量实测——简单图 ≤30MB、1M 点 ≤70MB（A3 预算 +20% 容差；Linux /proc/self/status VmRSS、Windows GetProcessMemoryInfo）。
4. **双工具链**：MSVC + MinGW 各测一遍（A8；Phase 2 教训：工具链/驱动行为差异必须两边验证）。
5. **llvmpipe 软渲染**：仅 CI 冒烟（能跑不崩），**不作性能基准**（无 GPU 意义，A8）。
6. **环境负载教训**（Phase 2 基准结论）：记录 env（kernel|platform|compiler，bench 已有）、WSLg vs 桌面分开比较、排除后台负载干扰。

---

## 11. QChartBench 扩展（A8）

- `Test/bench/bench_main.cpp` 追加 GL 场景（真实 GL 环境跑，offscreen 跳过）：
  - `gl_vbo_upload_1M`：1M 点 World float3 批次 glBufferData 耗时（ms）
  - `gl_rotate_100k` / `gl_rotate_1M`：相机旋转 200 帧中位帧耗时（排除首帧 shader 编译）
  - `gl_rss_simple` / `gl_rss_1M`：进程 RSS 增量（MB）
- CSV 列扩展：`bench=gl_*`、`backend=gl`；复用现有 env/toolchain 自动识别。

---

## 12. demo 更新计划（A9）

| demo | 更新 |
|---|---|
| scatter3d / line3d / surface3d | 默认切 GL 后端（`setRenderBackend(OpenGL)`；环境变量 QCHART_GL=0 回退 QPainter）；交互（orbit/dolly/pan）、双 Widget 联动（uvHovered/uvSelected）、轴/网格观感**不变**；'A' 键不变 |
| 2D demo（8 个） | 零改动（GL 只替换 3D 屏显路径） |
| 导出 | 3D demo 的 saveAs* 仍走 QPainterChartRenderer renderUncached（GL 不参与导出，A4） |

---

## 13. 回归策略与测试（A9）

### 13.1 回归保障（硬指标）

- **旧 158 用例锁 QPainter 路径零回归**：测试直接构造 scene + QPainterChartRenderer（不经 GL），GL 默认开启不影响；QChartHitTester 抽取已前置且纯重构（任务 0 验收通过）。
- 2D 路径与导出（PNG/SVG/PDF）零改动（A4）。
- 后端开关：编译（BUILD_GL option）+ 运行（QCHART_GL 环境变量 / setRenderBackend）双保险（A9）。

### 13.2 GL 新测试（真实 GL 环境；offscreen 平台 QSKIP）

| 测试类 | 用例 |
|---|---|
| test_qchartmath 扩展 | 数值预转换缓存：数值型 append → numericCache 12B/点、QVariant 路径不激活；worldCache 填充（toWorld 对照） |
| test_qopenglrenderer（新） | 批次结构（图元→顶点数/合并规则/ID baseId 分配）；深度语义（Grid 偏置 uniform 生效、decor 关深度后画——像素/readback 断言）；**深度偏置等价性**（同一场景 GL 与 QPainter 后端输出一致：同深度系列优先）；ID 帧拾取（pickIdAt 命中可见图元、dataIndex=-1 哨兵、1×1 解码）；**ID 帧自包含**（无主 pass/首帧前/连续拾取之间深度正确——清深度+写深度，与 paint 时序解耦）；**拾取限流**（快速连续 move：位移 <1px 跳过、<16ms 间隔跳过、m_glReady 前跳过） |
| test_qcharthittester 扩展 | hitTestGPU 解码纯函数（RGB24→ID→表→HitResult、哨兵/越界）；与 CPU 近邻在简单场景结果一致（两后端交叉验证） |
| 运行环境 | 真实 GL（WSLg 桌面/带 GL 的 CI）；`QGuiApplication::platformName()=="offscreen"` → QSKIP（A8 llvmpipe 冒烟除外） |

---

## 14. 实施顺序（依赖链；QChartHitTester 已前置）

1. **数值预转换缓存 + 存储策略**（§9：numericCache/worldCache 扩展、float3 权威存储）→ 单测（数据正确性/内存核算——A3 预算的前提）。
2. **QChartGL 资源池 + 渲染器骨架**（§7.3/§2.1：共享上下文、程序池、惰性 initializeGL、批次池 buildBatches）。
3. **Shader 三程序 + 主 pass**（§4/§5.1：line/point/pick、顶点布局、Grid 偏置、decor 关深度）→ GL 新测试批次/深度部分。
4. **拾取**（§5.3/§8：ID 帧、pickTable、hitTestGPU 解码、QChartWidget3D 后端分支、uvHovered 零改动）。
5. **Overlay 合成**（§6：QPainter 标签/图例）。
6. **后端开关 + demo 切换**（§2.2/§12：setRenderBackend/QCHART_GL、3 demo 切 GL）。
7. **QChartBench 扩展 + 验收**（§10/§11：硬件基线表实测、双工具链、内存/性能口径）。
8. **终验**：Linux 构建 0 error/0 warning + ctest 全量（旧 158 + GL 新）+ 11 demo 冒烟 + 性能/内存达标；reviewer 逐任务独立审查（实跑）。

> 每步完成后立即交付 reviewer（D9 逐任务审查），审查通过再进下一步。

---

## 15. 风险与遗留

### 15.1 风险

| 风险 | 对策 |
|---|---|
| 内存名义预算（≤25MB）与 1080p 默认 FBO 固有成本（≈14.5MB）的差距 | 验收口径 = 预算 +20% 容差（≤30MB，A3 已定）；预算表按 7B/px 线性缩放；若用户需严格收紧，降分辨率或缩小视口 |
| 1M 点内存超标（QVariant 存储 ~60MB） | §9 float3 权威存储（12MB）为**达成预算的必要条件**，实施顺序第 1 步先立（消息中向队长/用户说明） |
| WSLg/驱动差异（llvmpipe vs 直通 GPU、MinGW/MSVC） | A8 基线表记录环境；llvmpipe 仅冒烟；双工具链各测；帧耗时排除首帧 shader 编译 |
| ID 帧拾取与主 pass 深度一致性（decor 关深度可能盖住 series） | 拾取 pass 与主 pass 同一批次顺序/深度语义；单测交叉验证（两后端结果一致） |
| QPainter overlay 与 GL 合成性能（QOpenGLWidget 合成层） | 标签/图例数量少（tickCount 2~3），可接受；若超标 → 标签纹理化（Phase 3.5 评估，违背零纹理约束则否决） |

### 15.2 遗留（明确后置）

- 光照（方向光 lambert）→ Phase 3 子项或 Phase 4（A7）；射线拾取 → Phase 4（unproject 已预留）；「数学积木 + shader 翻译器」→ **Phase 3.5**（A5 范围边界，用户构想）；曲面线框顶点去重/索引缓冲 → 优化项；标签纹理化 → 仅当 overlay 性能不达标时评估（零纹理约束优先）。
