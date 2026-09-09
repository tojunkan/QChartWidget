// QChartAbstractWidget.cpp —— 图表抽象基类实现（S0+批次 A：纯容器）
#include "QChartAbstractWidget.h"
#include "QChartLayer.h"
#include "QPainterChartRenderer.h"
#include "QOpenGLChartRenderer.h"
#include "QChartGL.h"
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QPainter>
#include <QImage>
#include <QtMath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logAbstractWidget, "chart.abstractwidget")

// t14(HiDPI)：GL 渲染器 DPR 注入通道（定义于 QOpenGLChartRenderer.cpp；头文件不动——
// 正式 setPixelRatio API 随渲染器配置阶段落头）
extern void qchartSetGLPixelRatio(qreal);

// ===== GlPlotWidget：plotArea 对齐的 QOpenGLWidget 子控件（替换旧全窗 GlHost）=====
// GL 只画 plotArea 内；plotArea 外（边框轴 drawAtEdge/标题）由外层 widget QPainter 画，互不遮挡。
// 旧 GlHost（全窗覆盖 + legend overlay）已整体后置，随图例/Phase-1 恢复。
// （类名保持全局可见：QChartAbstractWidget 以 friend 授予渲染编排访问）
class GlPlotWidget : public QOpenGLWidget
{
public:
    explicit GlPlotWidget(QChartAbstractWidget* owner)
        : QOpenGLWidget(owner)
        , m_owner(owner)
    {
        setFormat(QChartGL::surfaceFormat());
        setAttribute(Qt::WA_TransparentForMouseEvents);   // 交互事件仍由外层 widget 统一分发
        hide();                                            // setRenderBackend(OpenGL) 时显示
    }

protected:
    void initializeGL() override { m_ready = true; }

    void paintGL() override
    {
        if (!m_ready) return;

        // ★ t13(HiDPI 修复)：视口必须用设备像素尺寸（FBO/默认帧缓冲按 devicePixelRatio
        // 分配：Windows DPR=1.5 时 FBO=498x416 而逻辑尺寸 332x277）。用逻辑尺寸会令
        // GL 内容只占 FBO 左下、中心采样落空。逻辑中心 → 设备中心由视口变换保证
        // （u_viewProj 的 aspect 与逻辑/设备一致——等比缩放不改变中心/几何比例）。
        auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(context());
        if (!f) return;
        const int vpW = qRound(width() * devicePixelRatioF());
        const int vpH = qRound(height() * devicePixelRatioF());
        f->glViewport(0, 0, vpW, vpH);
        f->glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // t14(HiDPI)：每帧向 GL 渲染器注入宿主 DPR（点尺寸/线宽按设备像素放大，
        // 保持与 CPU QPainter 逻辑像素语义对等）；DPR=1 时注入 1 → 零变化。
        qchartSetGLPixelRatio(devicePixelRatioF());

        // plotArea 内内容：各 layer 快照 → GL 渲染（标签 QPainter 覆盖层由 renderer 完成）
        m_owner->renderLayersGL(this);
    }

    void resizeGL(int w, int h) override {
        Q_UNUSED(w); Q_UNUSED(h);   // 视口每次 paintGL 设置
    }

private:
    QChartAbstractWidget* m_owner;
    bool m_ready = false;
};

// ===== 构造 / 析构 =====

QChartAbstractWidget::QChartAbstractWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(200, 150);
    m_cpuRenderer = std::make_unique<QPainterChartRenderer>();
}

QChartAbstractWidget::~QChartAbstractWidget()
{
    // 释放 GL 批次资源（上下文仍存活时），随后成员按声明逆序析构（glHost → glRenderer）
    if (m_glHost && m_glRenderer) {
        m_glHost->makeCurrent();
        m_glRenderer->clearBatches();
    }
}

// ===== 渲染后端 =====

