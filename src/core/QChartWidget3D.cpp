// QChartWidget3D.cpp —— 3D 图表控件实现
// 交互（R6：orbit 拖拽 + dolly 滚轮；平移无鼠标手势）+ 3D 悬停命中（屏幕近邻→dataIndex→(u,v)）
// + 联动信号 + buildScreenScene/buildExportScene 重写（填 3D 段）
// + Phase 3 GL 宿主（t42，design_phase3.md §2.2：内嵌 QOpenGLWidget 组合；QPainter 路径共存，
//   后端开关属实现⑤ t48；shader/主 pass 属实现③ t44）。
#include "QChartWidget3D.h"
#include "QCartesianProjection.h"   // 构造占位 2D 投影（§8.3 ⚠）
#include "QOpenGLChartRenderer.h"
#include "QChartGL.h"
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <cmath>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logGlHost, "chart.glhost.debug")   // GlHost 内部调试（§5.1/§6.1）

// ===== A9 兜底：QCHART_GL=0 环境变量强制 QPainter（§2.2；GL 默认、QPainter 保底）=====
// 全局一次判定：环境变量存在且为 0 → GL 禁用（demo/测试可整进程回退，不弹 GL 窗口）
static bool glForcedOffByEnv() {
    static const bool s_off =
        qEnvironmentVariableIsSet("QCHART_GL") && qEnvironmentVariableIntValue("QCHART_GL") == 0;
    return s_off;
}

// ===== GlHost：内嵌 QOpenGLWidget（A2 组合；§2.2）=====
// ⚠ 不重写 paintEvent（QOpenGLWidget 官方模式：GL 内容画在 paintGL；QPainter overlay 在 paintGL
//   内以 widget 为 QPaintDevice 直接画——本骨架阶段 GL 清透明，QPainter 内容透出，两者共存）。
class QChartWidget3D::GlHost : public QOpenGLWidget {
public:
    explicit GlHost(QChartWidget3D* outer) : QOpenGLWidget(outer), m_outer(outer) {
        setAttribute(Qt::WA_TransparentForMouseEvents);   // 事件透传外层 QChartWidget3D（§2.2）
        setFormat(QChartGL::surfaceFormat());             // 3.3 Core + depth 24（§7.3）
        QChartGL::registerHost();                         // 实例计数 → 共享根/程序池生命周期（§7.3）
        if (QOpenGLContext* sc = QChartGL::sharedContext())
            shareContextIfAvailable(sc);
    }
    ~GlHost() override { QChartGL::unregisterHost(); }

protected:
    void initializeGL() override {
        // t44 主 pass 落地：初始化渲染器（上下文检查 + shader 编译）；就绪则保持显示。
        // 上下文不可用（无 GL/EGL 故障）→ 隐藏，回退纯 QPainter（§5.1 ⚠ 透明语义教训）
        if (m_outer->m_glRenderer)
            m_outer->m_glRenderer->initializeGL();
        if (!m_outer->m_glRenderer || !m_outer->m_glRenderer->isReady())
            hide();
    }
    void paintGL() override {
        // GL 主 pass（§5.1：不透明清屏 + Grid/Series/Decor 分层）
        const QChartScene scene = m_outer->buildScreenScene();
        if (!m_outer->m_glRenderer) return;
        m_outer->m_glRenderer->paintGL(scene);
        // §6 Overlay：GL 不透明底色之上 QPainter 画 billboard 标签 + 图例（官方模式：paintGL 内
        //   QPainter；不重写 paintEvent）。标签 screenPos 为外层 plotArea 坐标 → 平移至本部件局部。
        QPainter p(this);
        // p.setClipRect(QRectF(0, 0, scene.plotArea.width(), scene.plotArea.height()));
        p.setPen(Qt::yellow);
        p.translate(-scene.plotArea.topLeft());
        qCDebug(logGlHost) << "GlHost geometry:" << geometry();
        qCDebug(logGlHost) << "plotArea:" << scene.plotArea;
        qCDebug(logGlHost) << "After translate: painter transform" << p.transform();
        p.drawRect(scene.plotArea);   // §6.1 调试黄框（可选）
        QFont f = p.font();
        for (const QChartTextLabel& lbl : m_outer->m_glRenderer->labels()) {
            f.setPointSizeF(lbl.fontSize);
            f.setBold(lbl.isTitle);
            p.setFont(f);
            p.setPen(lbl.color);
            p.drawText(QRectF(lbl.screenPos, QSizeF(0, 0)),
                       int(lbl.anchor) | Qt::TextDontClip, lbl.text);
        }
        if (scene.legend && scene.legend->isVisible())
            scene.legend->draw(&p, scene.plotArea, scene.legendItems);
    }

