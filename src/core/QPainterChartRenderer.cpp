// QPainterChartRenderer.cpp
#include "QPainterChartRenderer.h"
#include "QChartAbstractProjection.h"
#include "QChartMath.h"   // 细分判据常量（与 GLSL 的 GS 同一份）
#include "QChartCamera.h"
#include "QChartCamera3D.h"
#include "QCube.h"
#include <QPainter>
#include <QLoggingCategory>
#include <cmath>

Q_LOGGING_CATEGORY(logPainter, "chart.render.painter")

// t73：组尾锚点助手（按图元类型分类：Point→自身 / Line→尾端 / 顶点型→末顶点 / Rect·Ellipse→中心）。
// 定义在 QChartRenderer.cpp（与 GL 后端共用同一实现，避免两后端语义分叉）；头文件不在本批 inScope，
// 故此处以相同签名的前置声明共用。
QVector3D qchartGroupTailAnchorCartesian(const QChartPrimitive& p);

// 步骤 2a：Numeric → Cartesian 变换

void QPainterChartRenderer::transformNumericToCartesian(QChartScene& scene)
{
    const QChartAbstractProjection* proj = scene.projection;
    if (!proj) return;

    const bool isIdentity = proj->isIdentityMapping();

    // 细分判据（与 GLSL 的 GS 共用 QChartMath 里那一份常量）：
    //   在 **Numeric** 上均匀取点、再逐点投影；判据本身只用 Cartesian 量（视图无关）。
    //   k = ceil( d / (ratio·L) )，d = 弓高、L = 弦长；恒等投影直接不细分。
    auto subdivide = [&](const QVector3D& n0, const QVector3D& n1,
                         const QVector3D& c0, const QVector3D& c1,
                         QVector<QVector3D>& out, bool skipFirst) {
        int k = 1;
        // ★ 闭环特例（角度维扫满一圈）：两端点在 Cartesian 里重合 ⇒ L≈0，但中点偏离极大（d≈2r）。
        //   相对判据 d/(ratio·L) 此时趋于无穷，被上限截住 —— 这正是"圆环画成 32 边形"的来路。
        //   所以**绝不能**用 `L > eps` 当门槛：那会把闭环整段跳过，环直接画不出来（踩过）。
        const QVector3D cm = proj->toCartesian((n0 + n1) * 0.5f);
        const qreal d = (cm - (c0 + c1) * 0.5f).length();
        if (d > 1e-9) {
            const qreal L = qMax((c1 - c0).length(), 1e-12);
            const qreal need = d / (QChartMath::SUBDIV_RATIO * L);
            k = int(qBound(1.0, std::ceil(need), double(QChartMath::SUBDIV_MAX_SEGMENTS)));
        }
        for (int i = (skipFirst ? 1 : 0); i <= k; ++i) {
            const float t = float(i) / float(k);
            out.append(proj->toCartesian(n0 + (n1 - n0) * t));
        }
    };

    // ===== 部分变换的判据在这里 =====
    // 只有 cartValid == false 的段才重算 cart*；其余段（视图变过、内容没变）一个字节不动。
    // 失效来源只有两个：段被重建（新段默认 false）、投影变化（全体置 false）。
    QVector<QChartSegment*> todo;
    for (QChartSegment& s : scene.axisSegments) if (!s.cartValid) todo.append(&s);
    for (QChartSegment& s : scene.seriesSegments) if (!s.cartValid) todo.append(&s);
    const int skipped = scene.axisSegments.size() + scene.seriesSegments.size() - todo.size();

    QVector<QChartPrimitive*> flat;
    for (QChartSegment* s : todo)
        for (QChartPrimitive& p : s->prims) flat.append(&p);

    for (QChartPrimitive* pp : flat) {
        QChartPrimitive& prim = *pp;
        switch (prim.type) {
        case QChartPrimitive::Type::Point:
            prim.cartA = proj->toCartesian(prim.numA);
            break;

        case QChartPrimitive::Type::Rect:
        case QChartPrimitive::Type::Ellipse: {
            if (isIdentity) {
                QVector3D tl = proj->toCartesian(
                    QVector3D(prim.numRect.left(), prim.numRect.top(), 0.0f));
                QVector3D br = proj->toCartesian(
                    QVector3D(prim.numRect.right(), prim.numRect.bottom(), 0.0f));
                prim.cartRect = QRectF(tl.x(), tl.y(), br.x() - tl.x(), br.y() - tl.y());
            } else {
                // Rect/Ellipse → Polygon
                prim.type = QChartPrimitive::Type::Polygon;
                prim.numVerts.clear();
                prim.numVerts.reserve(4);
                prim.numVerts.append(QVector3D(prim.numRect.left(),  prim.numRect.top(),    0.0f));
                prim.numVerts.append(QVector3D(prim.numRect.right(), prim.numRect.top(),    0.0f));
                prim.numVerts.append(QVector3D(prim.numRect.right(), prim.numRect.bottom(), 0.0f));
                prim.numVerts.append(QVector3D(prim.numRect.left(),  prim.numRect.bottom(), 0.0f));
                prim.cartVerts.resize(4);
                for (int i = 0; i < 4; ++i) {
                    prim.cartVerts[i] = proj->toCartesian(prim.numVerts[i]);
                }
            }
            break;
        }

        case QChartPrimitive::Type::Path: {
            // 非线性投影下逐相邻段加密（Numeric 插点 → 逐点投影）
            if (!isIdentity && prim.numVerts.size() >= 2) {
                QVector<QVector3D> dense;
                for (int j = 0; j + 1 < prim.numVerts.size(); ++j) {
                    const QVector3D c0 = proj->toCartesian(prim.numVerts[j]);
                    const QVector3D c1 = proj->toCartesian(prim.numVerts[j + 1]);
                    subdivide(prim.numVerts[j], prim.numVerts[j + 1], c0, c1, dense, j > 0);
                }
                prim.cartVerts = dense;
            } else {
                prim.cartVerts.resize(prim.numVerts.size());
                for (int j = 0; j < prim.numVerts.size(); ++j)
                    prim.cartVerts[j] = proj->toCartesian(prim.numVerts[j]);
            }
            prim.cartIndices = prim.numIndices;
            break;
        }

        case QChartPrimitive::Type::Polygon:
        case QChartPrimitive::Type::TriangleMesh:
        case QChartPrimitive::Type::TriangleFan:
        case QChartPrimitive::Type::TriangleStrip: {
            prim.cartVerts.resize(prim.numVerts.size());
            for (int j = 0; j < prim.numVerts.size(); ++j) {
                prim.cartVerts[j] = proj->toCartesian(prim.numVerts[j]);
            }
            prim.cartIndices = prim.numIndices;
            break;
        }

        case QChartPrimitive::Type::Grid: {
            // Grid：rows×cols 共享控制点表 → 逐点投影；**索引在这里一次性派生**
            //（跳过控制点非有限的格子，与旧 TriangleMesh 收集期跳过的行为等价），
            // 于是 draw 阶段可以与 TriangleMesh 共用同一条索引绘制路径。
            prim.cartVerts.resize(prim.numVerts.size());
            for (int j = 0; j < prim.numVerts.size(); ++j) {
                prim.cartVerts[j] = proj->toCartesian(prim.numVerts[j]);
            }
            prim.cartIndices.clear();
            const int rows = prim.rows;
            const int cols = prim.cols;
            if (rows >= 2 && cols >= 2 && prim.numVerts.size() == rows * cols) {
                prim.cartIndices.reserve(6 * (rows - 1) * (cols - 1));
                for (int r = 0; r + 1 < rows; ++r) {
                    for (int c = 0; c + 1 < cols; ++c) {
                        const int i00 = r * cols + c;      // 左上
                        const int i01 = i00 + 1;           // 右上
                        const int i10 = i00 + cols;        // 左下
                        const int i11 = i10 + 1;           // 右下
                        const QVector3D& a00 = prim.numVerts[i00];
                        const QVector3D& a01 = prim.numVerts[i01];
                        const QVector3D& a10 = prim.numVerts[i10];
                        const QVector3D& a11 = prim.numVerts[i11];
                        if (!std::isfinite(a00.x()) || !std::isfinite(a00.y()) || !std::isfinite(a00.z())
                            || !std::isfinite(a01.x()) || !std::isfinite(a01.y()) || !std::isfinite(a01.z())
                            || !std::isfinite(a10.x()) || !std::isfinite(a10.y()) || !std::isfinite(a10.z())
                            || !std::isfinite(a11.x()) || !std::isfinite(a11.y()) || !std::isfinite(a11.z()))
                            continue;
                        prim.cartIndices << i00 << i10 << i01;
                        prim.cartIndices << i01 << i10 << i11;
                    }
                }
            }
            break;
        }
        }
    }

    // 算完就把这些段标为有效；下一帧若内容没变，它们会被整体跳过。
    for (QChartSegment* s : todo) s->cartValid = true;
    QCHART_DIRTY_MSG(QStringLiteral("  transform: 重算 %1 段 / 跳过 %2 段（valid）")
                         .arg(todo.size()).arg(skipped));
}

