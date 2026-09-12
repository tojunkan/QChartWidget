// QChartWidget3D.cpp —— 3D 图表控件实现（批次 B2：轻量 3D 容器）
#include "QChartWidget3D.h"
#include "QChartCamera3D.h"
#include "QChartProjection3D.h"
#include "QPainterChartRenderer.h"
#include "QOpenGLChartRenderer.h"
#include <QImage>
#include <QPainter>
#include <QMouseEvent>     // 4f：交互接线
#include <QWheelEvent>
#include <QtMath>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logWidget3D, "chart.widget3d")

// 4g-fix（t58 F1）：把一根轴的范围/样式类信号统一接到层置脏（三维与二维同构；层为 QObject，
// 其销毁自动断连）。定义在 .cpp 局部，避免改动头文件（t59 inScope 未含 QChartWidget3D.h）。
static void wireAxisToLayerDirty(QChartAxis* a, QChartAbstractLayer* layer)
{
    if (!a || !layer) return;
    auto mark = [layer]() { layer->invalidateData(); };
    QObject::connect(a, &QChartAxis::rangeChanged, layer, [mark]() { mark(); });
    QObject::connect(a, &QChartAxis::tickCountChanged, layer, [mark]() { mark(); });
    QObject::connect(a, &QChartAxis::subTickCountChanged, layer, [mark]() { mark(); });
    QObject::connect(a, &QChartAxis::styleChanged, layer, [mark]() { mark(); });
    QObject::connect(a, &QChartAxis::visibleChanged, layer, [mark]() { mark(); });
}

// ===== 构造 / 析构 =====

QChartWidget3D::QChartWidget3D(QWidget* parent)
    : QChartWidget(parent)
{
    // 单 layer3D 托管（QObject 子；基类 addLayer 接线——容器非持有列表）
    m_layer3D = new QChartLayer3D(this);
    QChartWidget::addLayer(m_layer3D);

    // 默认三轴（2D QValueAxis 实例，3D 数值化共用；可经 setAxisX3D/Y3D/Z3D 替换）
    m_axisX3D = new QValueAxis(this, Qt::AlignBottom);
    m_axisY3D = new QValueAxis(this, Qt::AlignLeft);
    m_axisZ3D = new QValueAxis(this, Qt::AlignBottom);
    for (QValueAxis* a : {m_axisX3D, m_axisY3D, m_axisZ3D}) {
        a->setRange(0.0, 10.0);
        a->setTickCount(5);
        a->setColor(Qt::black);
    }
    m_layer3D->setAxisX(m_axisX3D);
    m_layer3D->setAxisY(m_axisY3D);
    m_layer3D->setAxisZ(m_axisZ3D);
    // 4g：三维数值侧驱动 → 层置脏（三维轴范围变化不必然改相机，必须显式标记重收集）
    // 4g-fix（t58 F1）：轴样式类信号（tickCount/subTickCount/style/visible）同样置脏（网格/刻度/标签依赖它们）
    for (QChartAxis* a : { static_cast<QChartAxis*>(m_axisX3D), static_cast<QChartAxis*>(m_axisY3D),
                           static_cast<QChartAxis*>(m_axisZ3D) })
        wireAxisToLayerDirty(a, m_layer3D);
}

QChartWidget3D::~QChartWidget3D() = default;

// ===== 图层 =====

void QChartWidget3D::addLayer3D(QChartLayer3D* g)
{
    if (!g) return;
    QChartWidget::addLayer(g);   // 基类容器接线（布局/渲染遍历均经基类 m_layers）
}

// ===== 三轴绑定（透传 layer3D；默认轴替换由调用方管理生命周期——非持有约定同基类 addAxis）=====

// 4g：轴替换后同样接上“轴范围/样式变化 → 层置脏”（维度数值侧 → 重收集）
void QChartWidget3D::setAxisX3D(QChartAxis* a)
{
    if (!m_layer3D) return;
    m_layer3D->setAxisX(a);
    wireAxisToLayerDirty(a, m_layer3D);
}
void QChartWidget3D::setAxisY3D(QChartAxis* a)
{
    if (!m_layer3D) return;
    m_layer3D->setAxisY(a);
    wireAxisToLayerDirty(a, m_layer3D);
}
void QChartWidget3D::setAxisZ3D(QChartAxis* a)
{
    if (!m_layer3D) return;
    m_layer3D->setAxisZ(a);
    wireAxisToLayerDirty(a, m_layer3D);
}
QChartAxis* QChartWidget3D::axisX3D() const { return m_layer3D ? m_layer3D->axisX() : nullptr; }
QChartAxis* QChartWidget3D::axisY3D() const { return m_layer3D ? m_layer3D->axisY() : nullptr; }
QChartAxis* QChartWidget3D::axisZ3D() const { return m_layer3D ? m_layer3D->axisZ() : nullptr; }

