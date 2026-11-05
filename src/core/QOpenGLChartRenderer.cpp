// QOpenGLChartRenderer.cpp —— GL 后端渲染器实现
#include "QOpenGLChartRenderer.h"
#include "QChartAbstractProjection.h"
#include "QInterpolatedProjection.h"
#include "QChartCamera.h"
#include "QChartMath.h"   // triangulatePolygon（多边形填充 → 三角索引）
#include <QOpenGLContext>
#include <QOpenGLVersionFunctionsFactory>
#include <QPainter>
#include <QLoggingCategory>
#include <cmath>
#include <QOpenGLShaderProgram>

Q_LOGGING_CATEGORY(logGLRender, "chart.render.gl")

// ===== t14(HiDPI)：宿主 DPR 注入通道 =====
// 图元内的点尺寸/线宽是逻辑单位（CPU QPainter 按逻辑像素自动放大）；
// GL 侧以设备像素光栅化，需按宿主 DPR 放大保持 CPU/GL 视觉对等。
// 最小侵入：文件级共享值 + 自由函数（宿主 GlPlotWidget::paintGL 每帧注入；
// 头文件不动——正式 setPixelRatio API 随渲染器配置阶段落头）。
namespace {
qreal s_glPixelRatio = 1.0;
}
void qchartSetGLPixelRatio(qreal r) { if (r > 0.0) s_glPixelRatio = r; }
qreal qchartGLPixelRatio() { return s_glPixelRatio; }

// t73：组尾锚点助手（与二维 CPU 后端共用同一分类：Point→自身 / Line→尾端 numB / 顶点型→末顶点 /
// Rect·Ellipse→中心）。定义在 QChartRenderer.cpp；头文件不在本批 inScope，故在此前置声明共用。
QVector3D qchartGroupTailAnchorNumeric(const QChartPrimitive& p);


// 构造 / 析构

QOpenGLChartRenderer::~QOpenGLChartRenderer()
{
    if(m_batches.size() > 0) {
        qWarning(logGLRender) << "QOpenGLChartRenderer destroyed with non-empty batches!";
    }
}

// 基类虚函数实现

void QOpenGLChartRenderer::transformNumericToCartesian(QChartScene& /*scene*/)
{
    // ★ GPU 后端：变换在 Shader 中完成，CPU 端什么都不做
}

// （tier1 显式锚点可见性判定 = 基类 QChartRenderer::anchorVisibleInPlotArea，两后端共用单一实现）

void QOpenGLChartRenderer::cullAndResolveLabels(QChartScene& scene)
{
    const QChartAbstractProjection* proj = scene.projection;

    // 逐段解析（段内完成，不跨段）
    auto resolveSegment = [&](QChartSegment& seg) {
        // ★ GPU 后端：真正的裁剪在 GPU 上（视锥/深度由硬件判），CPU 侧一律视为可见。
        //   因此 prim.visible 在 GL 路径上恒为 true、**不承载语义**；GL 的拾取走 ID 帧，也不需要它。
        //   （契约 S6 要求该字段与 cart* 一样由步骤 2 写回，所以仍然显式赋值，
        //     避免继承上一次收集残留的 false。）
        for (QChartPrimitive& prim : seg.prims) prim.visible = true;
        if (!proj) return;

        for (QChartTextLabel& label : seg.labels) {
            // tier1：显式锚点 → CPU 侧 toCartesian + 像素可见性判定
            if (label.hasExplicitAnchor()) {
                label.cartesianAnchor = proj->toCartesian(label.numericAnchor);
                label.visible = anchorVisibleInPlotArea(scene, label.cartesianAnchor);
                continue;
            }

            // tier2：段内绑定 → 用该图元的组尾锚点（与 CPU 后端同一分类，避免两后端锚点语义分叉）；
            // 图元可见性归 GPU 裁剪（本后端粗裁 = 全可见）
            if (label.segPrim >= 0) {
                if (label.segPrim < seg.prims.size()) {
                    const QVector3D num = qchartGroupTailAnchorNumeric(seg.prims[label.segPrim]);
                    label.cartesianAnchor = proj->toCartesian(num);
                    label.visible = true;
                } else {
                    label.visible = false;   // 越界绑定：无对应图元 → 不可见
                }
                continue;
            }

            // tier3：自由标签 —— 既定契约：纯 GPU 后端无能力渲染自由标签 → 不可见。
            // 二维网格脊标签因此在 GL 后端不显示：已知缺陷、既定接受差异。
            label.visible = false;
        }
    };

    for (QChartSegment& seg : scene.axisSegments) resolveSegment(seg);
    for (QChartSegment& seg : scene.seriesSegments) resolveSegment(seg);
}