// 步骤 2b：裁剪 + 标签解析
// （tier1 显式锚点可见性判定 = 基类 QChartRenderer::anchorVisibleInPlotArea，两后端共用单一实现）

void QPainterChartRenderer::cullAndResolveLabels(QChartScene& scene)
{
    const QChartAbstractCamera* camera = scene.camera;

    // 逐段：段内先裁剪（只标 false、不删），再解析本段标签（契约 S6/S8）
    auto resolveSegment = [&](QChartSegment& seg) {
        int lastVisible = -1;
        for (int i = 0; i < seg.prims.size(); ++i) {
            QChartPrimitive& prim = seg.prims[i];
            // 契约 S6②：视口裁剪只在这里**标 false**，绝不从段里删元素。
            prim.visible = isPrimitiveVisible(prim, camera);
            // 本段最后一个可见图元 = 自由标签的锚点（段内相对位置承载语义）
            if (prim.visible) lastVisible = i;
        }

        for (QChartTextLabel& label : seg.labels) {
            // tier1：显式锚点（numericAnchor 任一分量非 NaN）
            if (label.hasExplicitAnchor()) {
                if (!scene.projection) { label.visible = false; continue; }
                label.cartesianAnchor = scene.projection->toCartesian(label.numericAnchor);
                label.visible = anchorVisibleInPlotArea(scene, label.cartesianAnchor);
                continue;
            }

            // tier2：段内绑定 → 继承该图元的组尾锚点与可见性
            if (label.segPrim >= 0) {
                if (label.segPrim < seg.prims.size()) {
                    label.cartesianAnchor = qchartGroupTailAnchorCartesian(seg.prims[label.segPrim]);
                    label.visible = seg.prims[label.segPrim].visible;
                } else {
                    label.visible = false;   // 越界绑定：无对应图元 → 不可见
                }
                continue;
            }

            // tier3：自由标签 → 本段最后一个可见图元的组尾锚点
            if (label.segPrim == -1) {
                if (lastVisible >= 0) {
                    label.cartesianAnchor = qchartGroupTailAnchorCartesian(seg.prims[lastVisible]);
                    label.visible = true;
                    continue;
                }
                label.visible = false;   // 本段无可见图元
                continue;
            }

            label.visible = false;       // 非法绑定值（segPrim < -1）
        }
    };

    for (QChartSegment& seg : scene.axisSegments) resolveSegment(seg);
    for (QChartSegment& seg : scene.seriesSegments) resolveSegment(seg);
}