// ===== 域盒 =====

void QChartWidget3D::setDomainBox(const QCube& box)
{
    m_domainBoxSet = true;   // 4b：数值落在轴上（layer3D->setDataBounds 写三轴范围）
    if (m_layer3D)
        m_layer3D->setDataBounds(box);   // 4c：QCube 主签名
    fitWorld();
}

QCube QChartWidget3D::domainBox() const
{
    // 4c：域盒访问器（QCube）——按需从 layer3D 的三轴组装，无长期持有
    return (m_layer3D && m_layer3D->axes3D()) ? m_layer3D->axes3D()->dataBounds() : QCube();
}

void QChartWidget3D::clearDomainBox()
{
    m_domainBoxSet = false;
    if (m_layer3D)
        m_layer3D->setDataBounds(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));   // 4c：QCube 默认盒
    fitWorld();
}

// ===== 投影 =====

void QChartWidget3D::setProjection3D(const QChartProjection3D* proj)
{
    if (m_layer3D)
        m_layer3D->setProjection3D(proj);   // layer 内部（采样提示）；scene3D.projection 由 pushContext 注入
    fitWorld();
}

const QChartProjection3D* QChartWidget3D::projection3D() const
{
    return m_layer3D ? m_layer3D->projection3D() : nullptr;
}

// ===== 网格模式（批次2 B：转发 layer3D）=====

void QChartWidget3D::setGridMode3D(QChartLayer3D::GridMode m)
{
    if (m_layer3D) {
        m_layer3D->setGridMode(m);   // Box 仅 Cartesian3D：非直角投影时图层 qWarning 并回退 FaceLine
        m_layer3D->invalidateData(); // 4g-fix（t58 F3）：模式切换 → 重收集（层 setter 已置脏，此处冗余保险）
    }
}

QChartLayer3D::GridMode QChartWidget3D::gridMode3D() const
{
    return m_layer3D ? m_layer3D->gridMode() : QChartLayer3D::GridMode::FaceLine;
}

// ===== fit / 坐标 =====

void QChartWidget3D::fitWorld()
{
    if (!m_layer3D) return;
    QChartCamera3D* cam = m_layer3D->camera3D();
    if (!cam) return;

    // 4d①：autoFit 门——关闭时不做自动重算（相机零触碰，保留用户手调参数）
    if (!cam->autoFit())
        return;

    // 4d②：域盒（4b：由三根轴范围组装；4c：全程用 QCube 访问器）→ 外扩 6% → 数据锚点 viewCube
    const QCube box = domainBox();
    const QCube fitBox = box.isValid()
        ? QCube(box.min, box.max) : QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10));
    const QVector3D pad = fitBox.size() * 0.06f;   // 外扩 6%（与迁移前 (max-min)*0.06 逐位一致）
    cam->setViewCube(QCube(fitBox.min - pad, fitBox.max + pad));

    // 4d②：镜头解算并入相机 fitCameraConfig（原手写 halfDiag/sin(fov/2) + setDistance + 近远面处理
    // 全部由相机承担）：
    //  · aspect ≥ 1 或 plotArea 退化（按 aspect=1）：d == r / max(0.05, sin(fov/2))，与 4c 内联式逐位一致；
    //  · aspect < 1（竖高视口）：自 4d 起改按**横向**临界半视角解算
    //    （d = r / max(0.05, sin(atan(tan(fov/2)·aspect)))）——4c 内联式忽略 aspect，此处为 4d 的
    //    有意改进（t46 裁定①，已作为“表现可观测变更”登记；竖高视口下相机自动后退以完整包容数据）。
    // 近远面由相机按内切值收尾。
    cam->fitCameraConfig(plotArea(), FitConstraint::FixedFov);

    // 姿态复位（widget 策略，非相机 fit 职责）
    if (qAbs(cam->yaw() - 45.0) > 0.5) cam->setYaw(45.0);
    if (qAbs(cam->pitch() - 30.0) > 0.5) cam->setPitch(30.0);
}

// ===== 4e：像素侧驱动（plotArea 变化 → 仅重解算镜头）=====

