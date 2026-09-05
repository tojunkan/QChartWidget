// QPainterChartRenderer.cpp
#include "QPainterChartRenderer.h"
#include "QChartAbstractProjection.h"
#include "QChartCamera.h"
#include "QChartCamera3D.h"
#include "QCube.h"
#include <QPainter>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logPainter, "chart.render.painter")

// 步骤 2a：Numeric → Cartesian 变换

void QPainterChartRenderer::transformNumericToCartesian(QChartScene& scene)
{
    const QChartAbstractProjection* proj = scene.projection;
    if (!proj) return;

    const bool isIdentity = proj->isIdentityMapping();

    for (QChartPrimitive& prim : scene.primitives) {
        switch (prim.type) {
        case QChartPrimitive::Type::Point:
            prim.cartA = proj->toCartesian(prim.numA);
            break;

        case QChartPrimitive::Type::Line:
            prim.cartA = proj->toCartesian(prim.numA);
            prim.cartB = proj->toCartesian(prim.numB);
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

        case QChartPrimitive::Type::Polygon:
        case QChartPrimitive::Type::Path:
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
        }
    }
}

// 步骤 2b：裁剪 + 标签解析

void QPainterChartRenderer::cullAndResolveLabels(QChartScene& scene)
{
    const int N = scene.primitives.size();
    QVector<bool>& visibility = m_visibilityCache;
    visibility.resize(N);

    const QChartAbstractCamera* camera = scene.camera;

    QVector<int> lastVisibleIndex(scene.maxSourceId + 1, -1);

    for (int i = 0; i < N; ++i) {
        const QChartPrimitive& prim = scene.primitives[i];
        bool visible = isPrimitiveVisible(prim, camera);
        visibility[i] = visible;
        if (visible) {
            lastVisibleIndex[prim.sourceId] = i;
        }
    }

    // 绑定标签
    for (QChartTextLabel& label : scene.labels) {
        if (label.refPrimitiveId >= 0 && label.refPrimitiveId < N) {
            const QChartPrimitive& prim = scene.primitives[label.refPrimitiveId];
            label.cartesianAnchor = prim.cartA;
            label.visible = visibility[label.refPrimitiveId];
        }
    }

    // 自由标签
    for (QChartTextLabel& label : scene.labels) {
        if (label.refPrimitiveId != -1) continue;
        int sid = label.sourceId;
        if (sid >= 0 && sid < lastVisibleIndex.size()) {
            int idx = lastVisibleIndex[sid];
            if (idx != -1) {
                label.cartesianAnchor = scene.primitives[idx].cartA;
                label.visible = true;
                continue;
            }
        }
        label.visible = false;
    }
}

// 步骤 3：图元绘制

