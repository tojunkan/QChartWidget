// QOpenGLChartRenderer.cpp —— GL 后端渲染器实现
#include "QOpenGLChartRenderer.h"
#include "QChartAbstractProjection.h"
#include "QChartProjection3D.h"    // 数值域裁剪：走投影反算
#include "QChartCamera3D.h"        // 数值域裁剪：相机视野盒
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

// t73：组尾锚点助手（与二维 CPU 后端共用同一分类：Point→自身 / 折线类→末顶点（即尾端）/ ...
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
    QOpenGLFunctions_4_3_Core* f = glFuncs();
    if (!f) return;

    // ★ 深度状态显式复位：标签覆盖层用 QPainter 绘制，会把 GL_DEPTH_TEST 关掉、
    //   把 glDepthMask 置 false 且**不恢复**。深度函数也必须是显式值（GL 默认是 GL_LESS，
    //   与"同深度后画的赢"相反），否则 3D 的共面像素会翻面。
    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LEQUAL);

    // ★ 深度测试只对 3D 有意义：
    //   2D 里所有图元 z 相同，深度不承担任何正确语义——叠放由"绘制顺序 = 段序"决定；
    //   开着它反而会把顺序问题伪装成深度问题（GL_LESS / GL_LEQUAL 各错一半，实测过）。
    const bool is3D = (scene.projection && scene.projection->dimension() >= 3);
    if (is3D) f->glEnable(GL_DEPTH_TEST);
    else      f->glDisable(GL_DEPTH_TEST);

    // ===== 数值域裁剪盒（Numeric space clip box）=====
    // ★ 盒子**不另存**：它等价于"相机视野盒经投影反算"——三轴范围就是这么得来的
    //   （见 QChartWidget3D::updateAxes），而 scene 同时带着 camera 与 projection，
    //   所以现算即可，零新增字段。3D 下由此得到"盒子外的数据看不见"；
    //   2D 不用（2D 的可见域由 plotArea 与图元级裁剪承担，仍取极值 = 不裁）。
    QVector3D numMin(-3.0e38f, -3.0e38f, -3.0e38f);
    QVector3D numMax( 3.0e38f,  3.0e38f,  3.0e38f);
    if (is3D) {
        if (const auto* cam3 = dynamic_cast<const QChartCamera3D*>(scene.camera)) {
            if (const auto* proj3 = dynamic_cast<const QChartProjection3D*>(scene.projection)) {
                const QCube b = proj3->computeDataBounds(cam3->viewCube());
                if (b.isValid()) { numMin = b.min; numMax = b.max; }
            }
        }
    }

    // ★ 混合状态必须显式接管，不能靠"上下文默认/上一步遗留"：
    //   实测 Qt 建好 GL 控件后，第一帧开始时 GL_BLEND 就已经是**开**的，且函数是**预乘**形式
    //   （srcRGB = GL_ONE）；而我们的着色器输出**直通 alpha**（顶点色原样进 v_color）。
    //   若沿用预乘公式，半透明色会少乘一次 a ⇒ 偏亮（实测：半透明红叠白底得到 (255,192,188)，
    //   正确应为 (243,162,160)，与 CPU QPainter 后端一致）。
    //   这里统一成直通混合：颜色 = src.rgb*a + dst*(1-a)，alpha 通道同样按直通累积。
    f->glEnable(GL_BLEND);
    f->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                           GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // F2(S0 修复)：GL_PROGRAM_POINT_SIZE 未开启时，Core Profile 顶点着色器写的
    // gl_PointSize 被忽略（llvmpipe 实测刻度点完全不光栅化）；绘制前开启、绘制后恢复默认。
    f->glEnable(GL_PROGRAM_POINT_SIZE);

    // 调试开关（QCHART_GL_DEBUG=1）：打印绘制前的混合/深度状态 + 上下文 alpha 位数。
    if (qEnvironmentVariableIsSet("QCHART_GL_DEBUG")) {
        GLint srcAlpha = 0, dstAlpha = 0, srcRGB = 0, dstRGB = 0;
        GLboolean depthMask = GL_TRUE;
        f->glGetIntegerv(0x80C9 /*GL_BLEND_SRC_ALPHA*/, &srcAlpha);
        f->glGetIntegerv(0x80C7 /*GL_BLEND_DST_ALPHA*/, &dstAlpha);
        f->glGetIntegerv(0x80CB /*GL_BLEND_SRC_RGB*/, &srcRGB);
        f->glGetIntegerv(0x80C9 /*GL_BLEND_DST_RGB 的常量是 0x80C8，这里用 SRC 占位*/, &dstRGB);
        f->glGetBooleanv(0x0B72 /*GL_DEPTH_WRITEMASK*/, &depthMask);
        const QSurfaceFormat ctxFmt = QOpenGLContext::currentContext()->format();
        qDebug().nospace() << "[GLDBG] blendEnabled=" << f->glIsEnabled(GL_BLEND)
                           << " srcRGB=0x" << QString::number(srcRGB, 16)
                           << " srcA=0x" << QString::number(srcAlpha, 16)
                           << " dstA=0x" << QString::number(dstAlpha, 16)
                           << " depthTest=" << f->glIsEnabled(GL_DEPTH_TEST)
                           << " depthMask=" << bool(depthMask)
                           << " ctxAlphaBits=" << ctxFmt.alphaBufferSize()
                           << " ctxFmt=" << ctxFmt.majorVersion() << "." << ctxFmt.minorVersion();
    }

    buildBatches(scene);

    if (is3D) {
        // ★ 3D 两趟：3D 的前后关系由深度测试负责，半透明只负责"最后叠加"。
        //   第 1 趟：全不透明批次（正常写深度）；
        //   第 2 趟：含半透明的批次——**关深度写**，否则它会挡住自己后面的东西（半透明面变"灰壳"）。
        //   两趟内部各自仍按段序。
        f->glDepthMask(GL_TRUE);
        for (const GLBatch& batch : m_batches)
            if (!batch.translucent) drawBatch(scene, batch, numMin, numMax);
        f->glDepthMask(GL_FALSE);
        for (const GLBatch& batch : m_batches)
            if (batch.translucent) drawBatch(scene, batch, numMin, numMax);
        f->glDepthMask(GL_TRUE);
    } else {
        // 2D：单趟，严格按段序（2D 没有深度测试，叠放语义完全由段序承担；分两趟反而会打乱）
        for (const GLBatch& batch : m_batches)
            drawBatch(scene, batch, numMin, numMax);
    }

    f->glDisable(GL_PROGRAM_POINT_SIZE);

    // 调试开关（QCHART_GL_DEBUG=1）：读回画布中心像素，证明"实际写进帧缓冲的是什么"。
    if (qEnvironmentVariableIsSet("QCHART_GL_DEBUG")) {
        const qreal dpr = qchartGLPixelRatio();
        const int cx = int(scene.plotArea.center().x() * dpr);
        const int cyFlipped = int((scene.plotArea.height() - scene.plotArea.center().y()) * dpr);
        unsigned char px[4] = {0, 0, 0, 0};
        f->glReadPixels(cx, cyFlipped, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        qDebug().nospace() << "[GLDBG] centerPixel(plot) rgba=("
                           << int(px[0]) << "," << int(px[1]) << "," << int(px[2]) << "," << int(px[3])
                           << ") at (" << cx << "," << cyFlipped << ")";
    }
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
    int baseId = 0;
    uploadBatches(scene.axisSegments, baseId);     // 先轴：垫在下面
    uploadBatches(scene.seriesSegments, baseId);   // 后系列：盖在上面
}