    void resizeGL(int w, int h) override {
        if (m_outer->m_glRenderer) m_outer->m_glRenderer->resizeGL(w, h);
    }

private:
    /// 多实例共享（A2）：Qt ≥6.5 提供 QOpenGLWidget::setShareContext（须在 widget 上下文创建前调用）；
    /// 6.4.x 无此 API → 本实例上下文独立（共享根仍供程序池 t44 使用；单实例场景不受影响）
    void shareContextIfAvailable(QOpenGLContext* sc) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        sc->setShareContext(sc);
#else
        Q_UNUSED(sc);
#endif
    }
    QChartWidget3D* m_outer;
};

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

    // Phase 3 GL 宿主（t42）：内嵌 QOpenGLWidget 组合 + 渲染器挂接（先与 QPainter 路径共存）
    m_glHost = std::make_unique<GlHost>(this);
    m_glRenderer = new QOpenGLChartRenderer(m_glHost.get());
    // t48：QCHART_GL=0 强制 QPainter 兜底（A9；环境变量存在且为 0）
    m_renderBackend = glForcedOffByEnv() ? RenderBackend::QPainter : RenderBackend::OpenGL;
    // t44 恢复显示（队长统筹方案 A 的 TODO）：主 pass 已落地（§5.1 不透明清屏）→ GL 可用时
    // 显示 GlHost；无 GL（offscreen/EGL 故障，共享根创建失败）或 QCHART_GL=0 → 隐藏回退纯 QPainter——
    // QOpenGLWidget 原生子窗口的透明像素会露桌面，绝不透明示人（§5.1 ⚠ 透明语义教训）
    if (m_renderBackend == RenderBackend::OpenGL && QChartGL::sharedContext())
        m_glHost->show();
    else
        m_glHost->hide();
    layoutGlHost();
}

QChartWidget3D::~QChartWidget3D() {
    // GL 渲染器为裸指针（设计 §2.2：GlHost 生命周期内）→ 先于 GlHost 销毁
    delete m_glRenderer;
    m_glRenderer = nullptr;
    // m_glHost（unique_ptr）在 dtor 体后销毁 → GlHost 析构 unregisterHost（§7.3 引用计数）
}

