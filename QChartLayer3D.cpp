// QChartLayer3D.cpp —— 3D 图层实现（design_3d_axes.md §8.3/§8.4 扩展）
// 轴/网格图元（Grid/ForegroundDecor 分层）+ 系列图元（全链闭包 + worldCache）+ labels 出参。
#include "QChartLayer3D.h"
#include "QChartCamera3D.h"
#include "QChartProjection3D.h"
#include "QChartSurfaceSeries.h"
#include "QChartRenderer.h"   // QChartPrimitive / QChartTextLabel
#include <cmath>

QChartLayer3D::QChartLayer3D(QObject* parent) : QChartLayer(parent) {
    m_axes3D = std::make_unique<QChartAxes3D>();
    m_axes3D->axis(0).axis = m_axisX;   // 初始绑定（当前均 null；setAxisX/Y/Z 重绑 + collect 重同步）
    m_axes3D->axis(1).axis = m_axisY;
    m_axes3D->axis(2).axis = m_axisZ;
}

// ===== 轴（重绑时同步 axes3D 配置槽；轴变化 → worldCache 置脏重建）=====
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

// ===== 3D 系列管理 =====
void QChartLayer3D::addSeries3D(QChartSeries3D* s) {
    if (!s || m_series3D.contains(s)) return;
    m_series3D.append(s);
    addSeries(s);   // 存入基类 m_series（复用图例/主题/所有权/析构）
    hookSeriesDirty(s);   // 数据变化 → worldCache 置脏（§9 失效策略）
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

// ===== 轴/网格数据盒 =====
bool QChartLayer3D::hasValidAxesDataBox() const {
    if (m_axesDataMin.x() > m_axesDataMax.x() ||
        m_axesDataMin.y() > m_axesDataMax.y() ||
        m_axesDataMin.z() > m_axesDataMax.z())
        return false;   // min>max 任一维 → 无效
    return !(m_axesDataMin == m_axesDataMax);   // 默认 (0,0,0)-(0,0,0) → 无效
}

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

// ===== 辅助：Numeric → Screen =====
QChartProjectedPoint QChartLayer3D::projectNumeric(const QVector3D& num,
                                                   const QChartCamera3D* cam,
                                                   const QRectF& plotArea) const {
    if (!cam) return QChartProjectedPoint{ QPointF(qQNaN(), qQNaN()), 0.0 };
    const QVector3D world = m_projection3D
        ? m_projection3D->toWorld(num.x(), num.y(), num.z())
        : num;
    return cam->project(world, plotArea);
}

// ===== 辅助：直线采样（identity 快速通道：免 toWorld；段数 = samplingSegmentsHint）=====
void QChartLayer3D::emitLine(QVector3D numA, QVector3D numB, QChartPrimitive::Layer layer,
                             const QColor& color, qreal penWidth, const QChartCamera3D* cam,
                             const QRectF& plotArea, QVector<QChartPrimitive>& out) const {
    if (!cam) return;
    const bool identity = m_projection3D && m_projection3D->isIdentityMapping();   // §5.4 快速通道
    const int segments = m_projection3D ? m_projection3D->samplingSegmentsHint() : 32;
    for (int i = 0; i < segments; ++i) {
        const qreal t0 = static_cast<qreal>(i) / segments;
        const qreal t1 = static_cast<qreal>(i + 1) / segments;
        const QVector3D n0 = numA + (numB - numA) * t0;
        const QVector3D n1 = numA + (numB - numA) * t1;
        // identity：Numeric ≡ World 直通（免 toWorld）；否则 toWorld 弯曲
        const QVector3D w0 = identity ? n0 : m_projection3D->toWorld(n0.x(), n0.y(), n0.z());
        const QVector3D w1 = identity ? n1 : m_projection3D->toWorld(n1.x(), n1.y(), n1.z());
        const QChartProjectedPoint p0 = cam->project(w0, plotArea);
        const QChartProjectedPoint p1 = cam->project(w1, plotArea);
        if (!std::isfinite(p0.screen.x()) || !std::isfinite(p0.screen.y()) ||
            !std::isfinite(p1.screen.x()) || !std::isfinite(p1.screen.y()))
            continue;   // 任一端 NaN → 跳过该段

        QChartPrimitive prim;
        prim.type = QChartPrimitive::Type::LineSegment;
        prim.a = p0.screen;
        prim.b = p1.screen;
        prim.depth = (p0.depth + p1.depth) * 0.5;   // 段中点深度（排序键）
        prim.dataIndex = -1;                        // 轴/网格装饰无数据点索引
        prim.penWidth = penWidth;
        prim.color = color;
        prim.worldA = p0.world;                     // GL 顶点源（t42，§3.2）
        prim.worldB = p1.world;
        prim.layer = layer;
        out.append(prim);
    }
}

// ===== 该维刻度值（axes3D 委托）=====
QVector<qreal> QChartLayer3D::dimTicks(int dim) const {
    if (!m_axes3D || !hasValidAxesDataBox()) return {};
    qreal lo = 0.0, hi = 0.0;
    if (dim == 0)      { lo = m_axesDataMin.x(); hi = m_axesDataMax.x(); }
    else if (dim == 1) { lo = m_axesDataMin.y(); hi = m_axesDataMax.y(); }
    else               { lo = m_axesDataMin.z(); hi = m_axesDataMax.z(); }
    return m_axes3D->ticks(dim, lo, hi);
}

// ===== 图元收集 =====
void QChartLayer3D::collectPrimitives(const QChartCamera3D* cam, const QRectF& plotArea,
                                      QVector<QChartPrimitive>& out,
                                      QVector<QChartTextLabel>* labels) const {
    if (!cam) return;

    // 0. 轴配置重同步（防止经 QChartLayer* 调用 setAxisX/Y 漏绑）
    if (m_axes3D) {
        m_axes3D->axis(0).axis = m_axisX;
        m_axes3D->axis(1).axis = m_axisY;
        m_axes3D->axis(2).axis = m_axisZ;
    }

    // 1. worldCache 重建（design_phase3.md §9：投影/数据变化才重建——置脏才重算，免每帧 O(N)）：
    //    曲面（QVariant 路径）：自身 axis toNumeric + projection3D toWorld 直算（不走系列闭包，裁决 b）；
    //    数值型系列：worldCache = toWorld(numericCache)（数值已预转换，免 QVariant；VBO 源）；
    //    混合（QVariant）非曲面系列：无 worldCache（Phase 2 边界，走全链闭包）。
    if (m_worldCacheDirty) {
        for (QChartSeries3D* s : m_series3D) {
            if (!s) continue;
            auto* surf = qobject_cast<QChartSurfaceSeries*>(s);
            if (surf) {
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
            } else if (s->numericCacheActive()) {
                QVector<QVector3D>& cache = s->worldCache();
                const QVector<QVector3D>& num = s->numericCache();
                cache.resize(num.size());
                for (int i = 0; i < num.size(); ++i) {
                    const QVector3D& n = num.at(i);
                    cache[i] = m_projection3D
                        ? m_projection3D->toWorld(n.x(), n.y(), n.z())
                        : n;
                }
            }
        }
        m_worldCacheDirty = false;
    }

    // 2. 轴/网格图元（axesDataBox 有效 + axes3D 可见时才生成；默认无效 → 零轴零网格，零回归）
    const bool axesValid = m_axes3D && m_axes3D->visible() && hasValidAxesDataBox();
    if (axesValid) {
        const QVector3D& mn = m_axesDataMin;
        const QVector3D& mx = m_axesDataMax;
        const QVector<qreal> t0 = dimTicks(0), t1 = dimTicks(1), t2 = dimTicks(2);
        const QColor gridCol = gridColor();
        const QColor boxCol(160, 160, 160);   // 盒 12 边淡框
        auto axisColor = [this](int d) -> QColor {
            const QChartAxis* a = m_axes3D->axis(d).axis;
            return a ? a->color() : QColor(150, 150, 150);
        };
        const QColor spineCol[3] = { axisColor(0), axisColor(1), axisColor(2) };

        // Grid 层（§5.1/§5.2）：与系列统一深度排序（深度偏置由 Renderer 应用，§7.2）
        if (m_gridMode == GridMode::Box) {
            // 盒模式 = 盒底面（w=wMin）tick 对齐网格（沿 u、v）
            for (qreal v : t1)
                emitLine(QVector3D(mn.x(), v, mn.z()), QVector3D(mx.x(), v, mn.z()),
                         QChartPrimitive::Layer::Grid, gridCol, 1.0, cam, plotArea, out);
            for (qreal u : t0)
                emitLine(QVector3D(u, mn.y(), mn.z()), QVector3D(u, mx.y(), mn.z()),
                         QChartPrimitive::Layer::Grid, gridCol, 1.0, cam, plotArea, out);
        } else {
            // 晶格模式三族（§5.2）：族 U (nv+1)(nw+1)、族 V (nu+1)(nw+1)、族 W (nu+1)(nv+1)
            for (qreal v : t1)
                for (qreal w : t2)
                    emitLine(QVector3D(mn.x(), v, w), QVector3D(mx.x(), v, w),
                             QChartPrimitive::Layer::Grid, gridCol, 1.0, cam, plotArea, out);
            for (qreal u : t0)
                for (qreal w : t2)
                    emitLine(QVector3D(u, mn.y(), w), QVector3D(u, mx.y(), w),
                             QChartPrimitive::Layer::Grid, gridCol, 1.0, cam, plotArea, out);
            for (qreal u : t0)
                for (qreal v : t1)
                    emitLine(QVector3D(u, v, mn.z()), QVector3D(u, v, mx.z()),
                             QChartPrimitive::Layer::Grid, gridCol, 1.0, cam, plotArea, out);
        }

        // 3. ForegroundDecor 层：盒 12 边（淡框 1px）+ 3 强调 spine（2px 轴色）+ 刻度点标记
        const QVector<QVector3D> corners = QChartAxes3D::boxCorners(mn, mx);
        const QVector<QPair<int, int>> edges = QChartAxes3D::boxEdges();
        const QVector<int> spine = QChartAxes3D::spineEdgeIndices();
        for (int i = 0; i < edges.size(); ++i) {
            const bool isSpine = spine.contains(i);
            const int dim = i < 4 ? 0 : (i < 8 ? 1 : 2);
            emitLine(corners.at(edges.at(i).first), corners.at(edges.at(i).second),
                     QChartPrimitive::Layer::ForegroundDecor,
                     isSpine ? spineCol[dim] : boxCol,
                     isSpine ? 2.0 : 1.0, cam, plotArea, out);
        }
        const QVector<QVector<qreal>> ticks{ t0, t1, t2 };
        for (int d = 0; d < 3; ++d) {
            if (!m_axes3D->axis(d).visible) continue;
            for (qreal tv : ticks.at(d)) {
                const QVector3D anchor = QChartAxes3D::tickAnchor(d, tv, mn);
                const QChartProjectedPoint p = projectNumeric(anchor, cam, plotArea);
                if (!std::isfinite(p.screen.x()) || !std::isfinite(p.screen.y())) continue;
                QChartPrimitive prim;
                prim.type = QChartPrimitive::Type::Point;
                prim.a = p.screen;
                prim.markerSize = m_axes3D->axis(d).markerSizePx;
                prim.color = spineCol[d];
                prim.layer = QChartPrimitive::Layer::ForegroundDecor;
                prim.dataIndex = -1;
                prim.worldA = p.world;              // GL 顶点源（t42，§3.2）
                out.append(prim);
            }
        }

        // 4. labels（可选出参，§6.2）：tick 标签 + 轴标题（billboard）
        if (labels) {
            for (int d = 0; d < 3; ++d) {
                const QChartAxes3D::AxisConfig& cfg = m_axes3D->axis(d);
                if (!cfg.visible) continue;
                // spine 端点（min/max 锚）：自动偏移方向 = 沿投影轴向外 10px
                QVector3D minAnchor = mn, maxAnchor = mn;
                if (d == 0)      maxAnchor.setX(mx.x());
                else if (d == 1) maxAnchor.setY(mx.y());
                else             maxAnchor.setZ(mx.z());
                QPointF offset = cfg.labelOffsetPx;
                if (offset == QPointF(0, 0)) {
                    const QChartProjectedPoint pMin = projectNumeric(minAnchor, cam, plotArea);
                    const QChartProjectedPoint pMax = projectNumeric(maxAnchor, cam, plotArea);
                    const QPointF dir = pMax.screen - pMin.screen;
                    const qreal len = std::sqrt(QPointF::dotProduct(dir, dir));
                    if (len > 1e-6) offset = dir / len * 10.0;   // 沿投影轴向外
                }
                const QStringList texts = m_axes3D->tickLabelTexts(d, minAnchor[d], maxAnchor[d]);
                const QVector<qreal> tks = ticks.at(d);
                for (int i = 0; i < texts.size() && i < tks.size(); ++i) {
                    const QVector3D anchor = QChartAxes3D::tickAnchor(d, tks.at(i), mn);
                    const QChartProjectedPoint p = projectNumeric(anchor, cam, plotArea);
                    if (!std::isfinite(p.screen.x()) || !std::isfinite(p.screen.y())) continue;
                    QChartTextLabel lbl;
                    lbl.screenPos = p.screen + offset;
                    lbl.text = texts.at(i);
                    lbl.color = spineCol[d];
                    labels->append(lbl);
                }
                // 轴标题（spine 端点；空 = axis->title()，再空 = dimensionName）
                if (cfg.axisTitleVisible) {
                    QString title = cfg.axisTitle;
                    if (title.isEmpty() && cfg.axis) title = cfg.axis->title();
                    if (title.isEmpty() && m_projection3D) title = m_projection3D->dimensionName(d);
                    if (!title.isEmpty()) {
                        const QChartProjectedPoint pMax = projectNumeric(maxAnchor, cam, plotArea);
                        if (std::isfinite(pMax.screen.x()) && std::isfinite(pMax.screen.y())) {
                            QChartTextLabel lbl;
                            lbl.screenPos = pMax.screen + offset;
                            lbl.text = title;
                            lbl.isTitle = true;
                            lbl.fontSize = 12.0;
                            lbl.color = spineCol[d];
                            labels->append(lbl);
                        }
                    }
                }
            }
        }
    }

    // 5. 3D 系列图元（全链闭包；现有路径零改动）
    const ProjectFn3D fn = makeProjectFn(cam, plotArea);
    for (QChartSeries3D* s : m_series3D) {
        if (!s || !s->isVisible()) continue;
        s->collectPrimitives(fn, out);
    }
}
