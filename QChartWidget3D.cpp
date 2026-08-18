// QChartWidget3D.cpp —— 3D 图表控件实现
// 交互（R6：orbit 拖拽 + dolly 滚轮；平移无鼠标手势）+ 3D 悬停命中（屏幕近邻→dataIndex→(u,v)）
// + 联动信号 + buildScreenScene/buildExportScene 重写（填 3D 段）。
#include "QChartWidget3D.h"
#include "QCartesianProjection.h"   // 构造占位 2D 投影（§8.3 ⚠）
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <cmath>

QChartWidget3D::QChartWidget3D(QWidget* parent) : QChartWidget(parent) {
    // §8.3 ⚠：基类流程需要 m_projection 非空（addLayer 接线/布局/2D 相机初始化）；
    // 2D Cartesian 仅占位——渲染走 camera3D/layers3D 的 3D 段
    setProjection(std::make_unique<QCartesianProjection>());
    m_camera3D = std::make_unique<QChartCamera3D>(this);
    setMouseTracking(true);

    // §9：视图变化 → 反算 dataBounds + 推轴盒 + 重绘（每帧不重算，design_3d_axes.md §2.2/§9）
    connect(m_camera3D.get(), &QChartCamera::viewChanged, this, [this]() {
        recomputeDataBounds3D();
        pushAxesDataBoxToLayers();
        invalidateForeground();
    });
}

// ===== 相机 / 投影 / 图层 =====
void QChartWidget3D::setCamera3D(std::unique_ptr<QChartCamera3D> cam) {
    if (!cam) return;
    m_camera3D = std::move(cam);
    m_camera3D->setParent(this);
    // 重连视图变化钩子（旧相机随 unique_ptr 销毁自动断开）
    connect(m_camera3D.get(), &QChartCamera::viewChanged, this, [this]() {
        recomputeDataBounds3D();
        pushAxesDataBoxToLayers();
        invalidateForeground();
    });
    recomputeDataBounds3D();
    pushAxesDataBoxToLayers();
    invalidateForeground();
}

void QChartWidget3D::setProjection3D(std::unique_ptr<QChartProjection3D> proj) {
    if (!proj) return;
    m_projection3D = std::move(proj);
    for (QChartLayer3D* g : m_layers3D)
        if (g) g->setProjection3D(m_projection3D.get());
    fitWorld();   // A3 全链：按 defaultDataBounds/域盒 fit 相机（§8.3）
}

void QChartWidget3D::addLayer3D(QChartLayer3D* g) {
    if (!g || m_layers3D.contains(g)) return;
    m_layers3D.append(g);
    addLayer(g);   // 基类接线：主题/图例/调色板/所有权（复用）
    g->setProjection3D(m_projection3D.get());
    pushAxesDataBoxToLayers();   // 新图层继承当前轴盒（反算或 A9 锚定域盒）
    invalidateForeground();
}

// ===== 坐标 / fit =====
QPointF QChartWidget3D::worldToPixel(const QVector3D& w) const {
    if (!m_camera3D) return QPointF(qQNaN(), qQNaN());
    return m_camera3D->project(w, m_plotArea).screen;
}

// ===== A3 域盒链 =====
void QChartWidget3D::setDomainBox(const QVector3D& dataMin, const QVector3D& dataMax) {
    m_domainMin = dataMin;
    m_domainMax = dataMax;
    fitWorld();   // 立即 fit + 反算 + 推轴盒
}

void QChartWidget3D::clearDomainBox() {
    if (!m_domainMin) return;
    m_domainMin.reset();
    m_domainMax.reset();
    fitWorld();
}

bool QChartWidget3D::hasDomainBox() const {
    return m_domainMin.has_value() && m_domainMax.has_value();
}

