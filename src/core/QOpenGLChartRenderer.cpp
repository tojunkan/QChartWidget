// QOpenGLChartRenderer.cpp —— GL 后端渲染器实现（design_phase3.md §2.1/§3；t42 骨架）
// 骨架阶段（shader/主 pass 属实现③ t44）：initializeGL 上下文检查、paintGL = buildBatches + 空绘制、
// buildBatches 完整落地（图元收集 + pickTable + 批次分片 + 16B 顶点上传）。
#include "QOpenGLChartRenderer.h"
#include "QChartLayer3D.h"
#include "QChartCamera3D.h"
#include "QChartSeries3D.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLShaderProgram>
#include <QHash>
#include <cstddef>

QOpenGLChartRenderer::QOpenGLChartRenderer(QWidget* host) : m_host(host) {}

QOpenGLChartRenderer::~QOpenGLChartRenderer() {
    // VAO/VBO 属 GL 上下文资源：宿主 QOpenGLWidget 上下文销毁时随上下文释放（Qt 管理），
    // 无需（也不能在没有 current context 时）显式 glDelete；程序池归 QChartGL（引用计数）。
}

// ===== QChartRenderer 接口 =====
void QOpenGLChartRenderer::render(const QChartScene& scene, QPaintDevice* device) {
    if (device != m_host) {
        qWarning() << "QOpenGLChartRenderer::render: device 必须是宿主 QOpenGLWidget（A2 组合）——"
                      "GL 渲染无通用 QPaintDevice 目标；导出请走 QPainterChartRenderer::renderUncached（A4）";
        return;
    }
    // 宿主 paintGL 负责实际绘制（scene 直接传入）；render() 仅校验（与 QPainter 后端接口并列）
    Q_UNUSED(scene);
}

void QOpenGLChartRenderer::renderUncached(const QChartScene& scene, QPaintDevice* device) {
    Q_UNUSED(scene); Q_UNUSED(device);
    // A4：GL 不参与导出（PNG/SVG/PDF 一律走 QPainterChartRenderer）
    qWarning() << "QOpenGLChartRenderer::renderUncached: GL 不参与导出（A4）——请改用 QPainterChartRenderer";
}

void QOpenGLChartRenderer::invalidateBackground() { m_batches.dirty = true; }
void QOpenGLChartRenderer::invalidateForeground() { m_batches.dirty = true; }
void QOpenGLChartRenderer::setCachingEnabled(bool) { /* GL 无 QPixmap 缓存：no-op */ }
bool QOpenGLChartRenderer::isCachingEnabled() const { return true; }

// ===== GL 生命周期 =====
void QOpenGLChartRenderer::initializeGL() {
    if (m_glReady || m_initAttempted) return;
    m_initAttempted = true;
    auto* host = qobject_cast<QOpenGLWidget*>(m_host);
    if (!host) {
        qWarning() << "QOpenGLChartRenderer::initializeGL: 宿主必须是 QOpenGLWidget（A2 组合）";
        return;
    }
    QOpenGLContext* ctx = host->context();
    if (!ctx || !ctx->isValid()) {
        qWarning() << "QOpenGLChartRenderer::initializeGL: GL 上下文不可用（无 GL 环境 → QPainter 路径共存兜底，A9）";
        return;
    }
    if (QOpenGLFunctions* f = ctx->functions()) {
        const auto s = [](const GLubyte* p) {
            return p ? QString::fromLatin1(reinterpret_cast<const char*>(p)) : QStringLiteral("?");
        };
        qInfo().noquote() << "QOpenGLChartRenderer::initializeGL: GL 就绪（§10.1 基线记录）"
                          << "vendor=" << s(f->glGetString(GL_VENDOR))
                          << "renderer=" << s(f->glGetString(GL_RENDERER))
                          << "version=" << s(f->glGetString(GL_VERSION));
    }
    if (QOpenGLFunctions_3_3_Core* f3 = glFuncs()) {
        f3->glEnable(GL_PROGRAM_POINT_SIZE);   // pointShader 的 gl_PointSize 生效（§4）
        f3->glEnable(GL_DEPTH_TEST);           // 主 pass 分层时按批次开关；默认 ON
    }
    // 编译 3 shader 程序（§4；惰性——首次 program(kind) 于当前上下文编译，t43 O2）
    QChartGL::program(ShaderKind::Line);
    QChartGL::program(ShaderKind::Point);
    QChartGL::program(ShaderKind::Pick);   // t46 ID 帧留用
    m_glReady = true;   // 上下文可用 + 程序就绪；批次池惰性建（首个 buildBatches）
}

