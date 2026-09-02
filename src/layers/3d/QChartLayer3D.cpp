// QChartLayer3D.cpp
#include "QChartLayer3D.h"
#include "QChartCamera3D.h"
#include "QChartProjection3D.h"
#include "QChartSurfaceSeries.h"
#include "QChartRenderer.h"
#include <cmath>

QChartLayer3D::QChartLayer3D(QObject* parent)
    : QChartLayer(parent)
{
    m_axes3D = std::make_unique<QChartAxes3D>();
    m_axes3D->axis(0).axis = m_axisX;
    m_axes3D->axis(1).axis = m_axisY;
    m_axes3D->axis(2).axis = m_axisZ;
}

// ===== 轴重绑 =====
void QChartLayer3D::setAxisX(QChartAxis* a) {
    QChartLayer::setAxisX(a);
    if (m_axes3D) m_axes3D->axis(0).axis = a;
    m_worldCacheDirty = true;
}
void QChartLayer3D::setAxisY(QChartAxis* a) {
    QChartLayer::setAxisY(a);
    if (m_axes3D) m_axes3D->axis(1).axis = a;
    m_worldCacheDirty = true;
}
void QChartLayer3D::setAxisZ(QChartAxis* a) {
    m_axisZ = a;
    if (m_axes3D) m_axes3D->axis(2).axis = a;
    m_worldCacheDirty = true;
}

// ===== 系列管理 =====
void QChartLayer3D::addSeries3D(QChartSeries3D* s) {
    if (!s || m_series3D.contains(s)) return;
    m_series3D.append(s);
    addSeries(s);
    hookSeriesDirty(s);
    m_worldCacheDirty = true;
}
void QChartLayer3D::removeSeries3D(QChartSeries3D* s) {
    if (!s) return;
    m_series3D.removeOne(s);
    removeSeries(s);
    unhookSeriesDirty(s);
    m_worldCacheDirty = true;
}
void QChartLayer3D::hookSeriesDirty(QChartSeries3D* s) {
    QObject::connect(s, &QChartSeries3D::dataChanged, this, [this]() { m_worldCacheDirty = true; });
}
void QChartLayer3D::unhookSeriesDirty(QChartSeries3D* s) {
    QObject::disconnect(s, &QChartSeries3D::dataChanged, this, nullptr);
}

// ===== 数据盒 =====
void QChartLayer3D::setAxesDataBox(const QVector3D& dataMin, const QVector3D& dataMax) {
    m_axesDataMin = dataMin;
    m_axesDataMax = dataMax;
}
bool QChartLayer3D::hasValidAxesDataBox() const {
    if (m_axesDataMin.x() > m_axesDataMax.x() ||
        m_axesDataMin.y() > m_axesDataMax.y() ||
        m_axesDataMin.z() > m_axesDataMax.z())
        return false;
    return !(m_axesDataMin == m_axesDataMax);
}

// // ===== ProjectFn3D（供系列使用，保留）=====
// ProjectFn3D QChartLayer3D::makeProjectFn(const QChartCamera3D* cam,
//                                          const QRectF& plotArea) const {
//     return [this, cam, plotArea](const QDataPoint3D& d) -> QChartProjectedPoint {
//         qreal n0 = m_axisX ? m_axisX->toNumeric(d.x()) : d.x().toDouble();
//         qreal n1 = m_axisY ? m_axisY->toNumeric(d.y()) : d.y().toDouble();
//         qreal n2 = m_axisZ ? m_axisZ->toNumeric(d.z()) : d.z().toDouble();
//         const QVector3D world = m_projection3D
//             ? m_projection3D->toCartesian(n0, n1, n2)
//             : QVector3D(n0, n1, n2);
//         if (!cam) return QChartProjectedPoint{ QPointF(qQNaN(), qQNaN()), 0.0 };
//         return cam->project(world, plotArea);
//     };
// }

