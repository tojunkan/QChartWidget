// QChartLayer3D.cpp —— 3D 图层实现
#include "QChartLayer3D.h"
#include "QChartCamera3D.h"
#include "QChartProjection3D.h"
#include "QChartSurfaceSeries.h"
#include "QChartRenderer.h"   // QChartPrimitive
#include <cmath>

QChartLayer3D::QChartLayer3D(QObject* parent) : QChartLayer(parent) {}

// ===== 轴 =====
void QChartLayer3D::setAxisZ(QChartAxis* a) { m_axisZ = a; }

// ===== 3D 系列管理 =====
void QChartLayer3D::addSeries3D(QChartSeries3D* s) {
    if (!s || m_series3D.contains(s)) return;
    m_series3D.append(s);
    addSeries(s);   // 存入基类 m_series（复用图例/主题/所有权/析构）
}

void QChartLayer3D::removeSeries3D(QChartSeries3D* s) {
    if (!s) return;
    m_series3D.removeOne(s);
    removeSeries(s);
}

// ===== 网格地板 =====
void QChartLayer3D::setGridFloorVisible(bool v) { m_gridFloorVisible = v; }
void QChartLayer3D::setGridFloorHalfSize(qreal half) { m_gridFloorHalf = half; }

// ===== 全链闭包：Data → {screen, depth} =====
ProjectFn3D QChartLayer3D::makeProjectFn(const QChartCamera3D* cam,
                                         const QRectF& plotArea) const {
    return [this, cam, plotArea](const QDataPoint3D& d) -> QChartProjectedPoint {
        qreal n0 = m_axisX ? m_axisX->toNumeric(d.x()) : d.x().toDouble();
        qreal n1 = m_axisY ? m_axisY->toNumeric(d.y()) : d.y().toDouble();
        qreal n2 = m_axisZ ? m_axisZ->toNumeric(d.z()) : d.z().toDouble();
        const QVector3D world = m_projection3D
            ? m_projection3D->toWorld(n0, n1, n2)
            : QVector3D(n0, n1, n2);
        if (!cam) return QChartProjectedPoint{ QPointF(qQNaN(), qQNaN()), 0.0 };
        return cam->project(world, plotArea);
    };
}

// ===== 图元收集 =====
void QChartLayer3D::collectPrimitives(const QChartCamera3D* cam, const QRectF& plotArea,
                                      QVector<QChartPrimitive>& out) const {
    if (!cam) return;

    // 1. 曲面 worldCache 直算填充（自身 axis toNumeric + projection3D toWorld，不走系列闭包，裁决 b）
    for (QChartSeries3D* s : m_series3D) {
        auto* surf = qobject_cast<QChartSurfaceSeries*>(s);
        if (!surf) continue;
        QVector<QVector3D>& cache = surf->worldCache();
        cache.resize(surf->count());
        for (int i = 0; i < surf->count(); ++i) {
            const QDataPoint3D d = surf->points().at(i);
            qreal n0 = m_axisX ? m_axisX->toNumeric(d.x()) : d.x().toDouble();
            qreal n1 = m_axisY ? m_axisY->toNumeric(d.y()) : d.y().toDouble();
            qreal n2 = m_axisZ ? m_axisZ->toNumeric(d.z()) : d.z().toDouble();
            cache[i] = m_projection3D
                ? m_projection3D->toWorld(n0, n1, n2)
                : QVector3D(n0, n1, n2);
        }
    }

    // 2. 3D 系列图元（全链闭包）
    const ProjectFn3D fn = makeProjectFn(cam, plotArea);
    for (QChartSeries3D* s : m_series3D) {
        if (!s || !s->isVisible()) continue;
        s->collectPrimitives(fn, out);
    }

    // 3. 辅助网格地板（y=0 平面；dataIndex=-1；depth 经 camera 直算）
    if (!m_gridFloorVisible) return;
    const qreal half = (m_gridFloorHalf > 0.0) ? m_gridFloorHalf : 10.0;
    const int divisions = 10;
    auto emitFloorLine = [&](const QVector3D& w0, const QVector3D& w1) {
        const QChartProjectedPoint p0 = cam->project(w0, plotArea);
        const QChartProjectedPoint p1 = cam->project(w1, plotArea);
        if (!std::isfinite(p0.screen.x()) || !std::isfinite(p0.screen.y()) ||
            !std::isfinite(p1.screen.x()) || !std::isfinite(p1.screen.y()))
            return;
        QChartPrimitive prim;
        prim.type = QChartPrimitive::Type::LineSegment;
        prim.a = p0.screen;
        prim.b = p1.screen;
        prim.depth = (p0.depth + p1.depth) * 0.5;
        prim.dataIndex = -1;                    // 网格地板无数据点索引
        prim.penWidth = 1.0;
        prim.color = QColor(150, 150, 150, 160);
        out.append(prim);
    };
    for (int i = 0; i <= divisions; ++i) {
        const qreal t = -half + 2.0 * half * i / divisions;
        emitFloorLine(QVector3D(-half, 0, t), QVector3D(half, 0, t));   // 沿 x
        emitFloorLine(QVector3D(t, 0, -half), QVector3D(t, 0, half));   // 沿 z
    }
}
