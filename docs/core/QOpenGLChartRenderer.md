# QOpenGLChartRenderer Documentation

## Brief Introduction:
QOpenGLChartRenderer 是 **GL 后端渲染器**（继承 QChartRenderer；v2 修订：批次 A/t8-t14 HiDPI 与标签合成修复、3D 通路说明）。变换在 Shader 内完成（`transformNumericToCartesian` CPU 空操作）；裁剪退化（`cullAndResolveLabels` 全可见 + 仅解析绑定标签）。绘制要求**当前有效 GL 上下文**。流程（render → drawPrimitives）：`glEnable(GL_PROGRAM_POINT_SIZE)`（F2/R9）→ buildBatches（clear+upload）→ drawPass×3（Triangle→Line→Point）→ 恢复。批次按 (ShaderKind, 逻辑 markerSize, depth 推断 layer) 分组上传 VBO（GLVertex 16B）+ VAO；`drawPass` 取 `QChartGL::program(kind, projection)`、上传 `u_viewProj`（camera->viewProjectionMatrix——2D QChartCamera / 3D QChartCamera3D 通用，3D 轴/网格/盒均经同一批次管线）等 uniform 后 glDrawArrays。**v2 修复**：① F1（t8）`drawLabels` 开头 `painter.translate(-plotArea.topLeft())`——device 为 plotArea 对齐子控件时父系坐标落 child-local（全窗设备平移为零零回归）；② HiDPI（t14）文件级 DPR 通道 `qchartSetGLPixelRatio/qchartGLPixelRatio`（宿主每帧注入）：uploadBatches `batch.pointSize=逻辑×ratio`、Line pass `glLineWidth(ratio)`（后恢复 1.0），DPR=1 零变化；③ 3D 通路语义：widget3D 渲染前把图元 depth 置 2.0 → decor 批次（depthTest 关=全边可见，B1/B2 线框冒烟语义；3D series 深度后续批次）。标签经共享 `drawLabel` 排绘制。同头文件含辅助结构 GLVertex/GLBatch。

## Constant Variables:
None.（同头文件辅助结构非类成员：`GLVertex{x,y,z:float; r,g,b,a:uint8_t}`（static_assert 16B）；`GLBatch{vao; vbo; primitive:GLenum; vertexCount; baseId; shaderKind:ShaderKind; pointSize; depthTest; depthBias}`）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVector<GLBatch>` | `m_batches` | （private）VBO 批次列表（一次 drawPrimitives 内构建/消费；上下文失效前须 clearBatches） | `QVector<GLBatch>` | 空 | `QChartGL` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QOpenGLChartRenderer` | 构造（default explicit） | 无 | public | — | QChartAbstractWidget（m_glRenderer，OpenGL 激活时）、GL 测试 | — |
| — | `~QOpenGLChartRenderer` | 析构：m_batches 非空 → qWarning（须先 clearBatches） | 无 | public | — | — | — |
| `void` | `transformNumericToCartesian` | 覆写：空操作（GPU 变换在顶点 Shader：`vec3 num=a_pos; vec3 cart=<glslToCartesian 注入>; u_viewProj×cart`） | `QChartScene& scene` | protected | — | `QChartRenderer::render` | — |
| `void` | `cullAndResolveLabels` | 覆写：visibilityCache 全 true；仅绑定标签解析（锚=`proj->toCartesian(prim.numA)` 恒可见；自由标签恒不可见） | `QChartScene& scene` | protected | — | `QChartRenderer::render` | — |
| `void` | `drawPrimitives` | 覆写：无 current 上下文 → qWarning 返回；glEnable(GL_PROGRAM_POINT_SIZE)（F2）→ buildBatches → drawPass×3 → glDisable | `QChartScene& scene, QPaintDevice* device, const QVector<bool>& visibility` | protected | — | `QChartRenderer::render` | — |
| `void` | `drawLabels` | 覆写：QPainter 标签覆盖层；**`painter.translate(-plotArea.topLeft())`（t8 F1：device=plotArea 对齐子控件时父系→child-local；clip 同平移后 plotArea）**；`camera->project(cartesianAnchor)` 出 plotArea 跳过；共享 `QChartRenderer::drawLabel`（R8） | `QChartScene& scene, QPaintDevice* device` | protected | — | `QChartRenderer::render` | — |
| `void` | `clearBatches` | 释放 VAO/VBO 并清空；无 current 上下文 → qWarning + 仅清列表 | 无 | public | — | buildBatches 内部；析构前（容器/测试 makeCurrent 后） | — |
| `void` | `buildBatches` | （private）clearBatches + uploadBatches | `const QChartScene& scene` | private | — | drawPrimitives | — |
| `void` | `uploadBatches` | （private）图元→批次：按 (ShaderKind, pointSize=逻辑 markerSize, isGrid=depth>0.5, isDecor=depth>1.0) 分组；逐类型打包（Point×1/Line×2/Rect×6/Ellipse×48/Polygon·Mesh·Fan·Strip 逐 verts/Path 逐段×2）；VAO/VBO + attr（0=xyz float、1=rgba ubyte normalized）；**batch.pointSize=float(逻辑 pointSize × qchartGLPixelRatio())（t14）**；depthTest=!isDecor、depthBias=isGrid?0.001:0、baseId 按图元数累计 | `const QChartScene& scene` | private | — | buildBatches | — |
| `void` | `drawPass` | （private）单类绘制：program=QChartGL::program(kind, projection)；vp=camera->viewProjectionMatrix(aspect)（2D/3D 相机通用）；uniforms u_viewProj/u_blendAlpha（插值投影 blend）/u_depthBias/u_baseId；**Line 趟 `glLineWidth(max(1, ratio))`（t14，后恢复 1.0）**；Point 传 u_pointSize/u_vertPerPrim=1、Line=2、Triangle=3；逐同 kind 批次 depthTest 开关 → glDrawArrays；末恢复 depth test/lineWidth | `const QChartScene& scene, ShaderKind kind` | private | — | drawPrimitives | `QChartGL` |
| `QOpenGLFunctions_3_3_Core*` | `glFuncs` | （private）current context 的 3.3 Core 函数表 | 无 | private | 指针/`nullptr` | GL 调用内部 | — |

Notes:
- **DPR 通道（t14）**：`qchartSetGLPixelRatio(qreal)/qchartGLPixelRatio()` 为 .cpp 文件级自由函数（namespace 内 s_glPixelRatio=1.0；头文件不动）——正式 setPixelRatio API 随渲染器配置阶段落头（阶段记录 v2 §7 informational③）；宿主 GlPlotWidget::paintGL 每帧注入 `devicePixelRatioF()`，DPR=1 注入 1 → 零回归（t15 审查确认）。
- GLVertex 坐标即 Numeric 坐标（变换在 Shader）；`depth` 字段 GL 端兼作 layer 推断（>0.5 Grid / >1.0 Decor）；3D 通路：widget3D 把全部图元 depth=2.0 → decor 批次（depthTest 关），CPU/GL 线框冒烟对等（阶段记录 v2 §7 informational①：3D series 深度语义后续批次接管）。
- 本类不含上下文管理：宿主（GlPlotWidget/测试）负责 makeCurrent/doneCurrent/registerHost 生命周期；批次须在上下文仍 current 时 clearBatches（析构告警路径）。
- v1 语义保留：GL_PROGRAM_POINT_SIZE（F2）、绑定/自由标签规则、batch 分组 depth 推断、标签共享 drawLabel（R8）。

## Overrided Qt Events:
无（继承 QChartRenderer，非 QWidget）。

## Signals:
None.（非 QObject）
