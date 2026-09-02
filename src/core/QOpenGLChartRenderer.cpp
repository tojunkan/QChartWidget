// QOpenGLChartRenderer.cpp —— GL 后端渲染器实现
#include "QOpenGLChartRenderer.h"
#include "QChartAbstractProjection.h"
#include "QInterpolatedProjection.h"
#include "QChartCamera3D.h"
#include "QChartCamera.h"
#include <QOpenGLContext>
#include <QOpenGLVersionFunctionsFactory>
#include <QPainter>
#include <QLoggingCategory>
#include <cmath>
#include <QOpenGLShaderProgram>

Q_LOGGING_CATEGORY(logGLRender, "chart.render.gl")


// 构造 / 析构


QOpenGLChartRenderer::QOpenGLChartRenderer(QOpenGLWidget* host)
    : m_host(host)
{
    QChartGL::registerHost();
}

QOpenGLChartRenderer::~QOpenGLChartRenderer()
{
    clearBatches();
    QChartGL::unregisterHost();
}


// 基类虚函数实现


void QOpenGLChartRenderer::transformNumericToCartesian(QChartScene& /*scene*/)
{
    // ★ GPU 后端：变换在 Shader 中完成，CPU 端什么都不做
}

void QOpenGLChartRenderer::cullAndResolveLabels(QChartScene& scene)
{
    const int N = scene.primitives.size();
    m_visibilityCache.resize(N);
    std::fill(m_visibilityCache.begin(), m_visibilityCache.end(), true);

    const QChartAbstractProjection* proj = scene.projection;
    if (!proj) return;

    // ---- 绑定标签：用 CPU 计算 Cartesian 坐标 ----
    for (QChartTextLabel& label : scene.labels) {
        if (label.refPrimitiveId >= 0 && label.refPrimitiveId < N) {
            const QVector3D& num = scene.primitives[label.refPrimitiveId].numA;
            label.cartesianAnchor = proj->toCartesian(num);
            label.visible = true;
        } else {
            // GPU 后端不支持自由标签
            label.visible = false;
        }
    }
}

void QOpenGLChartRenderer::drawPrimitives(QChartScene& scene,
                                          QPaintDevice* device,
                                          const QVector<bool>& /*visibility*/)
{
    if (device != m_host) {
        qWarning() << "QOpenGLChartRenderer::drawPrimitives: device 必须是 GlHost";
        return;
    }

    if (!m_glReady) {
        initializeGL();
        if (!m_glReady) return;
    }

    // 用基类的 m_viewDirty 触发批次重建
    if (m_viewDirty) {
        buildBatches(scene);
        m_viewDirty = false;
    }

    // 执行绘制
    drawPass(scene, ShaderKind::Triangle);
    drawPass(scene, ShaderKind::Line);
    drawPass(scene, ShaderKind::Point);
}

void QOpenGLChartRenderer::drawLabels(QChartScene& scene, QPaintDevice* device)
{
    if (!device || device != m_host) return;

    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    const QRectF& plotArea = scene.plotArea;

    QPainter painter(device);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(plotArea);

    QFont font = painter.font();

    for (const QChartTextLabel& label : scene.labels) {
        if (!label.visible) continue;

        // 用 Camera 的 project 将 Cartesian 转成屏幕坐标
        QChartProjectedPoint pp = camera->project(label.cartesianAnchor, plotArea);
        QPointF pixelPos = pp.screen;

        if (!plotArea.contains(pixelPos)) continue;

        font.setPointSizeF(label.fontSize);
        painter.setFont(font);
        painter.setPen(label.color);

        const int flags = int(label.alignment) | Qt::TextDontClip;
        painter.drawText(QRectF(pixelPos, QSizeF(0, 0)), flags, label.text);
    }
}


// GL 生命周期