// 步骤 3：图元绘制

void QPainterChartRenderer::drawPrimitives(QChartScene& scene,
                                           QPaintDevice* device)
{
    if (!device) return;

    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    QPainter painter(device);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(scene.plotArea);

    // 根据相机类型选择映射方式（2D/3D 批次 B1 双路径）
    if (const QChartCamera* cam2d = dynamic_cast<const QChartCamera*>(camera)) {
        drawPrimitives2D(painter, scene, cam2d);
    } else if (const QChartCamera3D* cam3d = dynamic_cast<const QChartCamera3D*>(camera)) {
        drawPrimitives3D(painter, scene, cam3d);
    } else {
        qWarning() << "QPainterChartRenderer: 未知相机类型";
    }
}

void QPainterChartRenderer::drawPrimitives2D(QPainter& painter,
                                             const QChartScene& scene,
                                             const QChartCamera* cam2d)
{
    const QRectF& plotArea = scene.plotArea;

    // 段序展开：先轴段，后系列段；段内保持图元顺序（契约 S1）
    QVector<const QChartPrimitive*> flat;
    for (const QChartSegment& s : scene.axisSegments)
        for (const QChartPrimitive& p : s.prims) flat.append(&p);
    for (const QChartSegment& s : scene.seriesSegments)
        for (const QChartPrimitive& p : s.prims) flat.append(&p);

    for (const QChartPrimitive* pp : flat) {
        const QChartPrimitive& prim = *pp;
        if (!prim.visible) continue;   // 契约 S6②：可见性读图元自身

        QPointF pixelA = cam2d->project(QVector3D(prim.cartA.x(), prim.cartA.y(), 0), plotArea).screen;
        painter.setPen(QPen(prim.color, prim.penWidth));

        switch (prim.type) {
        case QChartPrimitive::Type::Point: {
            painter.setBrush(prim.color);
            qreal r = prim.markerSize * 0.5;
            painter.drawEllipse(pixelA, r, r);
            break;
        }
        case QChartPrimitive::Type::Rect: {
            // ★ 左上角必须从 cartRect 自己算。上面的 pixelA 来自 cartA，而 Rect/Ellipse 的变换
            //   **从不写 cartA**（只写 cartRect），吃 pixelA 会把每个矩形画成
            //   "从 Cartesian 原点到自己右下角" —— 柱状图糊成一整块阶梯（老 bug）。
            QPointF pixelTL = cam2d->project(QVector3D(prim.cartRect.left(), prim.cartRect.top(), 0), plotArea).screen;
            QPointF pixelBR = cam2d->project(QVector3D(prim.cartRect.right(), prim.cartRect.bottom(), 0), plotArea).screen;
            painter.setBrush(prim.fillColor);
            painter.drawRect(QRectF(pixelTL, pixelBR));
            break;
        }
        case QChartPrimitive::Type::Ellipse: {
            QPointF pixelTL = cam2d->project(QVector3D(prim.cartRect.left(), prim.cartRect.top(), 0), plotArea).screen;
            QPointF pixelBR = cam2d->project(QVector3D(prim.cartRect.right(), prim.cartRect.bottom(), 0), plotArea).screen;
            painter.setBrush(prim.fillColor);
            painter.drawEllipse(QRectF(pixelTL, pixelBR));
            break;
        }
        case QChartPrimitive::Type::Polygon: {
            if (prim.cartVerts.isEmpty()) break;
            QPolygonF poly;
            poly.reserve(prim.cartVerts.size());
            for (const QVector3D& v : prim.cartVerts) {
                poly.append(cam2d->project(v, plotArea).screen);
            }
            painter.setBrush(prim.fillColor);
            painter.drawPolygon(poly);
            break;
        }
        case QChartPrimitive::Type::Path: {
            if (prim.cartVerts.isEmpty()) break;
            QPolygonF poly;
            poly.reserve(prim.cartVerts.size());
            for (const QVector3D& v : prim.cartVerts) {
                poly.append(cam2d->project(v, plotArea).screen);
            }
            painter.setBrush(Qt::NoBrush);
            painter.drawPolyline(poly);
            break;
        }
        case QChartPrimitive::Type::TriangleMesh:
        case QChartPrimitive::Type::TriangleFan:
        case QChartPrimitive::Type::TriangleStrip:
        case QChartPrimitive::Type::Grid: {
            if (prim.cartVerts.isEmpty()) break;
            painter.setBrush(prim.fillColor);
            // 简单实现：按三角形绘制
            const auto& verts = prim.cartVerts;
            if ((prim.type == QChartPrimitive::Type::TriangleMesh
                 || prim.type == QChartPrimitive::Type::Grid)
                && !prim.cartIndices.isEmpty()) {
                for (int j = 0; j < prim.cartIndices.size(); j += 3) {
                    if (j + 2 >= prim.cartIndices.size()) break;
                    QPolygonF tri;
                    tri.append(cam2d->project(QVector3D(verts[prim.cartIndices[j]].x(), verts[prim.cartIndices[j]].y(), 0), plotArea).screen);
                    tri.append(cam2d->project(QVector3D(verts[prim.cartIndices[j+1]].x(), verts[prim.cartIndices[j+1]].y(), 0), plotArea).screen);
                    tri.append(cam2d->project(QVector3D(verts[prim.cartIndices[j+2]].x(), verts[prim.cartIndices[j+2]].y(), 0), plotArea).screen);
                    painter.drawPolygon(tri);
                }
            } else {
                // TriangleFan / TriangleStrip: 每3个连续顶点构成一个三角形
                for (int j = 0; j < verts.size() - 2; ++j) {
                    QPolygonF tri;
                    tri.append(cam2d->project(QVector3D(verts[j].x(),   verts[j].y(), 0), plotArea).screen);
                    tri.append(cam2d->project(QVector3D(verts[j+1].x(), verts[j+1].y(), 0), plotArea).screen);
                    tri.append(cam2d->project(QVector3D(verts[j+2].x(), verts[j+2].y(), 0), plotArea).screen);
                    painter.drawPolygon(tri);
                }
            }
            break;
        }
        }
    }
}

