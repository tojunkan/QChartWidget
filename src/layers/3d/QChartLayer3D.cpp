// QChartLayer3D.cpp
#include "QChartLayer3D.h"
#include "QChartAxis.h"   // 4a：不再经二维层头传递，需完整类型
#include "QChartCamera3D.h"
#include "QChartProjection3D.h"
#include "QChartSurfaceSeries.h"
#include "QChartRenderer.h"
#include <cmath>

namespace {
/// 批次2 B：Box 模式限定"三维直角坐标"投影
bool isCartesian3DProjection(const QChartProjection3D* p)
{
    return p && p->type() == QChartAbstractProjection::CoordinateSystem::Cartesian3D;
}

QString projection3DTypeName(const QChartProjection3D* p)
{
    if (!p) return QStringLiteral("null（无投影）");
    switch (p->type()) {
    case QChartAbstractProjection::CoordinateSystem::Cartesian3D: return QStringLiteral("Cartesian3D");
    case QChartAbstractProjection::CoordinateSystem::Spherical:   return QStringLiteral("Spherical3D");
    case QChartAbstractProjection::CoordinateSystem::Cylindrical: return QStringLiteral("Cylindrical3D");
    case QChartAbstractProjection::CoordinateSystem::Functional3D: return QStringLiteral("Functional3D");
    default: return QStringLiteral("非三维直角坐标");
    }
}
} // namespace

QChartLayer3D::QChartLayer3D(QObject* parent)
    : QChartAbstractLayer(parent)
{
    // 4a：网格样式变化 → 数据脏标记（语义同原二维层构造内的同名连接）
    connect(this, &QChartLayer3D::gridChanged, this, &QChartLayer3D::invalidateData);
    m_axes3D = std::make_unique<QChartAxes3D>();
    // 4b：数据盒不再独立持有（由三根轴范围组装）；轴缺省范围 0..10 即迁移前的默认盒
    m_axes3D->axis(0).axis = m_axisX;
    m_axes3D->axis(1).axis = m_axisY;
    m_axes3D->axis(2).axis = m_axisZ;
    // ★ 批次 B1：3D 相机归 layer——scene3D.camera 恒指向本层相机值成员
    m_scene.camera = &m_camera3D;
    // 4b：无独立工作副本——collect 时由 m_axes3D->dataBounds() 从三轴范围组装
}

// ===== 轴重绑 =====
void QChartLayer3D::setAxisX(QChartAxis* a) {
    m_axisX = a;                                   // 4a：三维层自持轴绑定
    if (m_axes3D) m_axes3D->axis(0).axis = a;
}
void QChartLayer3D::setAxisY(QChartAxis* a) {
    m_axisY = a;
    if (m_axes3D) m_axes3D->axis(1).axis = a;
}
void QChartLayer3D::setAxisZ(QChartAxis* a) {
    m_axisZ = a;
    if (m_axes3D) m_axes3D->axis(2).axis = a;
}

// ===== 数据盒 =====
void QChartLayer3D::setProjection3D(const QChartProjection3D* proj) {
    m_projection3D = proj;   // 仅用于采样提示/默认盒；图元组装本身纯 Numeric
}