void QOpenGLChartRenderer::drawPrimitives(QChartScene& scene,
                                          QPaintDevice* device)
{
    if (!QOpenGLContext::currentContext()) {
        qWarning() << "No current OpenGL context!";
        return;
    }
    QOpenGLFunctions_3_3_Core* f = glFuncs();
    if (!f) return;

    // ★ 深度状态显式复位：标签覆盖层用 QPainter 绘制，会把 GL_DEPTH_TEST 关掉、
    //   把 glDepthMask 置 false 且**不恢复**。若不重置，下一帧所有图元都不写深度 →
    //   遮挡退化为"按 pass 顺序"（Triangle→Line→Point，点永远压在网格上）。
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LESS);

    // TODO(blend)：半透明顶点色在 GL 下颜色不对（premultiplied/straight alpha 状态混用），
    //   本轮暂时只保证不透明填充正确；标签覆盖层会留下 premultiplied 混合函数。

    // F2(S0 修复)：GL_PROGRAM_POINT_SIZE 未开启时，Core Profile 顶点着色器写的
    // gl_PointSize 被忽略（llvmpipe 实测刻度点完全不光栅化）；绘制前开启、绘制后恢复默认。
    f->glEnable(GL_PROGRAM_POINT_SIZE);

    // 进入这个函数说明viewDirty确实是脏的。但是这些逻辑在基类的render函数里已经实现过了。
    // 可以直接调用buildBatches
    buildBatches(scene);

    // 执行绘制
    drawPass(scene, ShaderKind::Triangle);
    drawPass(scene, ShaderKind::Line);
    drawPass(scene, ShaderKind::Point);

    f->glDisable(GL_PROGRAM_POINT_SIZE);
}

void QOpenGLChartRenderer::drawLabels(QChartScene& scene, QPaintDevice* device)
{
    if (!device) return;

    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    const QRectF& plotArea = scene.plotArea;

    QPainter painter(device);
    painter.setRenderHint(QPainter::Antialiasing, true);
    // ★ F1(t8)：GL 标签覆盖层按宿主局部坐标系绘制——device 可能是 plotArea 对齐的
    // QOpenGLWidget 子控件（原点=plotArea.topLeft()）：整体平移 -topLeft 后，plotArea/
    // pp.screen 等父系坐标即落 child-local 空间（clip 用同一平移后的 plotArea）。
    // 全窗/整图设备（plotArea.topLeft()==0）平移为零，行为不变。
    painter.translate(-plotArea.topLeft());
    painter.setClipRect(plotArea);

    QVector<const QChartTextLabel*> flat;
    for (const QChartSegment& s : scene.axisSegments)
        for (const QChartTextLabel& l : s.labels) flat.append(&l);
    for (const QChartSegment& s : scene.seriesSegments)
        for (const QChartTextLabel& l : s.labels) flat.append(&l);

    for (const QChartTextLabel* lp : flat) {
        const QChartTextLabel& label = *lp;
        if (!label.visible) continue;

        // 用 Camera 的 project 将 Cartesian 转成屏幕坐标（平移后即 child-local）
        QChartProjectedPoint pp = camera->project(label.cartesianAnchor, plotArea);
        const QPointF pixelPos = pp.screen;

        // t73②：与二维 CPU 后端同策略——二次包含判定只对 tier1（显式锚点）保留；
        // tier2 的可见性来自图元裁剪（本后端粗裁=可见），锚点恰落边界时不应整组丢弃。
        if (label.hasExplicitAnchor() && !plotArea.contains(pixelPos)) continue;

        // 与 CPU 后端共用同一标签排版逻辑（对齐/避让/钳制）
        drawLabel(painter, plotArea, pixelPos, label.text, label.color,
                  label.fontSize, label.alignment);
    }
}