// 批次 B1 恢复：drawPrimitives3D（3D 数据主脊/盒边/网格，painter's algorithm 深度排序）
void QPainterChartRenderer::drawPrimitives3D(QPainter& painter,
                                             QChartScene& scene,
                                             const QChartCamera3D* cam3d) {
    const QRectF& plotArea = scene.plotArea;

    // 1. 收集可见图元（段序展开成扁平表；深度排序只排这张表，不写回 scene）
    QVector<QChartPrimitive*> flat;
    for (QChartSegment& s : scene.axisSegments)
        for (QChartPrimitive& p : s.prims) if (p.visible) flat.append(&p);
    for (QChartSegment& s : scene.seriesSegments)
        for (QChartPrimitive& p : s.prims) if (p.visible) flat.append(&p);

    // 1b. 逐图元算视图深度（只放在局部表里，不往 scene 上写任何东西）
    QVector<QPair<qreal, const QChartPrimitive*>> ordered;
    ordered.reserve(flat.size());
    for (const QChartPrimitive* prim : flat) {
        qreal depth = 0;
        switch (prim->type) {
        case QChartPrimitive::Type::Point:
            depth = cam3d->project(prim->cartA, plotArea).depth;
            break;
        case QChartPrimitive::Type::Rect:
        case QChartPrimitive::Type::Ellipse:
            break;
        case QChartPrimitive::Type::Polygon:
        case QChartPrimitive::Type::Path:
        case QChartPrimitive::Type::TriangleMesh:
        case QChartPrimitive::Type::TriangleFan:
        case QChartPrimitive::Type::TriangleStrip:
        case QChartPrimitive::Type::Grid:
            depth = cam3d->project(QCube(prim->cartVerts).center(), plotArea).depth;
            break;
        }
        ordered.append(qMakePair(depth, prim));
    }

    // 2. 按 depth 降序排序（远→近）= painter's algorithm
    std::sort(ordered.begin(), ordered.end(),
        [](const QPair<qreal, const QChartPrimitive*>& a,
           const QPair<qreal, const QChartPrimitive*>& b) {
            return a.first > b.first;
        });

    for (const QPair<qreal, const QChartPrimitive*>& item : ordered) {
        const QChartPrimitive& prim = *item.second;
        if (!prim.visible) continue;

        QChartProjectedPoint ppA = cam3d->project(prim.cartA, plotArea);
        if (!std::isfinite(ppA.screen.x()) || !std::isfinite(ppA.screen.y())) continue;

        painter.setPen(QPen(prim.color, prim.penWidth));

        switch (prim.type) {
        case QChartPrimitive::Type::Point: {
            painter.setBrush(prim.color);
            qreal r = prim.markerSize * 0.5;
            painter.drawEllipse(ppA.screen, r, r);
            break;
        }
        case QChartPrimitive::Type::Rect: {
            QPointF tl = cam3d->project(QVector3D(prim.cartRect.left(), prim.cartRect.top(), 0), plotArea).screen;
            QPointF br = cam3d->project(QVector3D(prim.cartRect.right(), prim.cartRect.bottom(), 0), plotArea).screen;
            painter.setBrush(prim.fillColor);
            painter.drawRect(QRectF(tl, br));
            break;
        }
        case QChartPrimitive::Type::Ellipse: {
            QPointF tl = cam3d->project(QVector3D(prim.cartRect.left(), prim.cartRect.top(), 0), plotArea).screen;
            QPointF br = cam3d->project(QVector3D(prim.cartRect.right(), prim.cartRect.bottom(), 0), plotArea).screen;
            painter.setBrush(prim.fillColor);
            painter.drawEllipse(QRectF(tl, br));
            break;
        }
        case QChartPrimitive::Type::Polygon: {
            if (prim.cartVerts.isEmpty()) break;
            QPolygonF poly;
            poly.reserve(prim.cartVerts.size());
            for (const QVector3D& v : prim.cartVerts) {
                QPointF p = cam3d->project(v, plotArea).screen;
                if (std::isfinite(p.x()) && std::isfinite(p.y()))
                    poly.append(p);
            }
            if (poly.size() >= 3) {
                painter.setBrush(prim.fillColor);
                painter.drawPolygon(poly);
            }
            break;
        }
        case QChartPrimitive::Type::Path: {
            if (prim.cartVerts.isEmpty()) break;
            QPolygonF poly;
            poly.reserve(prim.cartVerts.size());
            for (const QVector3D& v : prim.cartVerts) {
                QPointF p = cam3d->project(v, plotArea).screen;
                if (std::isfinite(p.x()) && std::isfinite(p.y()))
                    poly.append(p);
            }
            if (poly.size() >= 2) {
                painter.setBrush(Qt::NoBrush);
                painter.drawPolyline(poly);
            }
            break;
        }
        case QChartPrimitive::Type::TriangleMesh:
        case QChartPrimitive::Type::TriangleFan:
        case QChartPrimitive::Type::TriangleStrip:
        case QChartPrimitive::Type::Grid: {
            if (prim.cartVerts.isEmpty()) break;
            painter.setBrush(prim.fillColor);
            const auto& verts = prim.cartVerts;
            auto project = [&](const QVector3D& v) -> QPointF {
                return cam3d->project(v, plotArea).screen;
            };
            if ((prim.type == QChartPrimitive::Type::TriangleMesh
                 || prim.type == QChartPrimitive::Type::Grid)
                && !prim.cartIndices.isEmpty()) {
                for (int j = 0; j < prim.cartIndices.size(); j += 3) {
                    if (j + 2 >= prim.cartIndices.size()) break;
                    QPolygonF tri;
                    tri.append(project(verts[prim.cartIndices[j]]));
                    tri.append(project(verts[prim.cartIndices[j+1]]));
                    tri.append(project(verts[prim.cartIndices[j+2]]));
                    painter.drawPolygon(tri);
                }
            } else {
                for (int j = 0; j < verts.size() - 2; ++j) {
                    QPolygonF tri;
                    tri.append(project(verts[j]));
                    tri.append(project(verts[j+1]));
                    tri.append(project(verts[j+2]));
                    painter.drawPolygon(tri);
                }
            }
            break;
        }
        }
    }
}
// —— drawPrimitives3D 结束（批次 B1 恢复）

