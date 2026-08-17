// QChartWidget3D.cpp —— 3D 图表控件实现
// 交互（orbit/dolly/panTarget）+ 3D 悬停命中（屏幕近邻→dataIndex→(u,v)）+ 联动信号 +
// buildScreenScene/buildExportScene 重写（填 3D 段）。
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
}

// ===== 相机 / 投影 / 图层 =====
void QChartWidget3D::setCamera3D(std::unique_ptr<QChartCamera3D> cam) {
    if (!cam) return;
    m_camera3D = std::move(cam);
    m_camera3D->setParent(this);
    invalidateForeground();
}

void QChartWidget3D::setProjection3D(std::unique_ptr<QChartProjection3D> proj) {
    if (!proj) return;
    m_projection3D = std::move(proj);
    for (QChartLayer3D* g : m_layers3D)
        if (g) g->setProjection3D(m_projection3D.get());
    fitWorld();   // 自动按 defaultDataBounds fit 相机（§8.3）
}

void QChartWidget3D::addLayer3D(QChartLayer3D* g) {
    if (!g || m_layers3D.contains(g)) return;
    m_layers3D.append(g);
    addLayer(g);   // 基类接线：主题/图例/调色板/所有权（复用）
    g->setProjection3D(m_projection3D.get());
    invalidateForeground();
}

// ===== 坐标 / fit =====
QPointF QChartWidget3D::worldToPixel(const QVector3D& w) const {
    if (!m_camera3D) return QPointF(qQNaN(), qQNaN());
    return m_camera3D->project(w, m_plotArea).screen;
}

void QChartWidget3D::fitWorld() {
    if (!m_projection3D || !m_camera3D) return;
    const auto def = m_projection3D->defaultDataBounds();
    m_worldBounds = m_projection3D->computeWorldBounds(def.first, def.second);
    qreal aspect = 1.0;
    if (m_plotArea.height() > 0.0) aspect = m_plotArea.width() / m_plotArea.height();
    m_camera3D->fitToBounds(m_worldBounds, aspect);
    invalidateForeground();
}

// ===== 交互（D-3D-4：手势 → Camera 几何）=====
void QChartWidget3D::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_orbitDrag = true;
        m_pressPos = e->position();
        m_lastPos = e->position();
        e->accept();
        return;
    }
    if (e->button() == Qt::RightButton) {
        m_panDrag = true;
        m_lastPos = e->position();
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
                          delta.y() * m_orbitSensitivity);   // 垂直 = deltaPitch
        m_lastPos = pos;
        invalidateForeground();
        e->accept();
        return;
    }
    if (m_panDrag && m_camera3D) {
        const QPointF delta = pos - m_lastPos;
        // 像素 → World：经当前视距换算（目标平面上的世界单位/像素）
        qreal worldPerPixel = 1.0;
        if (m_plotArea.height() > 0.0) {
            if (m_camera3D->projectionMode() == QChartCamera3D::ProjectionMode::Perspective) {
                const qreal dist = (m_camera3D->position() - m_camera3D->lookAt()).length();
                const qreal viewH = 2.0 * dist * qTan(qDegreesToRadians(m_camera3D->fovY()) * 0.5);
                worldPerPixel = viewH / m_plotArea.height();
            } else {
                worldPerPixel = m_camera3D->orthographicBox().height() / m_plotArea.height();
            }
        }
        m_camera3D->panTarget(-delta.x() * worldPerPixel, delta.y() * worldPerPixel);
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
    if (e->button() == Qt::RightButton && m_panDrag) {
        m_panDrag = false;
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
                if (prim.dataIndex < 0) continue;
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