// 批次构建

void QOpenGLChartRenderer::buildBatches(const QChartScene& scene)
{
    clearBatches();
    uploadBatches(scene);
}

void QOpenGLChartRenderer::uploadBatches(const QChartScene& scene)
{
    QOpenGLFunctions_3_3_Core* f = glFuncs();
    if (!f) return;

    // 段序展开成扁平表（批次仍按样式全局分组，但遍历顺序 = 段序，保证轴垫在系列之下）
    QVector<const QChartPrimitive*> flat;
    for (const QChartSegment& s : scene.axisSegments)
        for (const QChartPrimitive& p : s.prims) flat.append(&p);
    for (const QChartSegment& s : scene.seriesSegments)
        for (const QChartPrimitive& p : s.prims) flat.append(&p);

    const int N = flat.size();
    if (N == 0) return;

    // ---- 1. 先按图元类型分片 ----
    struct GroupKey {
        ShaderKind kind;
        float pointSize;
        bool isGrid;
        bool isDecor;
        float lineWidth;    // 仅 Line kind 参与（其余填 0）

        bool operator==(const GroupKey& other) const {
            return kind == other.kind && qFuzzyCompare(pointSize, other.pointSize)
                && isGrid == other.isGrid && isDecor == other.isDecor
                && qFuzzyCompare(lineWidth, other.lineWidth);
        }
    };

    struct Group {
        GroupKey key;
        QVector<const QChartPrimitive*> prims;
    };

    QVector<Group> groups;

    for (int i = 0; i < N; ++i) {
        const QChartPrimitive& prim = *flat[i];

        ShaderKind kind;
        switch (prim.type) {
        case QChartPrimitive::Type::Point:
            kind = ShaderKind::Point;
            break;
        case QChartPrimitive::Type::Line:
        case QChartPrimitive::Type::Path:
            kind = ShaderKind::Line;
            break;
        case QChartPrimitive::Type::Rect:
        case QChartPrimitive::Type::Ellipse:
        case QChartPrimitive::Type::Polygon:
        case QChartPrimitive::Type::TriangleMesh:
        case QChartPrimitive::Type::TriangleFan:
        case QChartPrimitive::Type::TriangleStrip:
            kind = ShaderKind::Triangle;
            break;
        default:
            continue;
        }

        // 判断 layer（从 prim 的某个字段推断，暂时用 depth 或自定义）
        // 这里简化：depth > 0.5 视为 Grid/Decor
        bool isGrid = (prim.depth > 0.5f);
        bool isDecor = (prim.depth > 1.0f);

        // 线宽只对 Line 有意义（同宽线段合批，减少 glLineWidth 切换）
        GroupKey key{kind, prim.markerSize, isGrid, isDecor,
                     (kind == ShaderKind::Line) ? prim.penWidth : 0.0f};

        int gi = -1;
        for (int g = 0; g < groups.size(); ++g) {
            if (groups[g].key == key) {
                gi = g;
                break;
            }
        }
        if (gi == -1) {
            groups.append({key, {}});
            gi = groups.size() - 1;
        }
        groups[gi].prims.append(&prim);
    }

    // ---- 2. 每组合并成 GLBatch ----
    m_batches.reserve(groups.size());
    int baseId = 0;

    for (const Group& grp : groups) {
        GLBatch batch;
        batch.shaderKind = grp.key.kind;
        batch.pointSize = float(grp.key.pointSize * qchartGLPixelRatio());
        batch.lineWidth = grp.key.lineWidth;
        batch.depthTest = !grp.key.isDecor;
        batch.depthBias = grp.key.isGrid ? 0.001f : 0.0f;
        batch.baseId = baseId;

        // 确定 OpenGL 图元类型
        switch (grp.key.kind) {
        case ShaderKind::Point:
            batch.primitive = GL_POINTS;
            break;
        case ShaderKind::Line:
            batch.primitive = GL_LINES;
            break;
        case ShaderKind::Triangle:
            batch.primitive = GL_TRIANGLES;
            break;
        default:
            continue;
        }

        // 打包顶点
        QVector<GLVertex> verts;
        verts.reserve(grp.prims.size() * 3); // 粗略估计

        for (const QChartPrimitive* prim : grp.prims) {
            const QVector3D& numA = prim->numA;
            const QVector3D& numB = prim->numB;

            switch (prim->type) {
            case QChartPrimitive::Type::Point: {
                GLVertex v{ float(numA.x()), float(numA.y()), float(numA.z()),
                            uint8_t(prim->color.red()), uint8_t(prim->color.green()),
                            uint8_t(prim->color.blue()), uint8_t(prim->color.alpha()) };
                verts.append(v);
                break;
            }
            case QChartPrimitive::Type::Line: {
                GLVertex vA{ float(numA.x()), float(numA.y()), float(numA.z()),
                             uint8_t(prim->color.red()), uint8_t(prim->color.green()),
                             uint8_t(prim->color.blue()), uint8_t(prim->color.alpha()) };
                GLVertex vB{ float(numB.x()), float(numB.y()), float(numB.z()),
                             uint8_t(prim->color.red()), uint8_t(prim->color.green()),
                             uint8_t(prim->color.blue()), uint8_t(prim->color.alpha()) };
                verts.append(vA);
                verts.append(vB);
                break;
            }
            case QChartPrimitive::Type::Rect: {
                // 矩形 → 2 个三角形（6 个顶点）；填充色优先 fillColor
                const QRectF& r = prim->numRect;
                float x1 = r.left(), x2 = r.right(), y1 = r.top(), y2 = r.bottom();
                const QColor fill = (prim->fillColor.alpha() > 0) ? prim->fillColor : prim->color;
                uint8_t rCol = uint8_t(fill.red());
                uint8_t gCol = uint8_t(fill.green());
                uint8_t bCol = uint8_t(fill.blue());
                uint8_t aCol = uint8_t(fill.alpha());
                // Triangle 1: (x1,y1) -> (x2,y1) -> (x1,y2)
                verts.append({x1, y1, 0, rCol, gCol, bCol, aCol});
                verts.append({x2, y1, 0, rCol, gCol, bCol, aCol});
                verts.append({x1, y2, 0, rCol, gCol, bCol, aCol});
                // Triangle 2: (x2,y1) -> (x2,y2) -> (x1,y2)
                verts.append({x2, y1, 0, rCol, gCol, bCol, aCol});
                verts.append({x2, y2, 0, rCol, gCol, bCol, aCol});
                verts.append({x1, y2, 0, rCol, gCol, bCol, aCol});
                break;
            }
            case QChartPrimitive::Type::Ellipse: {
                // 椭圆 → 近似扇形三角形
                // 为了简化，这里用 16 个扇区
                const QRectF& r = prim->numRect;
                float cx = r.center().x(), cy = r.center().y();
                float rx = r.width() / 2, ry = r.height() / 2;
                const QColor fill = (prim->fillColor.alpha() > 0) ? prim->fillColor : prim->color;
                uint8_t rCol = uint8_t(fill.red());
                uint8_t gCol = uint8_t(fill.green());
                uint8_t bCol = uint8_t(fill.blue());
                uint8_t aCol = uint8_t(fill.alpha());
                const int segs = 16;
                for (int i = 0; i < segs; ++i) {
                    float a1 = 2 * M_PI * i / segs;
                    float a2 = 2 * M_PI * (i+1) / segs;
                    verts.append({cx, cy, 0, rCol, gCol, bCol, aCol});  // 中心
                    verts.append({float(cx + rx * cos(a1)), float(cy + ry * sin(a1)), 0, rCol, gCol, bCol, aCol});
                    verts.append({float(cx + rx * cos(a2)), float(cy + ry * sin(a2)), 0, rCol, gCol, bCol, aCol});
                }
                break;
            }
            case QChartPrimitive::Type::Polygon: {
                // 多边形 → 三角化（耳切/扇形）后按索引展开；退化多边形直接跳过
                const QVector<QVector3D>& vertsList = prim->numVerts;
                if (vertsList.size() < 3) break;
                // 填充色优先（fillColor 有效时），否则退回 color
                const QColor fill = (prim->fillColor.alpha() > 0) ? prim->fillColor : prim->color;
                uint8_t rCol = uint8_t(fill.red());
                uint8_t gCol = uint8_t(fill.green());
                uint8_t bCol = uint8_t(fill.blue());
                uint8_t aCol = uint8_t(fill.alpha());
                QVector<int> indices;
                if (!QChartMath::triangulatePolygon(vertsList, indices)) break;
                for (int t : indices) {
                    const QVector3D& v = vertsList.at(t);
                    verts.append({float(v.x()), float(v.y()), float(v.z()),
                                  rCol, gCol, bCol, aCol});
                }
                break;
            }
            case QChartPrimitive::Type::TriangleMesh: {
                // 三角网格：按 numIndices 展开（无索引时退化为顺序顶点）
                const QVector<QVector3D>& vertsList = prim->numVerts;
                const QVector<int>& idx = prim->numIndices;
                const QColor fill = (prim->fillColor.alpha() > 0) ? prim->fillColor : prim->color;
                uint8_t rCol = uint8_t(fill.red());
                uint8_t gCol = uint8_t(fill.green());
                uint8_t bCol = uint8_t(fill.blue());
                uint8_t aCol = uint8_t(fill.alpha());
                if (!idx.isEmpty()) {
                    for (int t : idx) {
                        if (t < 0 || t >= vertsList.size()) continue;
                        const QVector3D& v = vertsList.at(t);
                        verts.append({float(v.x()), float(v.y()), float(v.z()),
                                      rCol, gCol, bCol, aCol});
                    }
                } else {
                    for (const QVector3D& v : vertsList) {
                        verts.append({float(v.x()), float(v.y()), float(v.z()),
                                      rCol, gCol, bCol, aCol});
                    }
                }
                break;
            }
            case QChartPrimitive::Type::TriangleFan:
            case QChartPrimitive::Type::TriangleStrip: {
                // 扇形/条带：直接使用 numVerts（GL 图元类型仍按 GL_TRIANGLES 展开顺序顶点）
                const QVector<QVector3D>& vertsList = prim->numVerts;
                uint8_t rCol = uint8_t(prim->color.red());
                uint8_t gCol = uint8_t(prim->color.green());
                uint8_t bCol = uint8_t(prim->color.blue());
                uint8_t aCol = uint8_t(prim->color.alpha());
                for (const QVector3D& v : vertsList) {
                    verts.append({float(v.x()), float(v.y()), float(v.z()),
                                  rCol, gCol, bCol, aCol});
                }
                break;
            }
            case QChartPrimitive::Type::Path: {
                // Path 作为折线处理
                const QVector<QVector3D>& vertsList = prim->numVerts;
                if (vertsList.size() < 2) break;
                uint8_t rCol = uint8_t(prim->color.red());
                uint8_t gCol = uint8_t(prim->color.green());
                uint8_t bCol = uint8_t(prim->color.blue());
                uint8_t aCol = uint8_t(prim->color.alpha());
                for (int i = 0; i < vertsList.size() - 1; ++i) {
                    const QVector3D& v1 = vertsList[i];
                    const QVector3D& v2 = vertsList[i+1];
                    verts.append({float(v1.x()), float(v1.y()), float(v1.z()), rCol, gCol, bCol, aCol});
                    verts.append({float(v2.x()), float(v2.y()), float(v2.z()), rCol, gCol, bCol, aCol});
                }
                break;
            }
            default:
                break;
            }
        }

        if (verts.isEmpty()) continue;

        batch.vertexCount = verts.size();

        // 生成 VAO/VBO
        f->glGenVertexArrays(1, &batch.vao);
        f->glGenBuffers(1, &batch.vbo);

        f->glBindVertexArray(batch.vao);
        f->glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
        f->glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(GLVertex),
                        verts.constData(), GL_STATIC_DRAW);

        // 顶点属性
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                                 reinterpret_cast<void*>(0));
        f->glEnableVertexAttribArray(1);
        f->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLVertex),
                                 reinterpret_cast<void*>(offsetof(GLVertex, r)));

        f->glBindVertexArray(0);

        m_batches.append(batch);
        baseId += grp.prims.size();  // 图元数量（不是顶点数）
    }
}