void QOpenGLChartRenderer::paintGL(const QChartScene& scene) {
    if (!m_glReady) initializeGL();
    if (!m_glReady) return;   // 无 GL → 空绘制（GlHost 隐藏回退纯 QPainter，§5.1 ⚠）
    if (m_batches.dirty) buildBatches(scene);
    QOpenGLFunctions_3_3_Core* f = glFuncs();
    if (!f) return;

    // 视口 = plotArea（§5.1：NDC→像素映射与 QPainter clipToScreen(plotArea) 一致；深度缓冲按 widget 尺寸 A3）
    const QRect pa = scene.plotArea.toRect();
    f->glViewport(0, 0, qMax(1, pa.width()), qMax(1, pa.height()));

    // ⚠ ① 不透明清屏（§5.1 透明语义教训：glClearColor(场景背景色, 1.0)——GL 像素不可透明，
    //   否则 QOpenGLWidget 原生子窗口透明区露桌面/其他程序，用户可见穿帮）
    const QColor bg = scene.backgroundColor.isValid() ? scene.backgroundColor : QColor(Qt::white);
    f->glClearColor(float(bg.redF()), float(bg.greenF()), float(bg.blueF()), 1.0f);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ② 主 pass（§5.1/§5.2）：Grid（depth ON + u_depthBias>0）→ Series（depth ON）→
    //    ForegroundDecor（depth OFF，最后）——z-buffer 正确遮挡（painter's algorithm 退休，A4）
    drawPass(scene, ShaderKind::Line);
    drawPass(scene, ShaderKind::Point);
    // ③ Overlay（billboard 标签/图例，§6）属 t46；交换由 QOpenGLWidget 自动（§5.1 步骤 5）
}

void QOpenGLChartRenderer::resizeGL(int w, int h) {
    m_viewportSize = QSize(w, h);
    if (QOpenGLFunctions_3_3_Core* f = glFuncs())
        f->glViewport(0, 0, w, h);   // 深度缓冲由 QOpenGLWidget 默认 FBO 按尺寸重建（A3）
}