// ===== collectPrimitives —— 纯 Numeric 图元组装 =====
void QChartLayer3D::collectPrimitives(QChartScene& scene) const {
    // 0. 轴配置重同步
    unsigned int cnt = 0;
    if (m_axes3D) {
        m_axes3D->axis(0).axis = m_axisX;
        m_axes3D->axis(1).axis = m_axisY;
        m_axes3D->axis(2).axis = m_axisZ;
    }

    // 1. 系列 worldCache 重建（仅系列，保留）
    // if (m_worldCacheDirty) {
    //     for (QChartSeries3D* s : m_series3D) {
    //         if (!s) continue;
    //         auto* surf = qobject_cast<QChartSurfaceSeries*>(s);
    //         if (surf) {
    //             QVector<QVector3D>& cache = surf->worldCache();
    //             cache.resize(surf->count());
    //             for (int i = 0; i < surf->count(); ++i) {
    //                 const QDataPoint3D d = surf->points().at(i);
    //                 qreal n0 = m_axisX ? m_axisX->toNumeric(d.x()) : d.x().toDouble();
    //                 qreal n1 = m_axisY ? m_axisY->toNumeric(d.y()) : d.y().toDouble();
    //                 qreal n2 = m_axisZ ? m_axisZ->toNumeric(d.z()) : d.z().toDouble();
    //                 cache[i] = m_projection3D
    //                     ? m_projection3D->toCartesian(n0, n1, n2)
    //                     : QVector3D(n0, n1, n2);
    //             }
    //         } else if (s->numericCacheActive()) {
    //             QVector<QVector3D>& cache = s->worldCache();
    //             const QVector<QVector3D>& num = s->numericCache();
    //             cache.resize(num.size());
    //             for (int i = 0; i < num.size(); ++i) {
    //                 const QVector3D& n = num.at(i);
    //                 cache[i] = m_projection3D
    //                     ? m_projection3D->toCartesian(n.x(), n.y(), n.z())
    //                     : n;
    //             }
    //         }
    //     }
    //     m_worldCacheDirty = false;
    // }

    // 2. 轴/网格/盒边框（全部通过 drawAtPosition 生成 Numeric 图元）
    const bool axesValid = m_axes3D && m_axes3D->visible() && hasValidAxesDataBox();
    if (axesValid) {
        const QVector3D& mn = m_axesDataMin;
        const QVector3D& mx = m_axesDataMax;
        const int segments = m_projection3D ? m_projection3D->samplingSegmentsHint() : 72;
        const QColor gridCol = gridColor();
        const QColor boxCol(160, 160, 160);

        auto axisColor = [this](int d) -> QColor {
            const QChartAxis* a = m_axes3D->axis(d).axis;
            return a ? a->color() : QColor(150, 150, 150);
        };
        const QColor spineCol[3] = { axisColor(0), axisColor(1), axisColor(2) };

        // ----- 辅助 lambda：获取某维度的刻度数值 -----
        auto getTicks = [&](int dim) -> QVector<qreal> {
            const QChartAxis* a = m_axes3D->axis(dim).axis;
            if (!a) return {};
            qreal lo, hi;
            switch (dim) {
                case 0: lo = mn.x(); hi = mx.x(); break;
                case 1: lo = mn.y(); hi = mx.y(); break;
                case 2: lo = mn.z(); hi = mx.z(); break;
                default: return {};
            }
            return a->tickValues(lo, hi);
        };
        const QVector<qreal> t0 = getTicks(0);
        const QVector<qreal> t1 = getTicks(1);
        const QVector<qreal> t2 = getTicks(2);

        // ----- 辅助 lambda：添加一条线段（调用 drawAtPosition，generateTicks=false）-----
        auto addLine = [&](QChartAxis* axis, int dimIndex, qreal dimMin, qreal dimMax,
                           qreal off0, qreal off1, const QColor& color, qreal penWidth, QChartScene& out) {
            if (!axis) return;
            int segments = m_projection3D ? m_projection3D->samplingSegmentsHint() : 72;
            int cnt = out.primitives.size();
            axis->drawAtPosition(dimMin, dimMax, off0, off1, dimIndex,
                                 out.primitives, out.labels, segments, false);
            for (int i = cnt; i < out.primitives.size(); ++i) {
                auto& prim = out.primitives[i];
                prim.color = color;
                prim.penWidth = penWidth;
                prim.sourceId = -1;
            }
            out.maxSourceId++;
            out.PrimitiveIdPrefixSum.push_back(out.primitives.size());
        };

        // ===== 主轴（Spine）：带刻度点和标签 =====
        for (int d = 0; d < 3; ++d) {
            const auto& cfg = m_axes3D->axis(d);
            if (!cfg.visible) continue;
            QChartAxis* axis = cfg.axis;
            if (!axis) continue;

            qreal dimMin, dimMax, off0, off1;
            switch (d) {
                case 0: dimMin = mn.x(); dimMax = mx.x(); off0 = mn.y(); off1 = mn.z(); break;
                case 1: dimMin = mn.y(); dimMax = mx.y(); off0 = mn.x(); off1 = mn.z(); break;
                case 2: dimMin = mn.z(); dimMax = mx.z(); off0 = mn.x(); off1 = mn.y(); break;
                default: continue;
            }

            QVector<QChartPrimitive> tempPrims;
            QVector<QChartTextLabel> tempLabels;
            axis->drawAtPosition(dimMin, dimMax, off0, off1, d,
                                 tempPrims, tempLabels, segments, true);

            // 直接添加图元（Numeric 坐标）
            for (auto& prim : tempPrims) {
                prim.sourceId = -1;
                prim.layer = QChartPrimitive::Layer::ForegroundDecor;
                out.append(prim);
            }

            // 标签：只保留绑定标签（refPrimitiveId >= 0），自由标签丢弃
            if (labels) {
                for (const auto& lbl : tempLabels) {
                    if (lbl.refPrimitiveId >= 0) {
                        labels->append(lbl);
                    }
                }
            }
        }

        // ===== 网格线 =====
        if (m_gridMode == GridMode::Box) {
            // 底面 z = zMin：沿 X 方向（Y 为刻度值）和 Y 方向（X 为刻度值）
            for (qreal v : t1) {
                addLine(m_axes3D->axis(0).axis, 0, mn.x(), mx.x(), v, mn.z(),
                        gridCol, 1.0, QChartPrimitive::Layer::Grid);
            }
            for (qreal u : t0) {
                addLine(m_axes3D->axis(1).axis, 1, mn.y(), mx.y(), u, mn.z(),
                        gridCol, 1.0, QChartPrimitive::Layer::Grid);
            }
        } else { // Lattice 模式
            // 族 U：固定 (v, w) 沿 X
            for (qreal v : t1)
                for (qreal w : t2)
                    addLine(m_axes3D->axis(0).axis, 0, mn.x(), mx.x(), v, w,
                            gridCol, 1.0, QChartPrimitive::Layer::Grid);
            // 族 V：固定 (u, w) 沿 Y
            for (qreal u : t0)
                for (qreal w : t2)
                    addLine(m_axes3D->axis(1).axis, 1, mn.y(), mx.y(), u, w,
                            gridCol, 1.0, QChartPrimitive::Layer::Grid);
            // 族 W：固定 (u, v) 沿 Z
            for (qreal u : t0)
                for (qreal v : t1)
                    addLine(m_axes3D->axis(2).axis, 2, mn.z(), mx.z(), u, v,
                            gridCol, 1.0, QChartPrimitive::Layer::Grid);
        }

        // ===== 盒边框（12 条边）=====
        const QVector<QVector3D> corners = QChartAxes3D::boxCorners(mn, mx);
        const QVector<QPair<int,int>> edges = QChartAxes3D::boxEdges();
        const QVector<int> spineIdx = QChartAxes3D::spineEdgeIndices();

        for (int i = 0; i < edges.size(); ++i) {
            int a = edges[i].first, b = edges[i].second;
            QVector3D p1 = corners[a], p2 = corners[b];
            bool isSpine = spineIdx.contains(i);
            int dim = (i < 4) ? 0 : (i < 8 ? 1 : 2);
            QChartAxis* axis = m_axes3D->axis(dim).axis;
            if (!axis) continue;

            // 确定变化维度
            int dimIndex;
            qreal dimMin, dimMax, off0, off1;
            if (!qFuzzyCompare(p1.x(), p2.x())) {
                dimIndex = 0; dimMin = p1.x(); dimMax = p2.x(); off0 = p1.y(); off1 = p1.z();
            } else if (!qFuzzyCompare(p1.y(), p2.y())) {
                dimIndex = 1; dimMin = p1.y(); dimMax = p2.y(); off0 = p1.x(); off1 = p1.z();
            } else {
                dimIndex = 2; dimMin = p1.z(); dimMax = p2.z(); off0 = p1.x(); off1 = p1.y();
            }

            addLine(axis, dimIndex, dimMin, dimMax, off0, off1,
                    isSpine ? spineCol[dim] : boxCol,
                    isSpine ? 2.0 : 1.0,
                    QChartPrimitive::Layer::ForegroundDecor);
        }

        // ===== 轴标题（保留，仅用主轴端点）=====
        if (labels) {
            for (int d = 0; d < 3; ++d) {
                const auto& cfg = m_axes3D->axis(d);
                if (!cfg.visible) continue;
                QChartAxis* axis = cfg.axis;
                if (!axis) continue;

                QVector3D maxAnchor = mn;
                if (d == 0) maxAnchor.setX(mx.x());
                else if (d == 1) maxAnchor.setY(mx.y());
                else maxAnchor.setZ(mx.z());

                QString title = cfg.axisTitle;
                if (title.isEmpty() && axis) title = axis->title();
                if (title.isEmpty() && m_projection3D) title = m_projection3D->dimensionName(d);
                if (!title.isEmpty()) {
                    QChartTextLabel lbl;
                    lbl.text = title;
                    lbl.numericAnchor = maxAnchor;
                    lbl.color = spineCol[d];
                    lbl.isTitle = true;
                    lbl.fontSize = 12.0f;
                    lbl.refPrimitiveId = -1; // 自由标签，由渲染器处理
                    labels->append(lbl);
                }
            }
        }
    }

    // 3. 系列图元（保留，使用 ProjectFn3D）
    const ProjectFn3D fn = makeProjectFn(nullptr, QRectF()); // cam/plotArea 在 series 内部使用
    for (QChartSeries3D* s : m_series3D) {
        if (!s || !s->isVisible()) continue;
        // 注意：series 的 collectPrimitives 需要 ProjectFn3D，它内部会调用相机投影
        // 但我们暂时不传入 cam/plotArea，因为 series 自己会用到闭包。
        // 这里我们传一个 dummy，但实际 series 会使用 makeProjectFn 传入的闭包。
        // 为了兼容，我们让 series 使用自己的投影逻辑，暂不修改。
        // 但原代码中 s->collectPrimitives(fn, out) 需要 fn，所以我们保留原样。
        // 由于我们不再需要 cam/plotArea，所以这里直接传递默认构造的 fn。
        // 但原 series 实现依赖 fn，我们保持原样。
        s->collectPrimitives(fn, out);
    }
}