void QChartLayer3D::setDataBounds(const QCube& box) {
    // 4b：数据盒写入 = 写三根轴的范围（轴是范围的唯一持有者；未绑定的轴跳过）
    // 4c：统一 QCube 主签名（两点重载在头文件内转发至此）
    if (QChartAxis* ax = m_axes3D ? m_axes3D->axis(0).axis : nullptr) ax->setRange(box.min.x(), box.max.x());
    if (QChartAxis* ay = m_axes3D ? m_axes3D->axis(1).axis : nullptr) ay->setRange(box.min.y(), box.max.y());
    if (QChartAxis* az = m_axes3D ? m_axes3D->axis(2).axis : nullptr) az->setRange(box.min.z(), box.max.z());
}
bool QChartLayer3D::hasValidDataBounds() const {
    return m_axes3D && m_axes3D->dataBounds().isValid();   // 4b：从三轴范围按需组装
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

// ===== 网格样式（4a：自持；实现语义同原二维层）=====
void QChartLayer3D::setGridVisible(bool v)
{
    if (m_gridVisible == v) return;
    m_gridVisible = v;
    emit gridChanged();
}

void QChartLayer3D::setGridColor(const QColor& c)
{
    if (m_gridColorOverride && *m_gridColorOverride == c) return;
    m_gridColorOverride = c;
    emit gridChanged();
}

void QChartLayer3D::setThemeGridColor(const QColor& c)
{
    m_themeGridColor = c;
    if (!m_gridColorOverride) emit gridChanged();
}

void QChartLayer3D::clearGridColor()
{
    if (!m_gridColorOverride) return;
    m_gridColorOverride.reset();
    emit gridChanged();
}

// 4a：与迁移前继承自二维层的空默认一致（3D 范围重算归后续批次）
void QChartLayer3D::recomputeDataBounds() {}

// 4a：边框轴外边距（用本层 axisX/axisY；算术与原二维层实现逐字一致）
void QChartLayer3D::borderAxisSizeHint(const QFont& font, qreal& left, qreal& top,
                                       qreal& right, qreal& bottom) const
{
    const QChartAxis* axes[2] = { m_axisX, m_axisY };
    for (const QChartAxis* a : axes) {
        if (!a) continue;
        const Qt::Alignment al = a->alignment();
        if (al == Qt::AlignBottom || al == Qt::AlignTop) {
            const qreal h = a->sizeHint(font).height();
            if (al == Qt::AlignBottom) bottom += h; else top += h;
        } else if (al == Qt::AlignLeft || al == Qt::AlignRight) {
            const qreal w = a->sizeHint(font).width();
            if (al == Qt::AlignLeft) left += w; else right += w;
        }
        // HCenter/VCenter（数据主脊）不占外边距
    }
}

// ===== collectPrimitives —— 纯 Numeric 图元组装 =====
void QChartLayer3D::collectPrimitives() {
    // 0a. 场景负载复位（每帧重收集；sourceId 从 0 重新分配）
    m_scene.primitives.clear();
    m_scene.labels.clear();
    m_scene.maxSourceId = 0;
    m_scene.PrimitiveIdPrefixSum.clear();
    m_scene.PrimitiveIdPrefixSum.append(0);

    // 0. 轴配置重同步
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
    const bool axesValid = m_axes3D && m_axes3D->visible() && hasValidDataBounds();
    if (axesValid) {
        // 4b：数据盒按需从三根轴范围组装（用完即弃；数值与迁移前工作副本逐位一致）
        const QCube box = m_axes3D->dataBounds();
        const QVector3D mn = box.min;
        const QVector3D mx = box.max;
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

        // ----- 辅助 lambda：添加一条线段（调用 drawAtPosition，标签模式按调用点传入）-----
        auto addLine = [&](QChartAxis* axis, int dimIndex, qreal dimMin, qreal dimMax,
                           qreal off0, qreal off1, QChartAxis::LabelMode labelMode, const QColor& color, qreal penWidth) {
            if (!axis) return;
            // int segments = m_projection3D ? m_projection3D->samplingSegmentsHint() : 72;
            int cnt = m_scene.primitives.size();
            m_scene.maxSourceId++;
            axis->drawAtPosition(dimMin, dimMax, off0, off1, dimIndex,
                                 m_scene, segments, labelMode);
            for (int i = cnt; i < m_scene.primitives.size(); ++i) {
                auto& prim = m_scene.primitives[i];
                prim.color = color;
                prim.penWidth = penWidth;
                prim.sourceId = m_scene.maxSourceId;
            }
            m_scene.PrimitiveIdPrefixSum.push_back(m_scene.primitives.size());
        };

        // ===== 模式解析（批次2 B）=====
        // Box 仅三维直角坐标有效；其它投影 qWarning 并自动回退 FaceLine。
        GridMode mode = m_gridMode;
        if (mode == GridMode::Box && !isCartesian3DProjection(m_projection3D)) {
            qWarning() << "QChartLayer3D: GridMode::Box 仅支持三维直角坐标（Cartesian3D）投影，"
                          "当前投影 ="
                       << projection3DTypeName(m_projection3D)
                       << "→ 自动回退 GridMode::FaceLine（批次2 B 契约）";
            mode = GridMode::FaceLine;
        }
        const bool boxMode = (mode == GridMode::Box);
        const bool faceLineMode = (mode == GridMode::FaceLine);
        // FACE-DEFERRED（批次2 B）：面线模式本阶段只画一条退化安全轴线，不画任何面；
        // 面/曲面绘制留待曲面系列阶段（几何入口见 QChartAxes3D::faceLineSegment 注释）。

        // ===== 主轴 =====
        auto addSpine = [&](int d, QChartAxis::LabelMode labelMode) {
            const auto& cfg = m_axes3D->axis(d);
            if (!cfg.visible) return;
            QChartAxis* axis = cfg.axis;
            if (!axis) return;

            qreal dimMin, dimMax, off0, off1;
            switch (d) {
                case 0: dimMin = mn.x(); dimMax = mx.x(); off0 = mn.y(); off1 = mn.z(); break;
                case 1: dimMin = mn.y(); dimMax = mx.y(); off0 = mn.x(); off1 = mn.z(); break;
                case 2: dimMin = mn.z(); dimMax = mx.z(); off0 = mn.x(); off1 = mn.y(); break;
                default: return;
            }

            addLine(axis, d, dimMin, dimMax, off0, off1, labelMode, spineCol[d], 2.0);
        };

        if (boxMode) {
            // 盒模式：三条主轴按各自刻度逐个标注（Tickwise；显式坐标优先，退化走图元绑定回退）
            for (int d = 0; d < 3; ++d) addSpine(d, QChartAxis::LabelMode::Tickwise);
        } else if (faceLineMode) {
            // 面线模式（默认）：只画那条退化安全的轴线（球坐标 θ=φ=0），按刻度逐个标注
            const auto& cfg0 = m_axes3D->axis(0);
            if (cfg0.visible && cfg0.axis) {
                const QPair<QVector3D, QVector3D> safeSeg = QChartAxes3D::faceLineSegment(mn, mx);
                addLine(cfg0.axis, 0, safeSeg.first.x(), safeSeg.second.x(),
                        safeSeg.first.y(), safeSeg.first.z(), QChartAxis::LabelMode::Tickwise,
                        spineCol[0], 2.0);
            }
        } else {
            // 晶格模式：只画线，LabelMode::None —— 不生成任何文字
            for (int d = 0; d < 3; ++d) addSpine(d, QChartAxis::LabelMode::None);
        }

        // ===== 网格线 =====
        if (boxMode) {
            // 底面 z = zMin：沿 X 方向（Y 为刻度值）和 Y 方向（X 为刻度值）
            for (qreal v : t1) {
                addLine(m_axes3D->axis(0).axis, 0, mn.x(), mx.x(), v, mn.z(),
                        QChartAxis::LabelMode::None, gridCol, 1.0);
            }
            for (qreal u : t0) {
                addLine(m_axes3D->axis(1).axis, 1, mn.y(), mx.y(), u, mn.z(),
                        QChartAxis::LabelMode::None, gridCol, 1.0);
            }
        } else if (mode == GridMode::Lattice) {
            // 族 U：固定 (v, w) 沿 X
            for (qreal v : t1)
                for (qreal w : t2)
                    addLine(m_axes3D->axis(0).axis, 0, mn.x(), mx.x(), v, w,
                            QChartAxis::LabelMode::None, gridCol, 1.0);
            // 族 V：固定 (u, w) 沿 Y
            for (qreal u : t0)
                for (qreal w : t2)
                    addLine(m_axes3D->axis(1).axis, 1, mn.y(), mx.y(), u, w,
                            QChartAxis::LabelMode::None, gridCol, 1.0);
            // 族 W：固定 (u, v) 沿 Z
            for (qreal u : t0)
                for (qreal v : t1)
                    addLine(m_axes3D->axis(2).axis, 2, mn.z(), mx.z(), u, v,
                            QChartAxis::LabelMode::None, gridCol, 1.0);
        }
        // FaceLine 模式不画网格：面留待曲面系列阶段（FACE-DEFERRED）

        // ===== 盒边框（12 条边；面线模式只出安全轴线，不画盒）=====
        if (!faceLineMode) {
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
                        QChartAxis::LabelMode::None,
                        isSpine ? spineCol[dim] : boxCol,
                        isSpine ? 2.0 : 1.0);
            }
        }

        // ===== 轴标题（保留，仅用主轴端点）=====
        // if (labels) {
        //     for (int d = 0; d < 3; ++d) {
        //         const auto& cfg = m_axes3D->axis(d);
        //         if (!cfg.visible) continue;
        //         QChartAxis* axis = cfg.axis;
        //         if (!axis) continue;

        //         QVector3D maxAnchor = mn;
        //         if (d == 0) maxAnchor.setX(mx.x());
        //         else if (d == 1) maxAnchor.setY(mx.y());
        //         else maxAnchor.setZ(mx.z());

        //         QString title = cfg.axisTitle;
        //         if (title.isEmpty() && axis) title = axis->title();
        //         if (title.isEmpty() && m_projection3D) title = m_projection3D->dimensionName(d);
        //         if (!title.isEmpty()) {
        //             QChartTextLabel lbl;
        //             lbl.text = title;
        //             lbl.numericAnchor = maxAnchor;
        //             lbl.color = spineCol[d];
        //             lbl.isTitle = true;
        //             lbl.fontSize = 12.0f;
        //             lbl.refPrimitiveId = -1; // 自由标签，由渲染器处理
        //             labels->append(lbl);
        //         }
        //     }
        // }
    }

    // 3. 系列图元（保留，使用 ProjectFn3D）
    // const ProjectFn3D fn = makeProjectFn(nullptr, QRectF()); // cam/plotArea 在 series 内部使用
    // for (QChartSeries3D* s : m_series3D) {
    //     if (!s || !s->isVisible()) continue;
    //     // 注意：series 的 collectPrimitives 需要 ProjectFn3D，它内部会调用相机投影
    //     // 但我们暂时不传入 cam/plotArea，因为 series 自己会用到闭包。
    //     // 这里我们传一个 dummy，但实际 series 会使用 makeProjectFn 传入的闭包。
    //     // 为了兼容，我们让 series 使用自己的投影逻辑，暂不修改。
    //     // 但原代码中 s->collectPrimitives(fn, out) 需要 fn，所以我们保留原样。
    //     // 由于我们不再需要 cam/plotArea，所以这里直接传递默认构造的 fn。
    //     // 但原 series 实现依赖 fn，我们保持原样。
    //     s->collectPrimitives(fn, out);
    // }
}

