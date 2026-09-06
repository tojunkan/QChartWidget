# QOpenGLChartRenderer Documentation

## Brief Introduction:
QOpenGLChartRenderer 是 **GL 后端渲染器**（继承 QChartRenderer）：变换在 Shader 内完成（`transformNumericToCartesian` CPU 空操作），裁剪退化（`cullAndResolveLabels` 全可见 + 仅解析绑定标签）。绘制前要求**当前有效 OpenGL 上下文**（外部保证；本类不管理上下文生命周期）。流程：`render → drawPrimitives`：F2 修复后 **glEnable(GL_PROGRAM_POINT_SIZE)**（Core Profile 下顶点着色器 gl_PointSize 才生效）→ `buildBatches`（clearBatches + uploadBatches）→ 依次 `drawPass(Triangle/Line/Point)` → glDisable(GL_PROGRAM_POINT_SIZE)。批次按 (ShaderKind, markerSize, depth 推断 layer) 分组；每批上传 VBO（GLVertex 16 字节：xyz float + rgba ubyte）并 VAO 化；`drawPass` 从 `QChartGL::program(kind, projection)` 取动态 Shader，上传 `u_viewProj`（camera->viewProjectionMatrix）等 uniform 后 glDrawArrays。标签走 QPainter 覆盖层（device 上）经共享 `drawLabel`。同头文件定义辅助结构 GLVertex/GLBatch。析构时若批次未清空告警（须在上下文仍 current 时 `clearBatches()`）。

## Constant Variables:
None.（同头文件辅助结构，非类成员：`GLVertex{x,y,z:float; r,g,b,a:uint8_t}`（static_assert 16 字节）；`GLBatch{vao; vbo; primitive:GLenum; vertexCount; baseId; shaderKind:ShaderKind; pointSize; depthTest; depthBias}`）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVector<GLBatch>` | `m_batches` | （private）当前 VBO 批次列表（一次 drawPrimitives 内构建/消费；上下文失效前须 clearBatches） | `QVector<GLBatch>` | 空 | `QChartGL` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QOpenGLChartRenderer` | 构造（default explicit） | 无 | public | — | TestAxisMatrixGl（每次组合新建） | — |
| — | `~QOpenGLChartRenderer` | 析构：m_batches 非空 → qWarning（GPU 资源泄漏提示；须先 clearBatches） | 无 | public | — | — | — |
| `void` | `transformNumericToCartesian` | 覆写：**空操作**（GPU 变换在顶点 Shader：`vec3 num=a_pos; vec3 cart=<glslToCartesian 注入>; u_viewProj × cart`） | `QChartScene& scene` | protected | — | `QChartRenderer::render` | `QChartAbstractProjection` |
| `void` | `cullAndResolveLabels` | 覆写：visibilityCache 全 true（粗裁）；仅绑定标签解析——锚=`proj->toCartesian(prim.numA)` 且恒可见；**自由标签（refPrimitiveId=-1）恒不可见**（GPU 不支持） | `QChartScene& scene` | protected | — | `QChartRenderer::render` | — |
| `void` | `drawPrimitives` | 覆写：无 current GL 上下文 → qWarning 返回；`glEnable(GL_PROGRAM_POINT_SIZE)`（F2）→ buildBatches → drawPass×3（Triangle→Line→Point）→ `glDisable(GL_PROGRAM_POINT_SIZE)` | `QChartScene& scene, QPaintDevice* device, const QVector<bool>& visibility` | protected | — | `QChartRenderer::render` | — |
| `void` | `drawLabels` | 覆写：QPainter 标签覆盖层（抗锯齿 + clipRect）；`camera->project(cartesianAnchor)` 出 plotArea 跳过；共享 `QChartRenderer::drawLabel` 排版（R8：替代旧零尺寸 rect drawText） | `QChartScene& scene, QPaintDevice* device` | protected | — | `QChartRenderer::render` | — |
| `void` | `clearBatches` | 释放本批 VAO/VBO 并清空；无 current 上下文 → qWarning + 仅清列表（资源泄漏路径） | 无 | public | — | `buildBatches` 内部、TestAxisMatrixGl（render 后上下文仍 current 时） | — |
| `void` | `buildBatches` | （private）clearBatches + uploadBatches | `const QChartScene& scene` | private | — | `drawPrimitives` 内部 | — |
| `void` | `uploadBatches` | （private）图元 → 批次：按 (ShaderKind, pointSize=markerSize, isGrid=depth>0.5, isDecor=depth>1.0) 分组；逐类型打包顶点——Point×1 / Line×2 / Rect×6（双三角）/ Ellipse×48（16 扇区×3）/ Polygon·Mesh·Fan·Strip 逐 numVerts / Path 逐段×2（GL_LINES）；生成 VAO/VBO（attr0=xyz float、attr1=rgba ubyte normalized）；batch.depthTest=!isDecor、depthBias=isGrid?0.001:0、baseId 按图元数累计 | `const QChartScene& scene` | private | — | `buildBatches` 内部 | — |
| `void` | `drawPass` | （private）单类绘制：`QChartGL::program(kind, scene.projection)` 取程序；`camera->viewProjectionMatrix(plotArea 宽高比)`；bind + setUniformValue u_viewProj/u_blendAlpha（scene.projection 为 QInterpolatedProjection 时取 blend()）/u_depthBias/u_baseId；Point 另传 u_pointSize/u_vertPerPrim=1、Line=2、Triangle=3；逐同 kind 批次按 depthTest 开关 GL_DEPTH_TEST → glBindVertexArray + glDrawArrays | `const QChartScene& scene, ShaderKind kind` | private | — | `drawPrimitives` 内部 | `QChartGL` <br> `QChartCamera` |
| `QOpenGLFunctions_3_3_Core*` | `glFuncs` | （private）取 current context 的 3.3 Core 函数表（无上下文 → nullptr） | 无 | private | 指针/`nullptr` | 各 GL 调用内部 | — |

Notes:
- S0 实测（TestAxisMatrixGl，wayland/Mesa llvmpipe 4.5 Core）：8/8 组合全绿，含 F2 刻度点邻域断言（GL Point 真出墨）；offscreen 下整类 QSKIP。
- GLVertex 坐标即 Numeric 坐标（变换在 Shader）；`depth` 字段被 GL 端借用作 layer 推断（>0.5 Grid / >1.0 Decor），与 CPU 端 3D 排序语义并存。
- 本类不含上下文管理：宿主（QOpenGLWidget/未来 Widget 阶段）负责 makeCurrent/doneCurrent 与 registerHost 生命周期。

## Overrided Qt Events:
无（继承 QChartRenderer，非 QWidget，无事件覆写）。

## Signals:
None.（非 QObject，无信号）