// 绘制


void QOpenGLChartRenderer::drawPass(const QChartScene& scene, ShaderKind kind)
{
    QOpenGLFunctions_3_3_Core* f = glFuncs();
    if (!f) return;

    // 获取动态 Shader
    QOpenGLShaderProgram* prog = QChartGL::program(kind, scene.projection);
    if (!prog) return;

    // 从 Camera 获取 viewProjection 矩阵
    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    qreal aspect = (scene.plotArea.height() > 0)
                   ? scene.plotArea.width() / scene.plotArea.height() : 1.0;
    QMatrix4x4 vp = camera->viewProjectionMatrix(aspect);

    prog->bind();
    prog->setUniformValue("u_viewProj", vp);
    qreal blendAlpha = 0.0;
    if (const QInterpolatedProjection* interp =
            dynamic_cast<const QInterpolatedProjection*>(scene.projection)) {
        blendAlpha = interp->blend();
    }
    prog->setUniformValue("u_blendAlpha", float(blendAlpha));

    // t14(HiDPI)：线宽按宿主 DPR 放大（图元 penWidth 逻辑单位；S0 轴/网格 penWidth 恒 1
    // → 设备宽 = 1×dpr，与 CPU 1 逻辑 px 对齐）。DPR=1 时 glLineWidth(1)=默认 → 零变化。
    // 注：宽度超 1 依赖驱动 ALIASED_LINE_WIDTH_RANGE（llvmpipe/多数桌面驱动支持 [1,N]）；
    // 驱动仅支持 1 时 glLineWidth 静默钳制为 1（不报错）。
    const bool linePass = (kind == ShaderKind::Line);

    // 分层绘制（Grid -> Series -> Decor）
    for (const GLBatch& batch : m_batches) {
        if (batch.shaderKind != kind) continue;

        // 线宽按图元 penWidth × DPR（驱动只支持 1.0 时会被静默钳制）
        if (linePass)
            f->glLineWidth(qMax(1.0f, float(batch.lineWidth * qchartGLPixelRatio())));

        // depthTest / depthBias
        if (batch.depthTest) f->glEnable(GL_DEPTH_TEST);
        else f->glDisable(GL_DEPTH_TEST);

        prog->setUniformValue("u_depthBias", float(batch.depthBias));
        prog->setUniformValue("u_baseId", batch.baseId);

        // 点的特殊情况
        if (kind == ShaderKind::Point) {
            prog->setUniformValue("u_pointSize", batch.pointSize);
            prog->setUniformValue("u_vertPerPrim", 1);
        } else if (kind == ShaderKind::Line) {
            prog->setUniformValue("u_vertPerPrim", 2);
        } else { // Triangle
            prog->setUniformValue("u_vertPerPrim", 3);
        }

        f->glBindVertexArray(batch.vao);
        f->glDrawArrays(batch.primitive, 0, batch.vertexCount);
    }

    if (linePass)
        f->glLineWidth(1.0f);   // 恢复默认
    f->glDisable(GL_DEPTH_TEST);
    prog->release();
}


// 辅助


QOpenGLFunctions_3_3_Core* QOpenGLChartRenderer::glFuncs() const
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) return nullptr;
    return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
}

void QOpenGLChartRenderer::clearBatches()
{
    QOpenGLFunctions_3_3_Core* f = glFuncs();
    if (!f) {
        qWarning() << "clearBatches: no current context, GPU resources leaked!";
        m_batches.clear();
        return;
    }

    for (const GLBatch& batch : m_batches) {
        if (batch.vao) f->glDeleteVertexArrays(1, &batch.vao);
        if (batch.vbo) f->glDeleteBuffers(1, &batch.vbo);
    }
    m_batches.clear();
}