void QChartWidget3D::onPlotAreaChanged(const QRectF& newPlotArea)
{
    Q_UNUSED(newPlotArea);
    QChartCamera3D* cam = m_layer3D ? m_layer3D->camera3D() : nullptr;
    if (!cam) return;
    if (!cam->autoFit()) return;   // 4d 开关语义一致：关闭时零触碰（保留用户手调参数）
    // 仅重解算镜头：plotArea 宽比 → 窄边临界半视角 → distance/fov/near/far（不改锚点/姿态）
    ++m_plotAreaFitCount;   // 像素侧驱动次数（hook 实跑一次解算；宽视口下常为幂等无变化）
    if (cam->fitCameraConfig(plotArea(), FitConstraint::FixedFov))
        ++m_plotAreaFitChangeCount;   // 其中真正改变镜头的次数（竖高视口 → 相机后退）
}

// ===== 4f：鼠标交互接线（事件 → 既有相机 API；不动 4a–4e 机制）=====

// 滚轮 → dolly 因子：120/格 → 1.10 倍；上滚（正值）放大（视野盒收缩 → factor < 1）
qreal QChartWidget3D::wheelDollyFactor(int angleDeltaY)
{
    if (angleDeltaY == 0) return 1.0;
    return qPow(1.10, -qreal(angleDeltaY) / 120.0);
}

void QChartWidget3D::onMousePress(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)                       m_drag3D = Drag3D::Orbit;
    else if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton) m_drag3D = Drag3D::Pan;
    else return;
    m_lastPixel = e->position();
}

void QChartWidget3D::onMouseMove(QMouseEvent* e)
{
    QChartCamera3D* cam = camera3D();
    if (!cam || m_drag3D == Drag3D::None) return;
    const QPointF pos = e->position();
    const QPointF d = pos - m_lastPixel;
    m_lastPixel = pos;
    if (qFuzzyIsNull(d.x()) && qFuzzyIsNull(d.y())) return;

    if (m_drag3D == Drag3D::Orbit) {
        cam->orbit(orbitYawDelta(d.x()), orbitPitchDelta(d.y()));   // 相机内部钳制 pitch ±89°
        scheduleRepaint();
        return;
    }
    // Pan（中键/右键）：平移视野盒中心（panViewCube）——不改盒尺寸、不触发 fit
    const QRectF pa = plotArea();
    if (pa.width() <= 0.0 || pa.height() <= 0.0) return;
    const QVector3D size = cam->viewCubeSize();
    const qreal kx = size.x() / pa.width();
    const qreal ky = size.y() / pa.height();
    if (qFuzzyIsNull(kx) && qFuzzyIsNull(ky)) return;
    cam->panViewCube(-d.x() * kx, d.y() * ky);   // 拖动内容：中心反向平移（屏向下 → 世界 +Y）
    scheduleRepaint();
}

void QChartWidget3D::onMouseRelease(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton || e->button() == Qt::RightButton)
        m_drag3D = Drag3D::None;
}

void QChartWidget3D::onWheel(QWheelEvent* e)
{
    QChartCamera3D* cam = camera3D();
    if (!cam) return;
    const qreal f = wheelDollyFactor(e->angleDelta().y());
    if (qFuzzyCompare(f, 1.0)) return;
    const QCube before = cam->viewCube();
    cam->dolly(f);                                            // 改视野盒尺寸（既有入口）
    if (cam->viewCube().min == before.min && cam->viewCube().max == before.max) return;   // 无变化 → 无联动
    // autoFit 联动：开 → 既有 fit 路径重算 distance/near/far（不改锚点、不复位姿态）；关 → 保留手调参数
    if (cam->autoFit()) {
        ++m_interactionFitCount;                              // 诊断计数（每次 dolly 后一次解算）
        cam->fitCameraConfig(plotArea(), FitConstraint::FixedFov);
    }
    scheduleRepaint();
}

QPointF QChartWidget3D::worldToPixel(const QVector3D& w) const
{
    const QChartCamera3D* cam = m_layer3D ? m_layer3D->camera3D() : nullptr;
    if (!cam) return QPointF(qQNaN(), qQNaN());
    return cam->project(w, m_plotArea).screen;
}

QCube QChartWidget3D::viewCube() const
{
    const QChartCamera3D* cam = m_layer3D ? m_layer3D->camera3D() : nullptr;
    return cam ? cam->viewCube() : QCube();
}

void QChartWidget3D::setViewCube(const QCube& box)
{
    if (QChartCamera3D* cam = m_layer3D ? m_layer3D->camera3D() : nullptr) {
        cam->setViewCube(box);
        cam->resetNearFar();
    }
}

// ===== 3D 渲染管线覆写（批次 B2）=====

