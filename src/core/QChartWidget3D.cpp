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

// ===== 构造 / 析构 =====

QChartWidget3D::QChartWidget3D(QWidget* parent)
    : QChartWidget(parent)
{
    // 默认三轴（2D QValueAxis 实例，3D 数值化共用）
    m_axisX3D = new QValueAxis(this, Qt::AlignBottom);
    m_axisY3D = new QValueAxis(this, Qt::AlignLeft);
    m_axisZ3D = new QValueAxis(this, Qt::AlignBottom);
    for (QValueAxis* a : {m_axisX3D, m_axisY3D, m_axisZ3D}) {
        a->setRange(0.0, 10.0);
        a->setTickCount(5);
        a->setColor(Qt::black);
    }
    // 单 layer3D 托管（QObject 子；基类 addLayer 接线——容器非持有列表）
    // ★ 轴**随构造传入**（构造期绑定、运行期不可更换）；轴信号→脏表的接线由 layer 自己在
    //   构造里完成，widget 侧不再重复接一遍（旧 wireAxisToLayerDirty 已删）。
    m_layer3D = new QChartLayer3D(m_axisX3D, m_axisY3D, m_axisZ3D, this);
    QChartWidget::addLayer(m_layer3D);
    // 相机视野盒变化 → 反算三轴范围（信号版；不再由 onWheel/onMouseMove 手动调用）
    connect(m_layer3D, &QChartLayer3D::viewChanged, this, &QChartWidget3D::updateAxes);
}

QChartWidget3D::~QChartWidget3D() = default;

// ===== 图层 =====

void QChartWidget3D::addLayer3D(QChartLayer3D* g)
{
    if (!g) return;
    QChartWidget::addLayer(g);   // 基类容器接线（布局/渲染遍历均经基类 m_layers）
}

// ===== 三轴访问（轴在 layer 构造时绑定，运行期不可更换：setAxis*3D 已删除）=====
QChartAxis* QChartWidget3D::axisX3D() const { return m_layer3D ? m_layer3D->axisX() : nullptr; }
QChartAxis* QChartWidget3D::axisY3D() const { return m_layer3D ? m_layer3D->axisY() : nullptr; }
QChartAxis* QChartWidget3D::axisZ3D() const { return m_layer3D ? m_layer3D->axisZ() : nullptr; }

// ===== 域盒 =====

void QChartWidget3D::setDataBounds3D(const QCube& box)
{
    if (m_layer3D)
        m_layer3D->setDataBounds(box);   // 4c：QCube 主签名（数值落三轴范围）
    fitWorld();
}

QCube QChartWidget3D::dataBounds3D() const
{
    // 4c：数据域访问器（QCube）——按需从 layer3D 的三轴组装，无长期持有
    return (m_layer3D && m_layer3D->axes3D()) ? m_layer3D->axes3D()->dataBounds() : QCube();
}

void QChartWidget3D::clearDataBounds3D()
{
    if (m_layer3D)
        m_layer3D->setDataBounds(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));   // 4c：QCube 默认盒
    fitWorld();
}

// ===== 投影 =====

void QChartWidget3D::setProjection3D(const QChartProjection3D* proj)
{
    // ★ "切换" = 原来已经有一个投影、现在换成另一个（构造期第一次设置不算）。
    const bool isSwitch = (m_projection3D != nullptr && m_projection3D != proj);

    m_projection3D = proj;                 // widget 持有（供 computeDataBounds/computeViewCube 使用）
    if (m_layer3D)
        m_layer3D->setProjection3D(proj);  // layer 同步一份（采样提示）；scene3D.projection 仍由 pushContext 注入

    if (isSwitch && m_layer3D) {
        // ★★ 切换投影：**保相机、不保数据**（与 2D 同构）。
        //   相机一个字节不动，用新投影从同一个视图反算 Numeric 写回三轴。
        //   这三步是不可分的：只保相机不做失效/反算，会得到"轴按新投影、系列仍按旧投影"的
        //   半新半旧状态（比全旧更糟）。
        QChartScene& sc = m_layer3D->scene();
        for (QChartSegment& s : sc.axisSegments) s.cartValid = false;
        for (QChartSegment& s : sc.seriesSegments) s.cartValid = false;

        const QCube cube = viewCube();     // 相机当前的视野盒（保持不动）
        if (cube.isValid() && !m_cameraTriggered)
            updateAxes(cube);              // 新投影: computeDataBounds(cube) → 写三轴
        scheduleRepaint();
        return;                            // 切换路径 **不** 走 fitWorld（那是"数据 → 相机"）
    }

    // 初始化（原来没有投影）：保持既有行为 —— 数据 → 相机
    fitWorld();
}