// 步骤 4：标签绘制

void QPainterChartRenderer::drawLabels(QChartScene& scene, QPaintDevice* device)
{
    if (!device) return;

    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    QPainter painter(device);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(scene.plotArea);

    // 根据相机类型选择映射方式（与 drawPrimitives 同构；2D/3D 双路径）
    if (const QChartCamera* cam2d = dynamic_cast<const QChartCamera*>(camera)) {
        drawLabels2D(painter, scene, cam2d);
    } else if (const QChartCamera3D* cam3d = dynamic_cast<const QChartCamera3D*>(camera)) {
        drawLabels3D(painter, scene, cam3d);
    } else {
        qWarning() << "QPainterChartRenderer: 未知相机类型";
    }
}

void QPainterChartRenderer::drawLabels2D(QPainter& painter,
                                         const QChartScene& scene,
                                         const QChartCamera* cam2d)
{
    const QRectF& plotArea = scene.plotArea;

    // 标签按段收集（段序 = 绘制顺序）
    QVector<const QChartTextLabel*> flat;
    for (const QChartSegment& s : scene.axisSegments)
        for (const QChartTextLabel& l : s.labels) flat.append(&l);
    for (const QChartSegment& s : scene.seriesSegments)
        for (const QChartTextLabel& l : s.labels) flat.append(&l);

    for (const QChartTextLabel* lp : flat) {
        const QChartTextLabel& label = *lp;
        if (!label.visible) continue;

        QChartProjectedPoint pp = cam2d->project(label.cartesianAnchor, plotArea);

        // t73②（★二维 CPU 生效路径）：二次包含判定**只对 tier1（显式锚点）保留**——
        // drawLabel 会把文字框钳制进绘图区，不判定则视图外的显式标签会被"挤"到边缘说谎。
        // tier2/tier3 的可见性由**同帧**裁剪结果导出（visibility[] / 同组组尾可见性），此处再判一次
        // 会因锚点恰落边界（实测 700.000018 vs plotArea.right()=700、67.999988 vs left=68，
        // 1e-5~2e-5 px 级浮点噪声）把整组网格标签丢掉（t70 §3a，用户可见"时横时纵整组消失"）。
        // 用户裁定：不加容差（fit 已保证视图内元素落在绘图区内，不做容差式绕过）。
        if (label.hasExplicitAnchor() && !plotArea.contains(pp.screen)) continue;

        drawLabel(painter, plotArea, pp.screen, label.text, label.color,
                  label.fontSize, label.alignment);
    }
}