void QOpenGLChartRenderer::uploadBatches(const QVector<QChartSegment>& segs, int& baseId)
{
    QOpenGLFunctions_4_3_Core* f = glFuncs();
    if (!f) return;

    // 本函数只处理**一个**段数组；轴段与系列段由调用方分两次调用，
    // 因此批次列表顺序天然等于"轴组在前、系列组在后"，绘制时顺序遍历即可。
    QVector<const QChartPrimitive*> flat;
    for (const QChartSegment& s : segs)
        for (const QChartPrimitive& p : s.prims) flat.append(&p);

    const int N = flat.size();
    if (N == 0) return;

    // ---- 1. 先按图元类型分片 ----
    struct GroupKey {
        ShaderKind kind;
        float pointSize;
        float lineWidth;    // 仅 Line kind 参与（其余填 0）

        bool operator==(const GroupKey& other) const {
            return kind == other.kind && qFuzzyCompare(pointSize, other.pointSize)
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
        case QChartPrimitive::Type::Grid:
            // Grid 单独一套程序：quads 域细分（每格 4 控制点）
            kind = ShaderKind::Grid;
            break;
        default:
            continue;
        }

        // 线宽只对 Line 有意义（同宽线段合批，减少 glLineWidth 切换）
        GroupKey key{kind, prim.markerSize,
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

    for (const Group& grp : groups) {
        GLBatch batch;
        batch.shaderKind = grp.key.kind;
        batch.pointSize = float(grp.key.pointSize * qchartGLPixelRatio());
        batch.lineWidth = grp.key.lineWidth;
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
        case ShaderKind::Grid:
            // 曲面网格：每格 4 个控制点的 patch（quads 域）
            batch.primitive = GL_PATCHES;
            break;
        default:
            continue;
        }

        // 打包顶点
        QVector<GLVertex> verts;
        verts.reserve(grp.prims.size() * 3); // 粗略估计

        // 索引缓冲只给"含 Grid 的 Triangle 批次"用（其它批次维持原 glDrawArrays 路径，
        // 避免让已经正确的 bar/polygon/line 平白换一条绘制通路）。
        bool useIndexBuffer = false;
        for (const QChartPrimitive* prim : grp.prims) {
            if (prim->type == QChartPrimitive::Type::Grid) { useIndexBuffer = true; break; }
        }
        QVector<GLuint> indices;

        for (const QChartPrimitive* prim : grp.prims) {
            const QVector3D& numA = prim->numA;
            const GLuint firstVert = GLuint(verts.size());

            switch (prim->type) {
            case QChartPrimitive::Type::Point: {
                GLVertex v{ float(numA.x()), float(numA.y()), float(numA.z()),
                            uint8_t(prim->color.red()), uint8_t(prim->color.green()),
                            uint8_t(prim->color.blue()), uint8_t(prim->color.alpha()) };
                verts.append(v);
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
                // 调试开关（QCHART_GL_DEBUG=1）：打印多边形填充色（输入侧证据）。
                static bool polyLogged = false;
                if (!polyLogged && qEnvironmentVariableIsSet("QCHART_GL_DEBUG")) {
                    polyLogged = true;
                    qDebug().nospace() << "[GLDBG] polygon fill rgba=(" << int(rCol) << ","
                                       << int(gCol) << "," << int(bCol) << "," << int(aCol)
                                       << ") nVerts=" << vertsList.size()
                                       << " nTriIdx=" << indices.size();
                }
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
            case QChartPrimitive::Type::Grid: {
                // 规则网格：rows×cols 控制点**只进顶点表一次**（旧的 TriangleMesh 是 9600 个
                // 展开顶点，现在是 1681 个共享控制点 + 派生索引）。
                // ★ 索引按"每格 4 个"派生，顺序必须是 GL quads 域的控制点约定：
                //   (u,v) = (0,0), (1,0), (1,1), (0,1)  ⇒  i00, i01, i11, i10
                //   （不是三角形的 6 个索引——那是"不细分"时代的事）
                const QVector<QVector3D>& vertsList = prim->numVerts;
                const QColor fill = (prim->fillColor.alpha() > 0) ? prim->fillColor : prim->color;
                const uint8_t rCol = uint8_t(fill.red());
                const uint8_t gCol = uint8_t(fill.green());
                const uint8_t bCol = uint8_t(fill.blue());
                const uint8_t aCol = uint8_t(fill.alpha());
                const GLuint base = GLuint(verts.size());
                for (const QVector3D& v : vertsList) {
                    verts.append({float(v.x()), float(v.y()), float(v.z()),
                                  rCol, gCol, bCol, aCol});
                }
                const int rows = prim->rows;
                const int cols = prim->cols;
                if (rows >= 2 && cols >= 2 && vertsList.size() == rows * cols) {
                    for (int r = 0; r + 1 < rows; ++r) {
                        for (int c = 0; c + 1 < cols; ++c) {
                            const int i00 = r * cols + c;      // (u=0,v=0)
                            const int i01 = i00 + 1;           // (u=1,v=0)
                            const int i10 = i00 + cols;        // (u=0,v=1)
                            const int i11 = i10 + 1;           // (u=1,v=1)
                            const QVector3D& a00 = vertsList[i00];
                            const QVector3D& a01 = vertsList[i01];
                            const QVector3D& a10 = vertsList[i10];
                            const QVector3D& a11 = vertsList[i11];
                            if (!std::isfinite(a00.x()) || !std::isfinite(a00.y()) || !std::isfinite(a00.z())
                                || !std::isfinite(a01.x()) || !std::isfinite(a01.y()) || !std::isfinite(a01.z())
                                || !std::isfinite(a10.x()) || !std::isfinite(a10.y()) || !std::isfinite(a10.z())
                                || !std::isfinite(a11.x()) || !std::isfinite(a11.y()) || !std::isfinite(a11.z()))
                                continue;
                            indices << base + GLuint(i00) << base + GLuint(i01)
                                    << base + GLuint(i11) << base + GLuint(i10);
                        }
                    }
                }
                break;
            }
            default:
                break;
            }

            // 展开型图元：新追加的顶点按顺序补索引，保证同一批次能统一走 glDrawElements
            //（Grid 的索引已在上面按格派生，不能再补顺序索引）
            if (useIndexBuffer && prim->type != QChartPrimitive::Type::Grid) {
                for (GLuint j = firstVert; j < GLuint(verts.size()); ++j) indices.append(j);
            }
        }

        if (verts.isEmpty()) continue;

        batch.vertexCount = verts.size();

        // 本批次是否含半透明顶点色：3D 下据此"不写深度 + 最后画"（2D 不看它）
        for (const GLVertex& gv : verts) {
            if (gv.a != 255) { batch.translucent = true; break; }
        }

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

        // 索引缓冲（仅含 Grid 的 Triangle 批次）。注意：ELEMENT_ARRAY_BUFFER 的绑定是
        // **VAO 状态**，所以必须在 VAO 仍绑定时建立。
        if (useIndexBuffer && !indices.isEmpty()) {
            f->glGenBuffers(1, &batch.ibo);
            f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.ibo);
            f->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                            indices.size() * sizeof(GLuint),
                            indices.constData(), GL_STATIC_DRAW);
            batch.indexCount = GLsizei(indices.size());
        }

        f->glBindVertexArray(0);

        m_batches.append(batch);
        baseId += grp.prims.size();  // 图元数量（不是顶点数）

        // TEMP-BATCHDUMP：每批次的 kind/顶点数/半透明标记/图元类型
        if (qEnvironmentVariableIsSet("QCHART_GL_DEBUG")) {
            qDebug().nospace() << "[BATCH] kind=" << int(batch.shaderKind)
                               << " primitive=0x" << QString::number(batch.primitive, 16)
                               << " verts=" << batch.vertexCount
                               << " idx=" << batch.indexCount
                               << " transl=" << batch.translucent
                               << " lineWidth=" << batch.lineWidth
                               << " prims=" << grp.prims.size();
        }
    }
}


