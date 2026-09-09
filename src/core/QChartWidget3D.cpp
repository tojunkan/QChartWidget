// QChartWidget3D.cpp —— 3D 图表控件实现（批次 B2：轻量 3D 容器）
#include "QChartWidget3D.h"
#include "QChartCamera3D.h"
#include "QChartProjection3D.h"
#include "QPainterChartRenderer.h"
#include "QOpenGLChartRenderer.h"
#include <QImage>
#include <QPainter>
#include <QtMath>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logWidget3D, "chart.widget3d")

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
}

QChartWidget3D::~QChartWidget3D() = default;

// ===== 图层 =====

void QChartWidget3D::addLayer3D(QChartLayer3D* g)
{
    if (!g) return;
    QChartWidget::addLayer(g);   // 基类容器接线（布局/渲染遍历均经基类 m_layers）
}

// ===== 三轴绑定（透传 layer3D；默认轴替换由调用方管理生命周期——非持有约定同基类 addAxis）=====

void QChartWidget3D::setAxisX3D(QChartAxis* a) { if (m_layer3D) m_layer3D->setAxisX(a); }
void QChartWidget3D::setAxisY3D(QChartAxis* a) { if (m_layer3D) m_layer3D->setAxisY(a); }
void QChartWidget3D::setAxisZ3D(QChartAxis* a) { if (m_layer3D) m_layer3D->setAxisZ(a); }
QChartAxis* QChartWidget3D::axisX3D() const { return m_layer3D ? m_layer3D->axisX() : nullptr; }
QChartAxis* QChartWidget3D::axisY3D() const { return m_layer3D ? m_layer3D->axisY() : nullptr; }
QChartAxis* QChartWidget3D::axisZ3D() const { return m_layer3D ? m_layer3D->axisZ() : nullptr; }

// ===== 域盒 =====

void QChartWidget3D::setDomainBox(const QVector3D& dataMin, const QVector3D& dataMax)
{
    m_domainMin = dataMin;
    m_domainMax = dataMax;
    if (m_layer3D)
        m_layer3D->setDataBounds(dataMin, dataMax);
    fitWorld();
}

void QChartWidget3D::clearDomainBox()
{
    m_domainMin.reset();
    m_domainMax.reset();
    if (m_layer3D)
        m_layer3D->setDataBounds(QVector3D(0, 0, 0), QVector3D(10, 10, 10));
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

// ===== fit / 坐标 =====

void QChartWidget3D::fitWorld()
{
    if (!m_layer3D) return;
    QChartCamera3D* cam = m_layer3D->camera3D();
    if (!cam) return;

    // 域盒（无则 axes3D 数据盒，再退默认 0..10）
    QVector3D mn = m_domainMin.value_or(QVector3D(0, 0, 0));
    QVector3D mx = m_domainMax.value_or(QVector3D(10, 10, 10));
    if (m_layer3D->axes3D() && m_layer3D->axes3D()->dataBounds.isValid()
        && !m_domainMin.has_value()) {
        mn = m_layer3D->axes3D()->dataBounds.min;
        mx = m_layer3D->axes3D()->dataBounds.max;
    }
    const QVector3D pad = (mx - mn) * 0.06f;
    mn -= pad;
    mx += pad;

    cam->setViewCube(QCube(mn, mx));
    // 重算 distance/near/far（fov 由相机保持；半对角线 → 距离）
    const qreal halfDiag = (mx - mn).length() * 0.5;
    cam->setDistance(halfDiag / qMax(qreal(0.05), qSin(qDegreesToRadians(cam->fov()) * 0.5)));
    cam->resetNearFar();
    if (qAbs(cam->yaw() - 45.0) > 0.5) cam->setYaw(45.0);
    if (qAbs(cam->pitch() - 30.0) > 0.5) cam->setPitch(30.0);
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
    m_layer3D->setScene3DProjection(m_layer3D->projection3D());
    m_layer3D->setScene3DPlotArea(m_plotArea);
    m_layer3D->setScene3DBackground(sceneBackgroundColor());
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
    m_layer3D->collectPrimitives();
    QChartScene scene = m_layer3D->scene3D();   // 拷贝（camera 指针指向 layer 成员，存活期渲染）
    m_cpuRenderer->invalidateView();
    m_cpuRenderer->render(scene, device);
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

    m_layer3D->collectPrimitives();
    QChartScene scene = m_layer3D->scene3D();
    // B1/B2 线框冒烟语义：depth>1 → decor 批次（depthTest 关=全边可见，与 CPU 全绘制一致）；
    // 3D series 深度排序语义后续批次接管。
    for (QChartPrimitive& p : scene.primitives)
        p.depth = 2.0f;

    m_glRenderer->invalidateView();
    m_glRenderer->render(scene, &labelDev);

    QPainter overlay(device);
    overlay.setRenderHint(QPainter::Antialiasing, true);
    overlay.setCompositionMode(QPainter::CompositionMode_SourceOver);
    overlay.drawImage(QPoint(0, 0), labelDev);
}