QRgb QOpenGLChartRenderer::pickIdAt(const QPoint& pos, const QChartScene& scene) {
    // §5.3 三闸限流：①Qt 事件循环合并连续 mouse move（外部）；②位移 ≥1px + 距上次 ≥16ms；③m_glReady
    if (!m_glReady) return qRgb(255, 255, 255);              // ③ 惰性初始化前跳过（§5.3 第三道闸；哨兵）
    if (!m_pickClock.isValid()) m_pickClock.start();
    const qint64 now = m_pickClock.elapsed();
    if (m_lastPickValid && pos == m_lastPickPos) return m_lastPickResult;            // 位移 <1px → 保持
    if (m_lastPickValid && now - m_lastPickMs < 16) return m_lastPickResult;          // <16ms → 保持
    auto* host = qobject_cast<QOpenGLWidget*>(m_host);
    if (!host || !host->context() || !host->context()->isValid()) return qRgb(255, 255, 255);

    // 须先 makeCurrent：buildBatches 的 uploadBatches 是 GL 调用（需 current context；
    //   t46 实测：先建批次后 makeCurrent → uploadBatches 空转 → 批次为空 → ID 帧全哨兵）
    host->makeCurrent();
    // 拾取表与批次同步（§5.3 重建时机：invalidateForeground → buildBatches；首帧前 dirty → 此处构建）
    if (m_batches.dirty) buildBatches(scene);

    // ID 帧（§5.3 ★自包含：清 color（哨兵白背景）+ 清 depth + 写 depth——与主 pass 深度/时序完全解耦，
    //   同主 pass 批次顺序/深度语义：Grid 偏置、Series 深度遮挡、Decor 关深度后画）
    QOpenGLFunctions_3_3_Core* f = glFuncs();
    if (!f) { host->doneCurrent(); return qRgb(255, 255, 255); }
    f->glBindFramebuffer(GL_FRAMEBUFFER, host->defaultFramebufferObject());
    const QRect pa = scene.plotArea.toRect();
    f->glViewport(0, 0, qMax(1, pa.width()), qMax(1, pa.height()));
    f->glClearColor(1.0f, 1.0f, 1.0f, 1.0f);              // 背景 = 0xFFFFFF 哨兵（无图元 → 未命中）
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawPass(scene, ShaderKind::Pick);                     // 全批次（含哨兵批次）

    // 1×1 readback（O(1)，A6）：GL 行序底→顶，需翻转 y；glFinish 保证绘制完成（软渲染确定性）
    GLubyte px[4] = { 255, 255, 255, 255 };
    if (pos.x() >= 0 && pos.x() < pa.width() && pos.y() >= 0 && pos.y() < pa.height()) {
        f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        f->glFinish();
        f->glReadPixels(pos.x(), qMax(0, pa.height() - 1 - pos.y()), 1, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, px);
    }
    host->doneCurrent();

    m_lastPickPos = pos;
    m_lastPickMs = now;
    m_lastPickValid = true;
    m_lastPickResult = qRgb(px[0], px[1], px[2]);
    return m_lastPickResult;
}

// ===== 批次构建 =====
void QOpenGLChartRenderer::buildBatches(const QChartScene& scene) {
    if (!m_glReady) return;
    collectScene(scene);      // 图元 + pickTable（dirty 才调用，§3.2）
    uploadBatches();          // 分片 + 16B 顶点上传
    // A3（队长批准，t48）：图元列表为瞬态缓存——VBO 已持有顶点数据，清空释放常驻内存
    // （1M 点 QChartPrimitive=120B → ≈120MB 超标主因；§7.1 预算按 worldCache+VBO 核算，
    //   图元列表不计常驻；下次 invalidate 再重建）。
    // ⚠ 必须 squeeze()：QVector::clear() 只置 size=0 保留 capacity——120MB 缓冲仍驻留（t48 实测 RSS 不降）
    m_cachedPrimitives.clear();
    m_cachedPrimitives.squeeze();
    m_batches.dirty = false;
}

void QOpenGLChartRenderer::collectScene(const QChartScene& scene) {
    m_cachedPrimitives.clear();
    m_cachedLabels.clear();
    m_batches.pickTable.clear();
    if (!scene.is3D() || !scene.camera3D) return;

    for (const QChartLayer3D* g : scene.layers3D) {
        if (!g) continue;
        const int layerStart = m_cachedPrimitives.size();
        const int pickBefore = m_batches.pickTable.size();
        // labels 出参：billboard 标签（tick 标签 + 轴标题，§6 overlay 绘制源；QPainter 路径同源）
        g->collectPrimitives(scene.camera3D, scene.plotArea, m_cachedPrimitives, &m_cachedLabels);
        const int layerEnd = m_cachedPrimitives.size();

        // pickTable（§8.1）：前缀 = 轴/网格装饰（无系列归属）；系列段 = 合并列表尾部
        // （Layer3D 按 m_series3D 顺序追加系列图元 → 与逐系列收集顺序一致；NaN 跳过确定性一致）
        int seriesStart = layerStart;
        while (seriesStart < layerEnd &&
               m_cachedPrimitives.at(seriesStart).layer != QChartPrimitive::Layer::Series)
            ++seriesStart;
        for (int i = layerStart; i < seriesStart; ++i)
            m_batches.pickTable.append({ nullptr, -1, m_cachedPrimitives.at(i).layer });

        const ProjectFn3D fn = g->makeProjectFn(scene.camera3D, scene.plotArea);
        for (QChartSeries3D* s : g->series3DList()) {
            if (!s || !s->isVisible()) continue;
            QVector<QChartPrimitive> sp;
            s->collectPrimitives(fn, sp);
            for (const QChartPrimitive& prim : sp)
                m_batches.pickTable.append({ s, prim.dataIndex, QChartPrimitive::Layer::Series });
        }
        // 对齐校验：本层 pickTable 增量 == 本层图元增量（图元 ID 与批次顶点一一对应，§5.3）
        Q_ASSERT(m_batches.pickTable.size() - pickBefore == layerEnd - layerStart);
    }
}