// 批次 B1 恢复：drawLabels3D（3D 标签：非有限投影/plotArea 外跳过）
// 注：3D 标签锚点来自图元自身（tier2），本次 t73 只改二维 CPU 路径（任务边界），3D 判定保持原样。
void QPainterChartRenderer::drawLabels3D(QPainter& painter,
                                         const QChartScene& scene,
                                         const QChartCamera3D* cam3d)
{
    const QRectF& plotArea = scene.plotArea;

    QVector<const QChartTextLabel*> flat;
    for (const QChartSegment& s : scene.axisSegments)
        for (const QChartTextLabel& l : s.labels) flat.append(&l);
    for (const QChartSegment& s : scene.seriesSegments)
        for (const QChartTextLabel& l : s.labels) flat.append(&l);

    for (const QChartTextLabel* lp : flat) {
        const QChartTextLabel& label = *lp;
        if (!label.visible) continue;

        QChartProjectedPoint pp = cam3d->project(label.cartesianAnchor, plotArea);
        if (!std::isfinite(pp.screen.x()) || !std::isfinite(pp.screen.y())) continue;
        if (!plotArea.contains(pp.screen)) continue;

        drawLabel(painter, plotArea, pp.screen, label.text, label.color,
                  label.fontSize, label.alignment);
    }
}