void QChartAbstractWidget::setRenderBackend(RenderBackend backend)
{
    if (m_renderBackend == backend) return;

    if (backend == RenderBackend::OpenGL) {
        // ---- 切 GPU：创建 plotArea 对齐 GL 子控件 + GL 渲染器 ----
        if (!m_glRenderer)
            m_glRenderer = std::make_unique<QOpenGLChartRenderer>();
        if (!m_glHost) {
            m_glHost = std::make_unique<GlPlotWidget>(this);
            m_glHostWidgetRaw = m_glHost.get();
        }
        m_renderBackend = RenderBackend::OpenGL;
        m_layoutDirty = true;          // 需要重算 plotArea 并摆放子控件
        m_glHost->setGeometry(m_plotArea.toRect());
        m_glHost->show();
    } else {
        // ---- 切 CPU：销毁 GL 宿主（不创建 GL 子控件）----
        if (m_glHost && m_glRenderer) {
            m_glHost->makeCurrent();
            m_glRenderer->clearBatches();
        }
        m_glHost.reset();
        m_glHostWidgetRaw = nullptr;
        m_glRenderer.reset();
        m_renderBackend = RenderBackend::QPainter;
    }
    scheduleRepaint();
}

// ===== 图层管理 =====

void QChartAbstractWidget::addLayer(QChartLayer* layer)
{
    if (!layer || m_layers.contains(layer)) return;
    m_layers.append(layer);
    m_layoutDirty = true;
    scheduleRepaint();
}

void QChartAbstractWidget::removeLayer(QChartLayer* layer)
{
    if (m_layers.removeAll(layer) > 0) {
        m_layoutDirty = true;
        scheduleRepaint();
    }
}

void QChartAbstractWidget::clearLayers()
{
    if (!m_layers.isEmpty()) {
        m_layers.clear();
        m_layoutDirty = true;
        scheduleRepaint();
    }
}

// ===== 布局 =====

void QChartAbstractWidget::setMargins(qreal left, qreal top, qreal right, qreal bottom)
{
    m_marginLeft = left; m_marginTop = top; m_marginRight = right; m_marginBottom = bottom;
    m_layoutDirty = true;
    scheduleRepaint();
}

void QChartAbstractWidget::relayout()
{
    const QRectF newPlotArea = calculatePlotArea();
    m_layoutDirty = false;
    if (newPlotArea != m_plotArea) {
        m_plotArea = newPlotArea;
        emit plotAreaChanged(m_plotArea);
    }
    layoutGlHost();
}

void QChartAbstractWidget::layoutGlHost()
{
    if (m_glHost && m_glHost->isVisible()) {
        m_glHost->setGeometry(m_plotArea.toRect());
    }
}

// ===== 上下文推送 / 渲染编排 =====

void QChartAbstractWidget::pushContextToLayers()
{
    const QChartAbstractProjection* proj = projection();
    const QColor bg = sceneBackgroundColor();
    for (QChartLayer* layer : m_layers) {
        if (!layer) continue;
        layer->setSceneProjection(proj);
        layer->setScenePlotArea(m_plotArea);
        layer->setSceneBackground(bg);
    }
}

void QChartAbstractWidget::renderLayers(QPaintDevice* device)
{
    if (!m_cpuRenderer) return;
    pushContextToLayers();   // plotArea/投影/背景 → 各层场景（渲染前恒同步，幂等）
    for (QChartLayer* layer : m_layers) {
        if (!layer) continue;
        layer->collectPrimitives();
        m_cpuRenderer->invalidateView();   // 每层场景各自重算变换（相机/内容不同）
        m_cpuRenderer->render(layer->scene(), device);
    }
}