void QOpenGLChartRenderer::uploadBatches() {
    QOpenGLFunctions_3_3_Core* f = glFuncs();
    if (!f) return;

    // 1. 分片：同 (Layer, primitive) 合并；Point 批次另按 markerSize 分键（u_pointSize 按批次
    //    uniform，§3.1「同 series 内 markerSize 一致」——混合点尺寸会画错）；每批次顶点 ≤ 64K（§3.1）
    constexpr int kMaxVertsPerBatch = 65536;
    struct Group { QChartPrimitive::Layer layer; GLenum prim; float pointSize; QVector<int> prims; };
    QVector<Group> groups;   // 组数少（层×原语×点尺寸），线性查找足够
    for (int i = 0; i < m_cachedPrimitives.size(); ++i) {
        const QChartPrimitive& prim = m_cachedPrimitives.at(i);
        const GLenum glPrim = (prim.type == QChartPrimitive::Type::Point) ? GL_POINTS : GL_LINES;
        const int vertsPer = (prim.type == QChartPrimitive::Type::Point) ? 1 : 2;
        const float pointSize = (prim.type == QChartPrimitive::Type::Point)
            ? float(prim.markerSize) : 0.0f;
        int gi = -1;
        for (int g = 0; g < groups.size(); ++g) {
            const Group& grp = groups.at(g);
            if (grp.layer == prim.layer && grp.prim == glPrim && grp.pointSize == pointSize &&
                grp.prims.size() * vertsPer + vertsPer <= kMaxVertsPerBatch) {
                gi = g;
                break;
            }
        }
        if (gi < 0) {
            gi = groups.size();
            groups.append({ prim.layer, glPrim, pointSize, {} });
        }
        groups[gi].prims.append(i);
    }

    // 2. 每组分片 → GLBatch + 16B interleaved 顶点打包 + 上传（A5：World float3 + ubyte4 颜色）
    m_batches.batches.clear();
    m_batches.batches.reserve(groups.size());
    int baseId = 0;
    for (const Group& grp : groups) {
        GLBatch b;
        b.primitive = grp.prim;
        b.layer = grp.layer;
        b.depthTest = (grp.layer != QChartPrimitive::Layer::ForegroundDecor);   // A4 恒可见
        b.depthBias = (grp.layer == QChartPrimitive::Layer::Grid) ? kGridDepthBias : 0.0;   // §5.2
        b.pointSize = grp.pointSize;   // Point 批次 u_pointSize（Line 批次 0）
        b.idSentinel = (grp.layer != QChartPrimitive::Layer::Series);   // ID 帧哨兵（§5.3 定案）
        b.baseId = baseId;
        const int vertsPer = (grp.prim == GL_POINTS) ? 1 : 2;
        b.vertexCount = GLsizei(grp.prims.size() * vertsPer);
        baseId += grp.prims.size();   // ID 零内存：id = baseId + gl_VertexID/vertsPer（§5.3）

        QVector<GLVertex> verts;
        verts.reserve(b.vertexCount);
        for (int pi : grp.prims) {
            const QChartPrimitive& prim = m_cachedPrimitives.at(pi);
            const GLVertex vA{ prim.worldA.x(), prim.worldA.y(), prim.worldA.z(),
                               uint8_t(prim.color.red()), uint8_t(prim.color.green()),
                               uint8_t(prim.color.blue()), uint8_t(prim.color.alpha()) };
            verts.append(vA);
            if (vertsPer == 2)
                verts.append(GLVertex{ prim.worldB.x(), prim.worldB.y(), prim.worldB.z(),
                                       uint8_t(prim.color.red()), uint8_t(prim.color.green()),
                                       uint8_t(prim.color.blue()), uint8_t(prim.color.alpha()) });
        }

        f->glGenVertexArrays(1, &b.vao);
        f->glGenBuffers(1, &b.vbo);
        f->glBindVertexArray(b.vao);
        f->glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
        f->glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * sizeof(GLVertex)),
                        verts.constData(), GL_STATIC_DRAW);
        // 顶点布局（§3.2/§4）：0 = vec3 position（float）；1 = vec4 color（UNSIGNED_BYTE normalized）
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                                 reinterpret_cast<void*>(0));
        f->glEnableVertexAttribArray(1);
        f->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLVertex),
                                 reinterpret_cast<void*>(offsetof(GLVertex, r)));
        f->glBindVertexArray(0);
        m_batches.batches.append(b);
    }
}