// 裁剪辅助函数

bool QPainterChartRenderer::isPrimitiveVisible(const QChartPrimitive& prim,
                                                const QChartAbstractCamera* camera) const
{
    if (!camera) return true;

    if (const QChartCamera* cam2d = dynamic_cast<const QChartCamera*>(camera)) {
        return isPrimitiveVisible2D(prim, cam2d->viewRect());
    }
    if (const QChartCamera3D* cam3d = dynamic_cast<const QChartCamera3D*>(camera)) {
        return isPrimitiveVisible3D(prim, cam3d->viewCube());
    }
    return true;
}

bool QPainterChartRenderer::isPrimitiveVisible2D(const QChartPrimitive& prim, const QRectF& viewRect) const
{
    switch (prim.type) {
    case QChartPrimitive::Type::Point:
        return viewRect.contains(prim.cartA.x(), prim.cartA.y());

    case QChartPrimitive::Type::Rect:
    case QChartPrimitive::Type::Ellipse:
        return viewRect.intersects(prim.cartRect);

    case QChartPrimitive::Type::Polygon:
    case QChartPrimitive::Type::Path:
    case QChartPrimitive::Type::TriangleMesh:
    case QChartPrimitive::Type::TriangleFan:
    case QChartPrimitive::Type::TriangleStrip:
    case QChartPrimitive::Type::Grid: {
        if (prim.cartVerts.isEmpty()) return false;
        qreal minX = prim.cartVerts[0].x(), maxX = minX;
        qreal minY = prim.cartVerts[0].y(), maxY = minY;
        for (const auto& v : prim.cartVerts) {
            minX = qMin(minX, v.x()); maxX = qMax(maxX, v.x());
            minY = qMin(minY, v.y()); maxY = qMax(maxY, v.y());
        }
        // 退化盒（水平/垂直轴线，宽或高为 0）：QRectF::intersects 对零面积盒返回 false，
        // 会误裁掉轴脊/网格线 → 微扩 1e-6 后判定（纯裁剪修正，不改几何）。
        QRectF bbox(minX, minY, maxX - minX, maxY - minY);
        if (bbox.width() <= 0.0 || bbox.height() <= 0.0)
            bbox.adjust(-1e-6, -1e-6, 1e-6, 1e-6);
        return viewRect.intersects(bbox);
    }
    default:
        return true;
    }
}

