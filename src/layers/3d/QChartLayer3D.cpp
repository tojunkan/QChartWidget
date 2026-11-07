// QChartLayer3D.cpp
#include "QChartLayer3D.h"
#include "QChartAxis.h"   // 4a：不再经二维层头传递，需完整类型
#include "QChartCamera3D.h"
#include "QChartProjection3D.h"
#include "QChartSurfaceSeries.h"
#include "QChartRenderer.h"
#include <cmath>

namespace {
// 注：这里**不再**有 isCartesian3DProjection 之类的类型特判辅助。
// Box 模式需要"恒等映射"这一性质，直接用投影自带的 isIdentityMapping()；
// FaceLine 的面固定最后一维（全坐标系统一约定），不需要任何"固定哪一维"的开关。
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

// 前向声明：定义在文件下方（轴信号 → 脏表 axis 行）
static void connectAxisPropertySignals(QChartAxis* a, QChartAbstractLayer* layer);

QChartLayer3D::QChartLayer3D(QChartAxis* axisX, QChartAxis* axisY, QChartAxis* axisZ, QObject* parent)
    : QChartAbstractLayer(parent)
{
    // ★ layer = 相机 + 三根轴 + 图元：**轴在构造时绑定、运行期不可更换**（换轴请重建 layer）。
    //   与 2D 同构的理由：换轴会改 Data→Numeric 映射 / range 权威 / 相机窗口（见 QChartWidget::removeAxis）。
    m_axisX = axisX;
    m_axisY = axisY;
    m_axisZ = axisZ;
    m_axes3D = std::make_unique<QChartAxes3D>();
    // 4b：数据盒不再独立持有（由三根轴范围组装）；轴缺省范围 0..10 即迁移前的默认盒
    m_axes3D->axis(0).axis = m_axisX;
    m_axes3D->axis(1).axis = m_axisY;
    m_axes3D->axis(2).axis = m_axisZ;
    // 轴的一切属性变化（含 rangeChanged 这个"域变了"的信号）→ 轴段重建（脏表 axis 行）
    connectAxisPropertySignals(m_axisX, this);
    connectAxisPropertySignals(m_axisY, this);
    connectAxisPropertySignals(m_axisZ, this);
    // 4a：网格样式/模式变化 → 脏表 axis 行（网格脊属于轴段）
    connect(this, &QChartLayer3D::gridChanged, this, &QChartLayer3D::invalidateAxis);
    // ★ 批次 B1：3D 相机归 layer——scene3D.camera 恒指向本层相机值成员
    m_scene.camera = &m_camera3D;
    // 相机变化 → 转发给 widget（setViewCube 期间由 m_rangeTriggered 挡住，防回环）。
    // ★ 相机本身不再直接置脏表：它只负责"域可能变"，真正决定重建的是 rangeChanged → invalidateAxis。
    connect(&m_camera3D, &QChartCamera3D::viewChanged, this, [this]() {
        if (!m_rangeTriggered) emit viewChanged(m_camera3D.viewCube());
    });
    // 4b：无独立工作副本——collect 时由 m_axes3D->dataBounds() 从三轴范围组装
}

// ===== 相机视野盒（由 widget 设置；本层不做 projection 换算）=====
void QChartLayer3D::setViewCube(const QCube& viewCube)
{
    m_rangeTriggered = true;
    m_camera3D.setViewCube(viewCube);
    m_rangeTriggered = false;
}

// ===== 轴重绑 =====
// 轴的一切属性变化（含 rangeChanged 这个"域变了"的信号）都导致轴段重建 —— 它们是脏表的输入。
static void connectAxisPropertySignals(QChartAxis* a, QChartAbstractLayer* layer)
{
    if (!a) return;
    QObject::connect(a, &QChartAxis::rangeChanged, layer, &QChartAbstractLayer::invalidateAxis);
    QObject::connect(a, &QChartAxis::visibleChanged, layer, &QChartAbstractLayer::invalidateAxis);
    QObject::connect(a, &QChartAxis::styleChanged, layer, &QChartAbstractLayer::invalidateAxis);
    QObject::connect(a, &QChartAxis::tickCountChanged, layer, &QChartAbstractLayer::invalidateAxis);
    QObject::connect(a, &QChartAxis::subTickCountChanged, layer, &QChartAbstractLayer::invalidateAxis);
    QObject::connect(a, &QChartAxis::alignmentChanged, layer, &QChartAbstractLayer::invalidateAxis);
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

// 3D **没有边界轴**（轴是立体盒的棱，画在 plotArea 内部），因此不占用外边距。
// 这里必须返回"什么都不加"：此前逐字沿用了二维层的算术，会让 3D 的 plotArea
// 被 axisX/axisY 的 sizeHint 白挤掉一圈（AlignBottom/AlignLeft 默认值就会命中）。
void QChartLayer3D::borderAxisSizeHint(const QFont&, qreal&, qreal&,
                                       qreal&, qreal&) const
{
}

// ===== 3D 系列管理（非持有，与 2D 层同约定）=====
// 数据/可见性/颜色变化 → 只置脏表的该系列行（不再全量置脏）
void QChartLayer3D::addSeries3D(QChartSeries3D* s) {
    if (!s || m_series3D.contains(s)) return;
    m_series3D.append(s);
    connect(s, &QChartSeries::dataChanged, this, [this, s]() { invalidateSeries(s); });
    connect(s, &QChartSeries::visibleChanged, this, [this, s]() { invalidateSeries(s); });
    connect(s, &QChartSeries::colorChanged, this, [this, s]() { invalidateSeries(s); });
    invalidateSeries(s);   // 新增 = 该段需要建
}

void QChartLayer3D::removeSeries3D(QChartSeries3D* s) {
    if (!s || !m_series3D.removeOne(s)) return;
    disconnect(s, nullptr, this, nullptr);
    m_dirtySeries.remove(s);
    for (int i = m_scene.seriesSegments.size() - 1; i >= 0; --i)
        if (m_scene.seriesSegments[i].owner == s) m_scene.seriesSegments.removeAt(i);
    if (scheduleRepaint) scheduleRepaint();
}

// ===== collectPrimitives —— 纯 Numeric 图元组装 =====
void QChartLayer3D::collectAxisPrimitives() {
    // 只清轴段：系列段不在本次重建范围内（脏表只标了 axis 行）
    m_scene.axisSegments.clear();

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

        // ----- 辅助 lambda：添加一条线段（一脊 = 一段）-----
        auto addLine = [&](QChartAxis* axis, int dimIndex, qreal dimMin, qreal dimMax,
                           qreal off0, qreal off1, QChartAxis::LabelMode labelMode, const QColor& color, qreal penWidth) {
            if (!axis) return;
            QChartSegment seg;
            seg.owner = axis;
            axis->drawAtPosition(dimMin, dimMax, off0, off1, dimIndex, seg, labelMode);
            // 契约 S2：**先产图元，再接纳段**。drawAtPosition 对不可见轴会立即 return，
            // 若先建段再调用就会留下"零图元的空段"（与 2D drawGrid::addSpine 同一教训）。
            if (seg.prims.isEmpty()) return;
            for (auto& prim : seg.prims) {
                prim.color = color;
                prim.penWidth = penWidth;
            }
            m_scene.axisSegments.append(seg);
        };

        // ===== 模式解析（批次2 B）=====
        // Box 需要一个"恒等（仿射）"映射：只有这种映射下"盒 + 盒边网格"才有意义。
        // ★ 判据用投影自己声明的 isIdentityMapping()，不要按具体类型（Cartesian3D）特判——
        //   layer 不该认识任何具体坐标系。
        GridMode mode = m_gridMode;
        if (mode == GridMode::Box && !(m_projection3D && m_projection3D->isIdentityMapping())) {
            qWarning() << "QChartLayer3D: GridMode::Box 仅支持恒等映射（直角坐标）投影，"
                          "当前投影 ="
                       << projection3DTypeName(m_projection3D)
                       << "→ 自动回退 GridMode::FaceLine（批次2 B 契约）";
            mode = GridMode::FaceLine;
        }
        const bool boxMode = (mode == GridMode::Box);
        const bool faceLineMode = (mode == GridMode::FaceLine);

        // ===== 面线模式的"面"（FACE，此前 FACE-DEFERRED）=====
        // ★ 面的定义：**固定最后一维于中值、前两维做参数网格**，即 Numeric 空间里的一张"平面"，
        //   由投影决定它弯成什么。全坐标系统一约定：**"面外方向"永远是最后一维**
        //     Cartesian   (x,y,z) → 固定 z → 平面
        //     Cylindrical (r,θ,z) → 固定 z → 圆盘
        //     Spherical   (θ,φ,r) → 固定 r → 球面   ← 球坐标已按同一约定把 r 挪到最后
        //   采样密度不拍数字：用投影自己声明的 samplingSegmentsHint()（恒等→2，弯曲→32）。
        //   段归 layer（坐标系装饰，不属于任何 series/axis），且**排在主轴之前**→线与标签压在面上。
        if (faceLineMode && m_projection3D) {
            const int fixedDim = 2;       // 统一约定：固定最后一维
            const int sweepU = 0;         // 列方向
            const int sweepV = 1;         // 行方向

            const qreal lo[3] = {mn.x(), mn.y(), mn.z()};
            const qreal hi[3] = {mx.x(), mx.y(), mx.z()};
            const qreal fixedVal = 0.5 * (lo[fixedDim] + hi[fixedDim]);

            const int cols = qMax(2, m_projection3D->samplingSegmentsHint());
            const int rows = qMax(2, m_projection3D->samplingSegmentsHint());
            QVector<QVector3D> faceVerts;
            faceVerts.reserve(rows * cols);
            for (int r = 0; r < rows; ++r) {
                const qreal v = lo[sweepV] + (hi[sweepV] - lo[sweepV]) * r / (rows - 1);
                for (int c = 0; c < cols; ++c) {
                    const qreal u = lo[sweepU] + (hi[sweepU] - lo[sweepU]) * c / (cols - 1);
                    qreal comp[3];
                    comp[fixedDim] = fixedVal;
                    comp[sweepU] = u;
                    comp[sweepV] = v;
                    faceVerts.append(QVector3D(float(comp[0]), float(comp[1]), float(comp[2])));
                }
            }
            QChartSegment faceSeg;
            faceSeg.owner = this;
            faceSeg.prims.append(generatePrimitive(QChartPrimitive::Type::Grid, faceVerts,
                                                   QColor(128, 128, 128, 90), rows, cols));
            m_scene.axisSegments.append(faceSeg);
        }

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
            // 面线模式（默认）：轴线 = **dim2 轴**（面外/等值维，面固定在它的中值上，所以线穿过面），
            // dim0/dim1 固定在退化安全值；刻度标签取 dim2 轴自己的（球坐标 r、柱坐标 z）。
            const auto& cfg2 = m_axes3D->axis(2);
            if (cfg2.visible && cfg2.axis) {
                const QPair<QVector3D, QVector3D> safeSeg = QChartAxes3D::faceLineSegment(mn, mx);
                addLine(cfg2.axis, 2, safeSeg.first.z(), safeSeg.second.z(),
                        safeSeg.first.x(), safeSeg.first.y(), QChartAxis::LabelMode::Tickwise,
                        spineCol[2], 2.0);
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

    QCHART_DIRTY_MSG(QStringLiteral("  collectAxis: rebuilt=%1 段").arg(m_scene.axisSegments.size()));
}

void QChartLayer3D::collectSeriesPrimitives() {
    int rebuilt = 0;
    // 1) 只重建脏系列对应的段（未脏的段一个字节不动）
    for (QChartSeries3D* s : m_series3D) {
        if (!s) continue;
        const bool dirty = m_dirtySeries.contains(s);
        int idx = -1;
        for (int i = 0; i < m_scene.seriesSegments.size(); ++i)
            if (m_scene.seriesSegments[i].owner == s) { idx = i; break; }

        // 契约 S6①：作者意图在收集期过滤——不可见的系列压根不入场景
        if (!s->isVisible()) {
            if (idx >= 0) m_scene.seriesSegments.removeAt(idx);
            continue;
        }
        if (!dirty && idx >= 0) continue;   // 未脏且已有段 → 保持原样

        QCHART_DIRTY_MSG(QStringLiteral("  series<'%1'> 被重新收集").arg(s->name()));
        QChartSegment seg;
        seg.owner = s;
        s->collectPrimitives(seg, m_axisX, m_axisY, m_axisZ);
        if (seg.prims.isEmpty()) {          // 空段丢弃
            if (idx >= 0) m_scene.seriesSegments.removeAt(idx);
            continue;
        }
        if (idx >= 0) m_scene.seriesSegments[idx] = seg;
        else m_scene.seriesSegments.append(seg);
        ++rebuilt;
    }

    // 2) 按 m_series3D 顺序归并（增删系列走同一条路）
    QVector<QChartSegment> ordered;
    ordered.reserve(m_scene.seriesSegments.size());
    for (QChartSeries3D* s : m_series3D) {
        for (const QChartSegment& seg : m_scene.seriesSegments)
            if (seg.owner == s) { ordered.append(seg); break; }
    }
    m_scene.seriesSegments = ordered;
    QCHART_DIRTY_MSG(QStringLiteral("  collectSeries: rebuilt=%1 / %2 段")
                         .arg(rebuilt).arg(m_scene.seriesSegments.size()));
}