void QOpenGLChartRenderer::initializeGL()
{
    if (m_glReady || m_initAttempted) return;
    m_initAttempted = true;

    if (!m_host) {
        qWarning() << "QOpenGLChartRenderer::initializeGL: 无宿主";
        return;
    }

    QOpenGLContext* ctx = m_host->context();
    if (!ctx || !ctx->isValid()) {
        qWarning() << "QOpenGLChartRenderer::initializeGL: GL 上下文无效";
        return;
    }

    if (QOpenGLFunctions_3_3_Core* f = glFuncs()) {
        f->glEnable(GL_PROGRAM_POINT_SIZE);
        f->glEnable(GL_DEPTH_TEST);
        f->glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        qInfo() << "QOpenGLChartRenderer: GL 就绪";
    } else {
        qWarning() << "QOpenGLChartRenderer::initializeGL: 无法获取 GL 3.3 Core 函数";
        return;
    }

    m_glReady = true;
}

void QOpenGLChartRenderer::paintGL(const QChartScene& scene)
{
    // ★ 调用基类 render，它会走 transformNumericToCartesian →
    //   cullAndResolveLabels → drawPrimitives → drawLabels
    QChartRenderer::render(const_cast<QChartScene&>(scene), m_host);
}

void QOpenGLChartRenderer::resizeGL(int w, int h)
{
    m_viewportSize = QSize(w, h);
    if (QOpenGLFunctions_3_3_Core* f = glFuncs()) {
        f->glViewport(0, 0, w, h);
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

    const QVector<QChartPrimitive>& prims = scene.primitives;
    const int N = prims.size();
    if (N == 0) return;

    // ---- 1. 先按图元类型分片 ----
    struct GroupKey {
        ShaderKind kind;
        float pointSize;
        bool isGrid;
        bool isDecor;

        bool operator==(const GroupKey& other) const {
            return kind == other.kind && qFuzzyCompare(pointSize, other.pointSize)
                && isGrid == other.isGrid && isDecor == other.isDecor;
        }
    };

    struct Group {
        GroupKey key;
        QVector<const QChartPrimitive*> prims;
    };

    QVector<Group> groups;

    for (int i = 0; i < N; ++i) {
        const QChartPrimitive& prim = prims[i];

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

        GroupKey key{kind, prim.markerSize, isGrid, isDecor};

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
        batch.pointSize = grp.key.pointSize;
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
                // 矩形 → 2 个三角形（6 个顶点）
                const QRectF& r = prim->numRect;
                float x1 = r.left(), x2 = r.right(), y1 = r.top(), y2 = r.bottom();
                uint8_t rCol = uint8_t(prim->color.red());
                uint8_t gCol = uint8_t(prim->color.green());
                uint8_t bCol = uint8_t(prim->color.blue());
                uint8_t aCol = uint8_t(prim->color.alpha());
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
                uint8_t rCol = uint8_t(prim->color.red());
                uint8_t gCol = uint8_t(prim->color.green());
                uint8_t bCol = uint8_t(prim->color.blue());
                uint8_t aCol = uint8_t(prim->color.alpha());
                const int segs = 16;
                for (int i = 0; i < segs; ++i) {
                    float a1 = 2 * M_PI * i / segs;
                    float a2 = 2 * M_PI * (i+1) / segs;
                    verts.append({cx, cy, 0, rCol, gCol, bCol, aCol});  // 中心
                    verts.append({cx + rx * cos(a1), cy + ry * sin(a1), 0, rCol, gCol, bCol, aCol});
                    verts.append({cx + rx * cos(a2), cy + ry * sin(a2), 0, rCol, gCol, bCol, aCol});
                }
                break;
            }
            case QChartPrimitive::Type::Polygon:
            case QChartPrimitive::Type::TriangleMesh:
            case QChartPrimitive::Type::TriangleFan:
            case QChartPrimitive::Type::TriangleStrip: {
                // 直接使用 numVerts
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

    // 分层绘制（Grid -> Series -> Decor）
    for (const GLBatch& batch : m_batches) {
        if (batch.shaderKind != kind) continue;

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
        m_batches.clear();
        return;
    }

    for (const GLBatch& batch : m_batches) {
        if (batch.vao) f->glDeleteVertexArrays(1, &batch.vao);
        if (batch.vbo) f->glDeleteBuffers(1, &batch.vbo);
    }
    m_batches.clear();
}