void QOpenGLChartRenderer::drawPass(const QChartScene& scene, ShaderKind kind) {
    QOpenGLFunctions_3_3_Core* f = glFuncs();
    if (!f || !scene.camera3D || m_batches.batches.isEmpty()) return;
    QOpenGLShaderProgram* prog = QChartGL::program(kind);
    if (!prog) return;

    const qreal aspect = scene.plotArea.height() > 0.0
        ? scene.plotArea.width() / scene.plotArea.height() : 1.0;
    const QMatrix4x4 vp = scene.camera3D->viewProjectionMatrix(aspect);
    prog->bind();
    prog->setUniformValue("u_viewProj", vp);

    // 分层（§5.1/§5.2）：Grid → Series → ForegroundDecor（decor 关深度最后画，恒可见 A4/A7）
    static const QChartPrimitive::Layer kLayerOrder[3] = {
        QChartPrimitive::Layer::Grid,
        QChartPrimitive::Layer::Series,
        QChartPrimitive::Layer::ForegroundDecor,
    };
    for (QChartPrimitive::Layer layer : kLayerOrder) {
        const bool depthOn = (layer != QChartPrimitive::Layer::ForegroundDecor);
        if (depthOn) f->glEnable(GL_DEPTH_TEST);
        else         f->glDisable(GL_DEPTH_TEST);   // decor 恒后画
        for (const GLBatch& b : m_batches.batches) {
            if (b.layer != layer) continue;
            if (kind != ShaderKind::Pick) {
                // 主 pass：Line 程序只画线批次、Point 程序只画点批次（§4 程序分工）
                const bool isPoint = (b.primitive == GL_POINTS);
                if ((kind == ShaderKind::Line) == isPoint) continue;
            }
            prog->setUniformValue("u_depthBias", float(b.depthBias));   // Grid > 0（§5.2）
            prog->setUniformValue("u_baseId", b.baseId);                // 图元 ID 基址（§5.3）
            prog->setUniformValue("u_vertPerPrim", b.primitive == GL_POINTS ? 1 : 2);
            if (kind == ShaderKind::Point)
                prog->setUniformValue("u_pointSize", b.pointSize);
            if (kind == ShaderKind::Pick)
                prog->setUniformValue("u_sentinel", b.idSentinel ? 1 : 0);   // 非 Series 批次 → 0xFFFFFF（§5.3）
            f->glBindVertexArray(b.vao);
            f->glDrawArrays(b.primitive, 0, b.vertexCount);
        }
    }
    f->glDisable(GL_DEPTH_TEST);
    prog->release();
}

QOpenGLFunctions_3_3_Core* QOpenGLChartRenderer::glFuncs() const {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    // Qt6：versioned 函数经工厂获取（对象归工厂缓存，勿 delete）
    return ctx ? QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx) : nullptr;
}