// 批次 B1 恢复：isPrimitiveVisible3D（图元包围盒 × viewCube 相交/含括）
bool QPainterChartRenderer::isPrimitiveVisible3D(const QChartPrimitive& prim, const QCube& viewCube) const
{
    // 使用 QCube 工具类的 intersects 方法
    // 先为图元构建一个包围盒
    switch (prim.type) {
    case QChartPrimitive::Type::Point:
        return viewCube.contains(prim.cartA);

    case QChartPrimitive::Type::Rect:
    case QChartPrimitive::Type::Ellipse: {
        qWarning() << "QPainterChartRenderer: isPrimitiveVisible3D: Rect/Ellipse 3D 裁剪未实现，使用包围盒近似";
        // Rect/Ellipse 在 z=0 平面，构建薄片包围盒
        QVector3D min(prim.cartRect.left(), prim.cartRect.top(), 0);
        QVector3D max(prim.cartRect.right(), prim.cartRect.bottom(), 0);
        QCube rectCube(min, max);
        return viewCube.intersects(rectCube);
    }

    case QChartPrimitive::Type::Polygon:
    case QChartPrimitive::Type::Path:
    case QChartPrimitive::Type::TriangleMesh:
    case QChartPrimitive::Type::TriangleFan:
    case QChartPrimitive::Type::TriangleStrip:
    case QChartPrimitive::Type::Grid: {
        if (prim.cartVerts.isEmpty()) return false;
        // 构建 AABB
        qreal minX = prim.cartVerts[0].x(), maxX = minX;
        qreal minY = prim.cartVerts[0].y(), maxY = minY;
        qreal minZ = prim.cartVerts[0].z(), maxZ = minZ;
        for (const auto& v : prim.cartVerts) {
            minX = qMin(minX, v.x()); maxX = qMax(maxX, v.x());
            minY = qMin(minY, v.y()); maxY = qMax(maxY, v.y());
            minZ = qMin(minZ, v.z()); maxZ = qMax(maxZ, v.z());
        }
        QCube aabb(QVector3D(minX, minY, minZ), QVector3D(maxX, maxY, maxZ));
        return viewCube.intersects(aabb);
    }
    default:
        return true;
    }
}
// —— isPrimitiveVisible3D 结束（批次 B1 恢复）