void QChartWidget3D::pushContextToLayers()
{
    if (!m_layer3D) return;
    const QChartProjection3D* proj3 = m_layer3D->projection3D();
    const QColor bg = sceneBackgroundColor();
    m_layer3D->setScene3DProjection(proj3);
    m_layer3D->setScene3DPlotArea(m_plotArea);
    m_layer3D->setScene3DBackground(bg);
    // 4g：视图状态指纹（相机视图投影 × plotArea × 投影 × 背景）→ 视图变化才置脏（重收集）
    const qreal aspect = (m_plotArea.height() > 0.0) ? m_plotArea.width() / m_plotArea.height() : 1.0;
    if (const QChartCamera3D* cam = m_layer3D->camera3D())
        m_layer3D->setSceneViewState(cam->viewProjectionMatrix(aspect), m_plotArea, proj3, bg);
    else
        m_layer3D->setSceneViewState(QMatrix4x4(), m_plotArea, proj3, bg);
    // 4g-fix（t58 F4）：内容贡献指纹（三轴范围/刻度/子刻度/可见性/颜色 + 网格模式/样式 + 数据版本）
    quint64 key = 0;
    for (const QChartAxis* a : { m_layer3D->axisX(), m_layer3D->axisY(), m_layer3D->axisZ() }) {
        if (!a) { key = QChartAbstractLayer::contentHash(key, 0); continue; }
        key = QChartAbstractLayer::contentHashReal(key, a->min());
        key = QChartAbstractLayer::contentHashReal(key, a->max());
        key = QChartAbstractLayer::contentHash(key, quint64(a->tickCount()));
        key = QChartAbstractLayer::contentHash(key, quint64(a->subTickCount()));
        key = QChartAbstractLayer::contentHash(key, a->isVisible() ? 1u : 2u);
        key = QChartAbstractLayer::contentHash(key, quint64(a->color().rgba()));
    }
    key = QChartAbstractLayer::contentHash(key, quint64(m_layer3D->gridMode()));
    key = QChartAbstractLayer::contentHash(key, m_layer3D->isGridVisible() ? 1u : 2u);
    key = QChartAbstractLayer::contentHash(key, quint64(m_layer3D->gridColor().rgba()));
    key = QChartAbstractLayer::contentHash(key, m_layer3D->contentRevision());
    m_layer3D->setSceneContentState(key);
}

void QChartWidget3D::onBeforePaint()
{
    // 3D 链不走 2D axis→camera 同步（基类同步为空操作覆盖）
}

void QChartWidget3D::drawExternalContent(QPainter&)
{
    // 3D：无 2D 边框轴/标题——plotArea 外保持背景（基类边框轴绘制不适用于 3D 层）
}

void QChartWidget3D::renderLayers(QPaintDevice* device)
{
    if (!m_cpuRenderer || !m_layer3D) return;
    pushContextToLayers();
    // 4g：脏才重收集（视图/数据变化 → 背景随可见范围更新）；无变化跳过 → 复用缓存（含已变换状态）
    if (m_layer3D->ensureSceneCollected()) {
        m_sceneCache = m_layer3D->scene3D();    // 刷新缓存（camera 指针指向 layer 成员，存活期渲染）
        m_cpuRenderer->invalidateView();         // 快照重建 → 重算变换与裁剪
    }
    m_cpuRenderer->render(m_sceneCache, device);
}

void QChartWidget3D::renderLayersGL(QPaintDevice* device)
{
    if (!m_glRenderer || !m_layer3D) return;
    pushContextToLayers();

    // 标签合成位图（设备分辨率；逻辑坐标光栅化由 Qt 按 dpr 映射——与 2D 容器同构）
    qreal labelDpr = 1.0;
    if (const QWidget* wgt = dynamic_cast<const QWidget*>(device))
        labelDpr = wgt->devicePixelRatioF();
    QImage labelDev(qMax(1, qCeil(m_plotArea.width() * labelDpr)),
                    qMax(1, qCeil(m_plotArea.height() * labelDpr)),
                    QImage::Format_ARGB32_Premultiplied);
    labelDev.setDevicePixelRatio(labelDpr);
    labelDev.fill(Qt::transparent);

    // 4g：脏才重收集（同 CPU 路径）——缓存保留已变换/已标 depth 的状态，无变化直接复用
    if (m_layer3D->ensureSceneCollected()) {
        m_sceneCache = m_layer3D->scene3D();
        // B1/B2 线框冒烟语义：depth>1 → decor 批次（depthTest 关=全边可见，与 CPU 全绘制一致）；
        // 3D series 深度排序语义后续批次接管。
        for (QChartPrimitive& p : m_sceneCache.primitives)
            p.depth = 2.0f;
        m_glRenderer->invalidateView();
    }
    m_glRenderer->render(m_sceneCache, &labelDev);

    QPainter overlay(device);
    overlay.setRenderHint(QPainter::Antialiasing, true);
    overlay.setCompositionMode(QPainter::CompositionMode_SourceOver);
    overlay.drawImage(QPoint(0, 0), labelDev);
}