// ===== 网格模式（批次2 B：转发 layer3D）=====

void QChartWidget3D::setGridMode3D(QChartLayer3D::GridMode m)
{
    // 置脏唯一通路：setGridMode 内 emit gridChanged → invalidateAxis
    if (m_layer3D)
        m_layer3D->setGridMode(m);   // Box 仅 Cartesian3D：非直角投影时图层 qWarning 并回退 FaceLine
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

    // 4d②：数据域（三轴组装）→ World 盒（非恒等投影须采样；Cartesian3D 下采样结果等于原盒）
    //       → 外扩 6% → 数据锚点 viewCube
    const QCube box = dataBounds3D();
    const QCube dataBox = box.isValid() ? box : QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10));
    QCube world = dataBox;
    if (m_projection3D)
        world = m_projection3D->computeViewCube(dataBox.min, dataBox.max);
    if (!world.isValid())
        world = dataBox;
    const QVector3D pad = world.size() * 0.06f;   // 外扩 6%（对 World 盒）

    // 本次相机变化由数值侧驱动 → 抑制反算回轴（否则外扩会写进轴范围）
    m_cameraTriggered = true;
    m_layer3D->setViewCube(QCube(world.min - pad, world.max + pad));

    // 4d②：镜头解算并入相机 fitCameraConfig（近远面由相机按内切值收尾）
    cam->fitCameraConfig(plotArea(), FitConstraint::FixedFov);

    // 姿态复位（widget 策略，非相机 fit 职责）
    if (qAbs(cam->yaw() - 45.0) > 0.5) cam->setYaw(45.0);
    if (qAbs(cam->pitch() - 30.0) > 0.5) cam->setPitch(30.0);
    m_cameraTriggered = false;
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
    scheduleRepaint();   // 相机 viewChanged 已触发层转发 → widget updateAxes 反算三轴
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
    scheduleRepaint();   // 相机 viewChanged（dolly/fit）已触发层转发 → widget updateAxes 反算三轴
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

// 相机视野盒（World 空间盒）→ 三轴范围（Numeric 空间）反算——镜像 2D QChartWidget::updateAxes。
// 由 layer3D 的 viewChanged 触发（dolly/pan/fit 后）；fitWorld 期间由 m_cameraTriggered 抑制。
void QChartWidget3D::updateAxes(const QCube& viewCube)
{
    if (m_cameraTriggered) return;
    if (!m_projection3D || !viewCube.isValid()) return;
    const QCube dataBox = m_projection3D->computeDataBounds(viewCube);   // 恒等→原盒；否则 5³ 采样
    if (!dataBox.isValid()) return;
    if (m_layer3D) {
        if (auto* a = m_layer3D->axisX()) a->setRange(dataBox.min.x(), dataBox.max.x());
        if (auto* a = m_layer3D->axisY()) a->setRange(dataBox.min.y(), dataBox.max.y());
        if (auto* a = m_layer3D->axisZ()) a->setRange(dataBox.min.z(), dataBox.max.z());
    }
    scheduleRepaint();
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
    // 视图/内容指纹已全部删除：cull 每帧全量跑、cart 由段的 cartValid 判，
    // 收集只由脏表驱动（轴走 invalidateAxis、系列走 invalidateSeries）。
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
    // 直接渲染 layer 的场景（不再做全量深拷贝）：cartValid / visible 等运行时状态必须
    // 留在段上，否则每次拷贝都会把"已算好"的状态丢掉 → 部分变换失效。
    m_layer3D->ensureSceneCollected();
    m_cpuRenderer->render(m_layer3D->scene(), device);
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

    // 直接渲染 layer 的场景（同 CPU 路径：运行时状态留在段上）
    m_layer3D->ensureSceneCollected();
    m_glRenderer->render(m_layer3D->scene(), &labelDev);

    QPainter overlay(device);
    overlay.setRenderHint(QPainter::Antialiasing, true);
    overlay.setCompositionMode(QPainter::CompositionMode_SourceOver);
    overlay.drawImage(QPoint(0, 0), labelDev);
}