// 绘制
//
// ★ 单批次绘制：不再"按 ShaderKind 分三趟遍历全部批次"。
//   三趟的问题是把"叠放顺序"交给了 pass 顺序（Triangle→Line→Point），
//   于是网格线（Line 趟）会压在柱体/填充（Triangle 趟）之上，与"轴在系列之下"的段序相反。
//   现在按 m_batches 的顺序遍历，而它的顺序就是打包顺序 = 轴段在前、系列段在后。

void QOpenGLChartRenderer::drawBatch(const QChartScene& scene, const GLBatch& batch,
                                     const QVector3D& numMin, const QVector3D& numMax)
{
    QOpenGLFunctions_4_3_Core* f = glFuncs();
    if (!f || !batch.vao) return;

    // 获取动态 Shader（按 (kind, projection) 缓存）
    QOpenGLShaderProgram* prog = QChartGL::program(batch.shaderKind, scene.projection);
    if (!prog) return;

    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    const qreal aspect = (scene.plotArea.height() > 0)
                         ? scene.plotArea.width() / scene.plotArea.height() : 1.0;

    prog->bind();
    prog->setUniformValue("u_viewProj", camera->viewProjectionMatrix(aspect));
    qreal blendAlpha = 0.0;
    if (const QInterpolatedProjection* interp =
            dynamic_cast<const QInterpolatedProjection*>(scene.projection)) {
        blendAlpha = interp->blend();
    }
    prog->setUniformValue("u_blendAlpha", float(blendAlpha));
    prog->setUniformValue("u_baseId", batch.baseId);
    // 数值域裁剪盒（FS 里按 Numeric 位置 discard）；极值 = 不裁
    prog->setUniformValue("u_numMin", numMin);
    prog->setUniformValue("u_numMax", numMax);

    switch (batch.shaderKind) {
    case ShaderKind::Point:
        prog->setUniformValue("u_pointSize", batch.pointSize);
        prog->setUniformValue("u_vertPerPrim", 1);
        break;
    case ShaderKind::Line:
        prog->setUniformValue("u_vertPerPrim", 2);
        // ★ 折线细分已交给硬件：按"2 控制点/段"的 patch 绘制，分段数由 TCS 依
        //   「弓高/弦长」判据写 gl_TessLevelOuter[0]（判据与 CPU 后端的 subdivide 同源）。
        f->glPatchParameteri(GL_PATCH_VERTICES, 2);
        // t14(HiDPI)：线宽按宿主 DPR 放大（驱动不支持 >1 时静默钳制为 1）
        f->glLineWidth(qMax(1.0f, float(batch.lineWidth * qchartGLPixelRatio())));
        break;
    case ShaderKind::Grid:
        prog->setUniformValue("u_vertPerPrim", 4);
        // 曲面网格：每格 4 个控制点，细分级别由 TCS 依同一判据（四角平均弓高 ÷ 对角线）写
        f->glPatchParameteri(GL_PATCH_VERTICES, 4);
        break;
    default:   // Triangle
        prog->setUniformValue("u_vertPerPrim", 3);
        break;
    }

    f->glBindVertexArray(batch.vao);
    if (batch.indexCount > 0) {
        // 索引批次（Grid）：共享控制点 + 每格 4 个索引，图元类型见 batch.primitive（GL_PATCHES）
        f->glDrawElements(batch.primitive, batch.indexCount, GL_UNSIGNED_INT, nullptr);
    } else if (batch.shaderKind == ShaderKind::Line) {
        // patch 顶点数已由 glPatchParameteri 指定；顶点数/2 = patch 数
        f->glDrawArrays(GL_PATCHES, 0, batch.vertexCount);
    } else {
        f->glDrawArrays(batch.primitive, 0, batch.vertexCount);
    }

    if (batch.shaderKind == ShaderKind::Line)
        f->glLineWidth(1.0f);   // 恢复默认
    prog->release();
}