void QChartWidget3D::setRenderBackend(RenderBackend b) {
    // A9：QCHART_GL=0 强制 QPainter（GL 禁用，setRenderBackend(OpenGL) 亦被压制——环境变量兜底优先）
    if (glForcedOffByEnv()) b = RenderBackend::QPainter;
    if (m_renderBackend == b) return;
    m_renderBackend = b;
    // ⚠ §5.1：GlHost 仅 GL 就绪且后端为 OpenGL 时显示（不透明主 pass 闭环）；否则隐藏回退纯 QPainter
    if (m_glHost) {
        const bool showGl = (b == RenderBackend::OpenGL) && m_glRenderer && m_glRenderer->isReady();
        showGl ? m_glHost->show() : m_glHost->hide();
    }
    invalidateForeground();   // 后端切换 → 重绘（拾取分支随之切换，§8.2 统一后端原则）
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

void QChartWidget3D::invalidateBackground() {
    QChartWidget::invalidateBackground();
    if(m_glHost) m_glHost->update();   // Phase 3 GL 宿主：背景重绘 → GlHost update，否则子类不会更新！！
}

void QChartWidget3D::invalidateForeground() {
    QChartWidget::invalidateForeground();
    if(m_glHost) m_glHost->update();   // Phase 3 GL 宿主：前景重绘 → GlHost update，否则子类不会更新！！
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

// ===== 3D 悬停（§8.2 后端分支；R5 语义不变）=====
// ⚠ 后端统一原则（用户定案，§8.2）：渲染与拾取必须同后端——GL 后端就绪时走 ID 帧拾取
//   （pickIdAt + hitTestGPU 解码）；GL 未就绪（QPainter 兜底渲染）→ CPU 屏幕近邻（A7 保留），禁止混搭。
// CPU 路径（Phase 2 保留）：Series 层过滤/点与线段距离/8px 阈值/dataIndex 透传（QChartHitTester）；
// 逐系列收集以定位系列 → (u,v)，跨系列全局最近语义保持。
void QChartWidget3D::updateHover(const QPointF& pos) {
    if (!m_camera3D) return;

    QChartSeries3D* hitSeries = nullptr;
    int hitIndex = -1;

    // GL 后端：ID 帧拾取（§5.3 三闸限流在 pickIdAt 内；m_glReady 守卫保证与渲染同后端）
    const bool glActive = (m_renderBackend == RenderBackend::OpenGL)
                          && m_glRenderer && m_glRenderer->isReady();
    if (glActive) {
        const QChartScene scene = buildScreenScene();
        // GlHost 几何 == plotArea → 外层坐标换算为宿主局部坐标（ID 帧视口 = plotArea）
        const QPoint local = pos.toPoint() - scene.plotArea.topLeft().toPoint();
        const QRgb id = m_glRenderer->pickIdAt(local, scene);
        const QChartHitTester::HitResult r =
            QChartHitTester::hitTestGPU(qRed(id), qGreen(id), qBlue(id),
                                        m_glRenderer->pickTable());
        if (r.dataIndex >= 0 && r.series) {
            hitSeries = qobject_cast<QChartSeries3D*>(r.series);
            hitIndex = r.dataIndex;
        }
    } else {
        // QPainter 后端：CPU 屏幕近邻（8px 阈值，§8.3；A7 保留路径）
        qreal bestDist = 8.0;
        for (QChartLayer3D* g : m_layers3D) {
            if (!g) continue;
            const ProjectFn3D fn = g->makeProjectFn(m_camera3D.get(), m_plotArea);
            for (QChartSeries3D* s : g->series3DList()) {
                if (!s || !s->isVisible()) continue;
                QVector<QChartPrimitive> items;
                s->collectPrimitives(fn, items);
                const QChartHitTester::HitResult r = QChartHitTester::hitTest(pos, items, bestDist);
                if (r.dataIndex >= 0) {
                    // 收紧全局阈值（保持跨系列全局最近语义）：命中距离 = 该 dataIndex 图元最近距离
                    hitSeries = s;
                    hitIndex = r.dataIndex;
                    qreal d = bestDist;
                    for (const QChartPrimitive& prim : items)
                        if (prim.dataIndex == r.dataIndex)
                            d = qMin(d, QChartHitTester::distanceToPrimitive(pos, prim));
                    bestDist = d;
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
    scene.legend = legend();                     // §6 overlay：图例（GL 路径同源绘制）
    scene.legendItems = legendItems();
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

// ===== 布局：GlHost 跟随 plotArea（Phase 3 GL 宿主；基类 resizeEvent 逻辑保留）=====
void QChartWidget3D::resizeEvent(QResizeEvent* e) {
    QChartWidget::resizeEvent(e);   // 基类：invalidate 缓存 + layoutAxes（更新 m_plotArea）
    layoutGlHost();
}

void QChartWidget3D::layoutGlHost() {
    if (!m_glHost) return;
    // 覆盖 plotArea（3D 画布）；plotArea 未就绪（首次布局前）→ 覆盖整个 widget
    const QRect area = (m_plotArea.isValid() && m_plotArea.width() > 1 && m_plotArea.height() > 1)
        ? m_plotArea.toRect()
        : rect();
    m_glHost->setGeometry(area);
}