void QChartAbstractWidget::renderLayersGL(QPaintDevice* device)
{
    if (!m_glRenderer) return;
    pushContextToLayers();

    // ★ F1(t8)：GL 标签覆盖层经透明位图合成到宿主（child-local）。
    // 背景：标签统一经透明 QImage 中间层（renderer 内 translate(-plotArea.topLeft)
    // 使锚点落在局部坐标）再 SourceOver 合成。
    // t14(HiDPI)：QImage 改设备分辨率（plotArea 逻辑 × 宿主 dpr）+ setDevicePixelRatio(dpr)——
    // QPainter 仍画逻辑坐标由 Qt 自动映射到设备像素，文字不再被二次放大（清晰度）；
    // 合成 drawImage 以逻辑尺寸上屏，Qt painter 按 dpr 映射，无二次缩放。DPR=1 时与现状逐位一致。
    qreal labelDpr = 1.0;
    if (const QWidget* wgt = dynamic_cast<const QWidget*>(device))
        labelDpr = wgt->devicePixelRatioF();
    const QSize devSize(qMax(1, qCeil(m_plotArea.width() * labelDpr)),
                        qMax(1, qCeil(m_plotArea.height() * labelDpr)));
    QImage labelDev(devSize, QImage::Format_ARGB32_Premultiplied);
    labelDev.setDevicePixelRatio(labelDpr);
    labelDev.fill(Qt::transparent);

    for (QChartLayer* layer : m_layers) {
        if (!layer) continue;
        layer->collectPrimitives();
        m_glRenderer->invalidateView();
        m_glRenderer->render(layer->scene(), &labelDev);   // 图元→当前 GL FBO；标签→labelDev
    }

    QPainter overlay(device);
    overlay.setRenderHint(QPainter::Antialiasing, true);
    overlay.setCompositionMode(QPainter::CompositionMode_SourceOver);
    overlay.drawImage(QPoint(0, 0), labelDev);
}

// ===== 事件分发（final）=====

void QChartAbstractWidget::paintEvent(QPaintEvent*)
{
    if (m_layoutDirty) {
        relayout();
    }
    onBeforePaint();

    // ---- CPU 后端：不建 GL 子控件，paintEvent 全量 QPainter 出图 ----
    if (m_renderBackend == RenderBackend::QPainter) {
        {
            QPainter bg(this);
            bg.fillRect(rect(), sceneBackgroundColor());
        }
        pushContextToLayers();
        renderLayers(this);
        QPainter ext(this);
        drawExternalContent(ext);
        return;
    }

    // ---- GPU 后端：外层只画 plotArea 外内容；plotArea 内由 GlPlotWidget::paintGL ----
    if (m_glHost && m_glHost->isVisible() && m_glRenderer) {
        {
            QPainter bg(this);
            bg.fillRect(rect(), sceneBackgroundColor());
        }
        pushContextToLayers();
        QPainter ext(this);
        drawExternalContent(ext);
        return;
    }

    // GPU 激活但宿主尚未就绪：退化为 CPU 全量绘制（不建 GL 子控件路径之外的安全兜底）
    {
        QPainter bg(this);
        bg.fillRect(rect(), sceneBackgroundColor());
    }
    pushContextToLayers();
    renderLayers(this);
}

void QChartAbstractWidget::resizeEvent(QResizeEvent*)
{
    m_layoutDirty = true;
    scheduleRepaint();
}

void QChartAbstractWidget::mousePressEvent(QMouseEvent* e)    { onMousePress(e); }
void QChartAbstractWidget::mouseMoveEvent(QMouseEvent* e)     { onMouseMove(e); }
void QChartAbstractWidget::mouseReleaseEvent(QMouseEvent* e)  { onMouseRelease(e); }
void QChartAbstractWidget::wheelEvent(QWheelEvent* e)         { onWheel(e); }

// ===== 外部内容 / 重绘调度 =====

void QChartAbstractWidget::drawExternalContent(QPainter&)
{
    // 基类为空；边框轴 drawAtEdge/标题由子类（QChartWidget）实现
}

void QChartAbstractWidget::scheduleRepaint()
{
    if (m_renderBackend == RenderBackend::OpenGL && m_glHost && m_glHost->isVisible()) {
        m_glHost->update();
    } else {
        update();
    }
}