// 辅助


QOpenGLFunctions_4_3_Core* QOpenGLChartRenderer::glFuncs() const
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) return nullptr;
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_3_Core>(ctx);
    // ★ 拿不到函数集时只警告一次：否则所有 GL 调用点静默 `if (!f) return;`，
    //   表现为"整块 GL 画面消失"，很难联想到是版本/函数集问题。
    //   （上下文升到 4.3 后，低版本 3.3 包装器在 core profile 下仍应可取；取不到就是驱动/格式问题。）
    static bool warned = false;
    if (!f && !warned) {
        warned = true;
        qWarning() << "QOpenGLChartRenderer: 取不到 QOpenGLFunctions_4_3_Core（context 版本"
                   << ctx->format().majorVersion() << "." << ctx->format().minorVersion()
                   << " profile=" << int(ctx->format().profile()) << "）——GL 绘制将全部跳过";
    }
    return f;
}

void QOpenGLChartRenderer::clearBatches()
{
    QOpenGLFunctions_4_3_Core* f = glFuncs();
    if (!f) {
        qWarning() << "clearBatches: no current context, GPU resources leaked!";
        m_batches.clear();
        return;
    }

    for (const GLBatch& batch : m_batches) {
        if (batch.vao) f->glDeleteVertexArrays(1, &batch.vao);
        if (batch.vbo) f->glDeleteBuffers(1, &batch.vbo);
        if (batch.ibo) f->glDeleteBuffers(1, &batch.ibo);
    }
    m_batches.clear();
}