std::pair<QVector3D, QVector3D> QChartWidget3D::computeSeriesDataBounds() const {
    qreal minX = qInf(), maxX = -qInf(), minY = qInf(), maxY = -qInf();
    qreal minZ = qInf(), maxZ = -qInf();
    for (const QChartLayer3D* g : m_layers3D) {
        if (!g) continue;
        for (const QChartSeries3D* s : g->series3DList()) {
            if (!s) continue;
            const QVector<QDataPoint3D>& pts = s->points();
            for (const QDataPoint3D& d : pts) {
                const qreal n0 = g->axisX() ? g->axisX()->toNumeric(d.x()) : d.x().toDouble();
                const qreal n1 = g->axisY() ? g->axisY()->toNumeric(d.y()) : d.y().toDouble();
                const qreal n2 = g->axisZ() ? g->axisZ()->toNumeric(d.z()) : d.z().toDouble();
                if (!std::isfinite(n0) || !std::isfinite(n1) || !std::isfinite(n2)) continue;
                minX = qMin(minX, n0); maxX = qMax(maxX, n0);
                minY = qMin(minY, n1); maxY = qMax(maxY, n1);
                minZ = qMin(minZ, n2); maxZ = qMax(maxZ, n2);
            }
        }
    }
    if (qIsInf(minX)) return { QVector3D(0, 0, 0), QVector3D(0, 0, 0) };   // 无有效点 → 空盒（链回退）
    return { QVector3D(minX, minY, minZ), QVector3D(maxX, maxY, maxZ) };
}

std::pair<QVector3D, QVector3D> QChartWidget3D::resolveDataBox() const {
    // A3 链：显式域盒 > 数据包围盒 > defaultDataBounds
    if (m_domainMin && m_domainMax)
        return { *m_domainMin, *m_domainMax };
    const auto seriesBox = computeSeriesDataBounds();
    if (seriesBox.first.x() <= seriesBox.second.x() &&
        seriesBox.first.y() <= seriesBox.second.y() &&
        seriesBox.first.z() <= seriesBox.second.z() &&
        !(seriesBox.first == seriesBox.second && seriesBox.first == QVector3D(0, 0, 0)))
        return seriesBox;
    if (m_projection3D)
        return m_projection3D->defaultDataBounds();
    return { QVector3D(0, 0, 0), QVector3D(10, 10, 10) };
}

void QChartWidget3D::fitWorld() {
    if (!m_projection3D || !m_camera3D) return;
    // A3 全链（§3）：resolveDataBox → computeWorldBounds → setViewCubeToFit → 反算 → 推轴盒 → 重绘
    const auto dataBox = resolveDataBox();
    m_anchorBox = dataBox;   // A9 兜底缓存（dataBounds3D 无效时轴/网格锚定此盒，静态）
    m_worldBounds = m_projection3D->computeWorldBounds(dataBox.first, dataBox.second);
    m_camera3D->setViewCubeToFit(m_worldBounds);   // R5：viewCube = 目标盒（orientation/fovY 保持）
    recomputeDataBounds3D();
    pushAxesDataBoxToLayers();
    invalidateForeground();
}