void QPainterChartRenderer::drawPrimitives(QChartScene& scene,
                                           QPaintDevice* device,
                                           const QVector<bool>& visibility)
{
    if (!device) return;

    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    QPainter painter(device);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(scene.plotArea);

    // 根据相机类型选择映射方式
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
    auto& visibility = m_visibilityCache;

    for (int i = 0; i < scene.primitives.size(); ++i) {
        if (!visibility[i]) continue;
        const QChartPrimitive& prim = scene.primitives[i];

        QPointF pixelA = cam2d->project(QVector3D(prim.cartA.x(), prim.cartA.y(), 0), plotArea).screen;
        painter.setPen(QPen(prim.color, prim.penWidth));

        switch (prim.type) {
        case QChartPrimitive::Type::Point: {
            painter.setBrush(prim.color);
            qreal r = prim.markerSize * 0.5;
            painter.drawEllipse(pixelA, r, r);
            break;
        }
        case QChartPrimitive::Type::Line: {
            QPointF pixelB = cam2d->project(QVector3D(prim.cartB.x(), prim.cartB.y(), 0), plotArea).screen;
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(pixelA, pixelB);
            break;
        }
        case QChartPrimitive::Type::Rect: {
            QPointF pixelBR = cam2d->project(QVector3D(prim.cartRect.right(), prim.cartRect.bottom(), 0), plotArea).screen;
            QRectF pixelRect(pixelA, pixelBR);
            painter.setBrush(prim.fillColor);
            painter.drawRect(pixelRect);
            break;
        }
        case QChartPrimitive::Type::Ellipse: {
            QPointF pixelBR = cam2d->project(QVector3D(prim.cartRect.right(), prim.cartRect.bottom(), 0), plotArea).screen;
            QRectF pixelRect(pixelA, pixelBR);
            painter.setBrush(prim.fillColor);
            painter.drawEllipse(pixelRect);
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
        case QChartPrimitive::Type::TriangleStrip: {
            if (prim.cartVerts.isEmpty()) break;
            painter.setBrush(prim.fillColor);
            // 简单实现：按三角形绘制
            const auto& verts = prim.cartVerts;
            if (prim.type == QChartPrimitive::Type::TriangleMesh && !prim.cartIndices.isEmpty()) {
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

void QPainterChartRenderer::drawPrimitives3D(QPainter& painter,
                                             const QChartScene& scene,
                                             const QChartCamera3D* cam3d) {
    const QRectF& plotArea = scene.plotArea;
    auto& visibility = m_visibilityCache;

        // 1. 收集可见图元的索引
    QVector<int> sortedIndices(scene.primitives.size());
    int cnt = 0;
    for (int i = 0; i < scene.primitives.size(); ++i) {
        if (m_visibilityCache[i]) sortedIndices[cnt++] = i;
    }
    sortedIndices.resize(cnt);

    for (auto i : sortedIndices) {
        QChartPrimitive& prim = scene.primitives[i];
        qreal depth;
        switch (prim.type) {
        case QChartPrimitive::Type::Point:
            depth = cam3d->project(prim.cartA, plotArea).depth;
            break;
        case QChartPrimitive::Type::Line:
            depth = cam3d->project((prim.cartA + prim.cartB) * 0.5, plotArea).depth;
            break;
        case QChartPrimitive::Type::Rect:
        case QChartPrimitive::Type::Ellipse:
            break;
        case QChartPrimitive::Type::Polygon:
        case QChartPrimitive::Type::Path:
            depth = cam3d->project(QCube(prim.cartVerts).center(), plotArea).depth;
            break;
        case QChartPrimitive::Type::TriangleMesh:
        case QChartPrimitive::Type::TriangleFan:
        case QChartPrimitive::Type::TriangleStrip:
            depth = cam3d->project(QCube(prim.cartVerts).center(), plotArea).depth;
            break;
        }
        prim.depth = depth;
    }

    // 2. 按 depth 降序排序（远→近）
    std::sort(sortedIndices.begin(), sortedIndices.end(),
        [&](int a, int b) {
            // 注意：这里的 depth 应该是相机空间下的视图深度（-viewZ）
            // 你可以从 prim.cartA 实时计算，或使用预先算好的 prim.depth
            return scene.primitives[a].depth > scene.primitives[b].depth;
        });

    for (auto i : sortedIndices) {
        if (!visibility[i]) continue;
        const QChartPrimitive& prim = scene.primitives[i];

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
        case QChartPrimitive::Type::Line: {
            QChartProjectedPoint ppB = cam3d->project(prim.cartB, plotArea);
            if (!std::isfinite(ppB.screen.x()) || !std::isfinite(ppB.screen.y())) break;
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(ppA.screen, ppB.screen);

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
        case QChartPrimitive::Type::TriangleStrip: {
            if (prim.cartVerts.isEmpty()) break;
            painter.setBrush(prim.fillColor);
            const auto& verts = prim.cartVerts;
            auto project = [&](const QVector3D& v) -> QPointF {
                return cam3d->project(v, plotArea).screen;
            };
            if (prim.type == QChartPrimitive::Type::TriangleMesh && !prim.cartIndices.isEmpty()) {
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

// 步骤 4：标签绘制

// 裁剪辅助函数（使用 QCube 工具类）

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

    case QChartPrimitive::Type::Line: {
        const QPointF a(prim.cartA.x(), prim.cartA.y());
        const QPointF b(prim.cartB.x(), prim.cartB.y());
        if (viewRect.contains(a) || viewRect.contains(b)) return true;
        QLineF line(a, b);
        QRectF rect = viewRect;
        QLineF edges[4] = {
            QLineF(rect.topLeft(), rect.topRight()),
            QLineF(rect.topRight(), rect.bottomRight()),
            QLineF(rect.bottomRight(), rect.bottomLeft()),
            QLineF(rect.bottomLeft(), rect.topLeft())
        };
        for (const auto& edge : edges) {
            QPointF inter;
            if (line.intersects(edge, &inter) == QLineF::BoundedIntersection)
                return true;
        }
        return false;
    }

    case QChartPrimitive::Type::Rect:
    case QChartPrimitive::Type::Ellipse:
        return viewRect.intersects(prim.cartRect);

    case QChartPrimitive::Type::Polygon:
    case QChartPrimitive::Type::Path:
    case QChartPrimitive::Type::TriangleMesh:
    case QChartPrimitive::Type::TriangleFan:
    case QChartPrimitive::Type::TriangleStrip: {
        if (prim.cartVerts.isEmpty()) return false;
        qreal minX = prim.cartVerts[0].x(), maxX = minX;
        qreal minY = prim.cartVerts[0].y(), maxY = minY;
        for (const auto& v : prim.cartVerts) {
            minX = qMin(minX, v.x()); maxX = qMax(maxX, v.x());
            minY = qMin(minY, v.y()); maxY = qMax(maxY, v.y());
        }
        return viewRect.intersects(QRectF(minX, minY, maxX - minX, maxY - minY));
    }
    default:
        return true;
    }
}

bool QPainterChartRenderer::isPrimitiveVisible3D(const QChartPrimitive& prim, const QCube& viewCube) const
{
    // 使用 QCube 工具类的 intersects 方法
    // 先为图元构建一个包围盒
    switch (prim.type) {
    case QChartPrimitive::Type::Point:
        return viewCube.contains(prim.cartA);

    case QChartPrimitive::Type::Line: {
        // 用线段两端点形成包围盒
        QCube lineCube(prim.cartA, prim.cartB);
        return viewCube.intersects(lineCube);
    }

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
    case QChartPrimitive::Type::TriangleStrip: {
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