// ===== 视图→dataBounds 反算（§2.2；R5 无逆矩阵/unproject）=====
void QChartWidget3D::recomputeDataBounds3D() {
    m_dataBounds3DValid = false;
    m_dataBounds3DMin = QVector3D(0, 0, 0);
    m_dataBounds3DMax = QVector3D(0, 0, 0);
    if (!m_projection3D || !m_camera3D) return;
    const QChartWorldBox vc = m_camera3D->viewCube();

    // 笛卡尔快速通道（§5.4 用户定案）：恒等映射免采样，dataBounds = viewCube 直接反算（0 次 fromWorld）
    if (m_projection3D->isIdentityMapping()) {
        m_dataBounds3DMin = vc.min;
        m_dataBounds3DMax = vc.max;
        m_dataBounds3DValid = true;
        return;
    }

    // 通用路径：5×5×5=125 点网格采样（每轴 5 档：min/25%/50%/75%/max）→ fromWorld 聚合
    const qreal levels[5] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
    qreal minX = qInf(), maxX = -qInf(), minY = qInf(), maxY = -qInf();
    qreal minZ = qInf(), maxZ = -qInf();
    for (int i = 0; i < 5; ++i) {
        const qreal x = vc.min.x() + levels[i] * (vc.max.x() - vc.min.x());
        for (int j = 0; j < 5; ++j) {
            const qreal y = vc.min.y() + levels[j] * (vc.max.y() - vc.min.y());
            for (int k = 0; k < 5; ++k) {
                const qreal z = vc.min.z() + levels[k] * (vc.max.z() - vc.min.z());
                const QVector3D num = m_projection3D->fromWorld(QVector3D(x, y, z));
                if (!std::isfinite(num.x()) || !std::isfinite(num.y()) || !std::isfinite(num.z()))
                    continue;   // 非有限（奇点 NaN 等）跳过
                minX = qMin(minX, num.x()); maxX = qMax(maxX, num.x());
                minY = qMin(minY, num.y()); maxY = qMax(maxY, num.y());
                minZ = qMin(minZ, num.z()); maxZ = qMax(maxZ, num.z());
            }
        }
    }
    if (qIsInf(minX)) return;   // 全 NaN → Valid=false（A9 兜底：轴/网格用锚定域盒）

    m_dataBounds3DMin = QVector3D(minX, minY, minZ);
    m_dataBounds3DMax = QVector3D(maxX, maxY, maxZ);
    m_dataBounds3DValid = true;
}

void QChartWidget3D::pushAxesDataBoxToLayers() {
    // dataBounds3D 有效 → 用它（视图驱动）；否则 A9 锚定域盒（静态参照系，§2.4）
    const QVector3D mn = m_dataBounds3DValid ? m_dataBounds3DMin : m_anchorBox.first;
    const QVector3D mx = m_dataBounds3DValid ? m_dataBounds3DMax : m_anchorBox.second;
    for (QChartLayer3D* g : m_layers3D)
        if (g) g->setAxesDataBox(mn, mx);
}

// ===== 交互（D-3D-4：手势 → viewCube/orientation；R6：平移无鼠标手势）=====
void QChartWidget3D::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_orbitDrag = true;
        m_pressPos = e->position();
        m_lastPos = e->position();
        e->accept();
        return;
    }
    // R6：平移不做鼠标手势（panViewCube 仅 API/动画驱动）
    if (e->button() == Qt::RightButton) {
        e->accept();
        return;
    }
    QChartWidget::mousePressEvent(e);
}

void QChartWidget3D::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    if (m_orbitDrag && m_camera3D) {
        const QPointF delta = pos - m_lastPos;
        m_camera3D->orbit(delta.x() * m_orbitSensitivity,   // 水平 = deltaYaw
                          delta.y() * m_orbitSensitivity);   // 垂直 = deltaPitch（viewCube 不动，R6）
        m_lastPos = pos;
        invalidateForeground();
        e->accept();
        return;
    }
    // 非拖拽：更新悬停命中（联动信号）
    updateHover(pos);
    e->accept();
}

void QChartWidget3D::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_orbitDrag) {
        m_orbitDrag = false;
        // 左键单击（拖动距离 <3px）且悬停命中 → 选中
        const QPointF moved = e->position() - m_pressPos;
        if (QPointF::dotProduct(moved, moved) < 9.0 && m_hoverActive)
            emit uvSelected(m_lastHoverUV.x(), m_lastHoverUV.y());
        e->accept();
        return;
    }
    QChartWidget::mouseReleaseEvent(e);
}

void QChartWidget3D::wheelEvent(QWheelEvent* e) {
    if (!m_camera3D) { QChartWidget::wheelEvent(e); return; }
    const qreal notches = e->angleDelta().y() / 120.0;
    if (notches == 0.0) { e->accept(); return; }
    m_camera3D->dolly(qExp(-notches * m_dollySensitivity));   // factor = exp(-delta·k)
    invalidateForeground();
    e->accept();
}

void QChartWidget3D::leaveEvent(QEvent* e) {
    if (m_hoverActive) {
        m_hoverActive = false;
        emit uvHoveredEnd();
    }
    QChartWidget::leaveEvent(e);
}

// ===== 3D 悬停简化版（§8.3 修订）：屏幕近邻 → dataIndex → Data (u,v) =====
qreal QChartWidget3D::distToPrimitive(const QPointF& pos, const QChartPrimitive& prim) {
    if (prim.type == QChartPrimitive::Type::Point) {
        const QPointF d = pos - prim.a;
        return std::sqrt(QPointF::dotProduct(d, d));
    }
    // 点到线段距离
    const QPointF ab = prim.b - prim.a;
    const qreal len2 = QPointF::dotProduct(ab, ab);
    if (len2 < 1e-12) {
        const QPointF d = pos - prim.a;
        return std::sqrt(QPointF::dotProduct(d, d));
    }
    qreal t = QPointF::dotProduct(pos - prim.a, ab) / len2;
    t = qBound<qreal>(0.0, t, 1.0);
    const QPointF proj = prim.a + ab * t;
    const QPointF d = pos - proj;
    return std::sqrt(QPointF::dotProduct(d, d));
}

void QChartWidget3D::updateHover(const QPointF& pos) {
    if (!m_camera3D) return;

    // 与渲染同路径收集图元（逐系列，命中即可定位系列 → dataIndex → Data (u,v)）
    QChartSeries3D* hitSeries = nullptr;
    int hitIndex = -1;
    qreal bestDist = 8.0;   // 阈值 8px（§8.3）
    for (QChartLayer3D* g : m_layers3D) {
        if (!g) continue;
        const ProjectFn3D fn = g->makeProjectFn(m_camera3D.get(), m_plotArea);
        for (QChartSeries3D* s : g->series3DList()) {
            if (!s || !s->isVisible()) continue;
            QVector<QChartPrimitive> items;
            s->collectPrimitives(fn, items);
            for (const QChartPrimitive& prim : items) {
                if (prim.dataIndex < 0 || prim.layer != QChartPrimitive::Layer::Series)
                    continue;   // §7.4：hover 只扫 Series 层图元（Grid/ForegroundDecor 排除）
                const qreal d = distToPrimitive(pos, prim);
                if (d < bestDist) {
                    bestDist = d;
                    hitSeries = s;
                    hitIndex = prim.dataIndex;
                }
            }
        }
    }

    if (hitSeries && hitIndex >= 0) {
        const QDataPoint3D pt = hitSeries->at(hitIndex);
        const qreal u = pt.x().toDouble();
        const qreal v = pt.y().toDouble();
        if (!m_hoverActive || m_lastHoverUV != QPointF(u, v)) {
            m_lastHoverUV = QPointF(u, v);
            m_hoverActive = true;
            emit uvHovered(u, v);
        }
    } else if (m_hoverActive) {
        m_hoverActive = false;
        emit uvHoveredEnd();
    }
}

// ===== 场景钩子（§8.2 重写：填 3D 段，2D 字段留默认）=====
QChartScene QChartWidget3D::buildScreenScene() const {
    QChartScene scene;
    scene.plotArea = m_plotArea;
    scene.backgroundColor = backgroundColor();   // 有效色（override 或主题默认）
    scene.camera3D = m_camera3D.get();
    scene.layers3D = m_layers3D;
    scene.worldBounds = m_worldBounds;
    return scene;
}

QChartScene QChartWidget3D::buildExportScene(QChartExportScope scope, const QSize& size,
                                             QSizeF& outDeviceSize) const {
    // 复用基类导出尺寸/plotArea/透明背景逻辑，注入 3D 段（导出 3D 低成本，验收不要求）
    QChartScene scene = QChartWidget::buildExportScene(scope, size, outDeviceSize);
    scene.camera3D = m_camera3D.get();
    scene.layers3D = m_layers3D;
    scene.worldBounds = m_worldBounds;
    scene.projection = nullptr;   // 3D 渲染路径不用 2D 字段
    scene.axes.clear();
    scene.layers.clear();
    scene.dataBounds = QRectF();
    scene.viewRect = QRectF();
    return scene;
}
