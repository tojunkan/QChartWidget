// QChartAbstractWidget.cpp —— 图表抽象基类实现
#include "QChartAbstractWidget.h"
#include "QChartLegend.h"
#include "QChartGL.h"
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QPainter>
#include <QImage>
#include <QSvgGenerator>
#include <QPdfWriter>
#include <QPageSize>
#include <QFileInfo>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logAbstractWidget, "chart.abstractwidget")


// GlHost：内嵌 QOpenGLWidget（按需创建，懒加载）


class QChartAbstractWidget::GlHost : public QOpenGLWidget
{
public:
    explicit GlHost(QChartAbstractWidget* outer)
        : QOpenGLWidget(outer)
        , m_outer(outer)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFormat(QChartGL::surfaceFormat());
        QChartGL::registerHost();

        // Qt 6.5+ 支持 setShareContext，低版本忽略
        if (QOpenGLContext* sc = QChartGL::sharedContext()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            setShareContext(sc);
#else
            Q_UNUSED(sc);
#endif
        }

        // 初始隐藏，由 setRenderBackend(OpenGL) 控制显示
        hide();
    }

    ~GlHost() override {
        // ★ 先释放 GPU 资源（在上下文销毁前）
        if (m_outer->m_glRenderer) {
            m_outer->m_glRenderer->clearBatches();
        }
        QChartGL::unregisterHost();
    }

    bool isReady() const { return m_ready; }

protected:
    void initializeGL() override {
        // GPU 渲染器初始化由外部（setRenderBackend）创建
        // 这里只需要检查渲染器就绪状态
        if (!m_outer->m_glRenderer) {
            // 如果 GlHost 被提前显示但渲染器未创建，隐藏并等待
            hide();
            m_ready = false;
            return;
        }

        // ★ 渲染器的初始化（Shader 编译）由渲染器自身完成
        // QOpenGLChartRenderer 没有 initializeGL，它只在需要时惰性编译 Shader
        m_ready = true;
    }

    void paintGL() override {
        if (!m_ready || !m_outer->m_glRenderer) {
            return;
        }

        // 1. 组装场景
        const QChartScene scene = m_outer->buildScene();

        // 2. GL 渲染
        m_outer->m_glRenderer->render(scene, this);

        // 3. QPainter 覆盖层（图例、数据标签等）
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.translate(-scene.plotArea.topLeft());

        // 图例
        if (scene.legend && scene.legend->isVisible()) {
            scene.legend->draw(&painter, scene.plotArea, scene.legendItems);
        }

        // 子类额外覆盖层
        m_outer->drawOverlay(painter, scene);
    }

    void resizeGL(int w, int h) override {
        // GL 视口由渲染器在 render() 中根据 scene.plotArea 设置
        // 这里不需要额外操作
        Q_UNUSED(w);
        Q_UNUSED(h);
    }

private:
    QChartAbstractWidget* m_outer;
    bool m_ready = false;
};


// 构造 / 析构


QChartAbstractWidget::QChartAbstractWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(200, 150);

    // 默认使用 CPU 渲染器
    m_renderer = std::make_unique<QPainterChartRenderer>();

    m_legend = new QChartLegend(this);
    connect(m_legend, &QChartLegend::visibleChanged, this, [this]() { invalidateForeground(); });
    connect(m_legend, &QChartLegend::alignmentChanged, this, [this]() { invalidateForeground(); });
}

QChartAbstractWidget::~QChartAbstractWidget()
{
    // ★ 析构顺序：
    // 1. m_glHost 先析构 → 在 GlHost 析构中调用 clearBatches() 释放 GPU 资源
    // 2. m_renderer 后析构 → 销毁 CPU/GPU 渲染器
    // 注意：unique_ptr 按声明逆序析构，m_glHost 在 m_renderer 之前声明，所以先析构
    // 这确保了 GL 资源在上下文销毁前释放
    m_glRenderer = nullptr;  // 防止悬空
}


// 渲染后端切换


void QChartAbstractWidget::setRenderBackend(RenderBackend backend)
{
    if (m_renderBackend == backend) {
        return;
    }

    if (backend == RenderBackend::OpenGL) {
        // ---- 切换到 GPU ----
        if (!m_glHost) {
            // 按需创建 GlHost
            m_glHost = std::make_unique<GlHost>(this);
            auto* gl = new QOpenGLChartRenderer();
            m_glRenderer = gl;
            m_renderer.reset(gl);
        }

        // GlHost 的 initializeGL 会在 show() 后自动调用
        m_glHost->show();
        m_renderBackend = RenderBackend::OpenGL;
    } else {
        // ---- 切换到 CPU ----
        if (m_glHost) {
            m_glHost->hide();
        }
        m_renderer = std::make_unique<QPainterChartRenderer>();
        m_glRenderer = nullptr;
        m_renderBackend = RenderBackend::QPainter;
    }

    scheduleRepaint();
}


// 布局


void QChartAbstractWidget::layoutAxes()
{
    m_plotArea = calculatePlotArea();

    if (m_camera) {
        m_camera->fitToPlotArea(m_plotArea);
        recomputeDataBounds();
    }

    layoutGlHost();
    m_layoutDirty = false;
}

void QChartAbstractWidget::layoutGlHost()
{
    if (m_glHost && m_glHost->isVisible()) {
        m_glHost->setGeometry(m_plotArea.toRect());
    }
}


// dataBounds


void QChartAbstractWidget::setDataBounds(const QCube& bounds)
{
    m_dataBounds = bounds;
    onDataBoundsChanged(bounds);
    scheduleRepaint();
}


// 缓存控制


void QChartAbstractWidget::invalidateBackground()
{
    if (m_renderer) {
        m_renderer->invalidateBackground();
    }
    scheduleRepaint();
}

void QChartAbstractWidget::invalidateForeground()
{
    if (m_renderer) {
        m_renderer->invalidateForeground();
    }
    scheduleRepaint();
}

void QChartAbstractWidget::invalidateLayout()
{
    m_layoutDirty = true;
    scheduleRepaint();
}


// 绘制触发


void QChartAbstractWidget::scheduleRepaint()
{
    if (m_renderBackend == RenderBackend::OpenGL && m_glHost && m_glHost->isReady()) {
        m_glHost->update();
    } else {
        update();
    }
}


// 事件分发（final）


void QChartAbstractWidget::paintEvent(QPaintEvent*)
{
    if (m_layoutDirty) {
        layoutAxes();
        if (m_renderer) {
            m_renderer->invalidateBackground();
            m_renderer->invalidateForeground();
        }
    }

    // ---- GPU 模式 ----
    if (m_renderBackend == RenderBackend::OpenGL && m_glHost && m_glHost->isReady()) {
        // plotArea 内部由 GlHost 覆盖，外层只画外部内容
        QPainter painter(this);
        drawExternalContent(painter);
        return;
    }

    // ---- CPU 模式 ----
    QPainter painter(this);
    const QChartScene scene = buildScene();
    if (m_renderer) {
        m_renderer->render(scene, this);
    }
}

void QChartAbstractWidget::resizeEvent(QResizeEvent*)
{
    m_layoutDirty = true;
    scheduleRepaint();
}

void QChartAbstractWidget::mousePressEvent(QMouseEvent* e)
{
    // 图例点击优先
    if (handleLegendClick(e->pos())) {
        return;
    }

    onMousePress(e);
    recomputeDataBounds();
    scheduleRepaint();
}

void QChartAbstractWidget::mouseMoveEvent(QMouseEvent* e)
{
    onMouseMove(e);
    scheduleRepaint();
}

void QChartAbstractWidget::mouseReleaseEvent(QMouseEvent* e)
{
    onMouseRelease(e);
    recomputeDataBounds();
    scheduleRepaint();
}

void QChartAbstractWidget::wheelEvent(QWheelEvent* e)
{
    onWheel(e);
    recomputeDataBounds();
    scheduleRepaint();
}


// 图例点击


bool QChartAbstractWidget::handleLegendClick(const QPointF& pos)
{
    if (!m_legend || !m_legend->isVisible()) {
        return false;
    }

    // m_legendItems 由子类在 buildScene 中填充
    QChartSeries* s = m_legend->seriesAt(pos, m_plotArea, m_legendItems);
    if (s) {
        s->setVisible(!s->isVisible());
        scheduleRepaint();
        return true;
    }
    return false;
}


// 导出（强制使用 CPU 渲染器）


bool QChartAbstractWidget::saveAsPng(const QString& path, const QSize& size, qreal devicePixelRatio)
{
    if (path.isEmpty() || devicePixelRatio <= 0.0) {
        return false;
    }

    const QChartScene scene = buildScene();
    QSizeF deviceSize = size.isEmpty() ? m_plotArea.size() : QSizeF(size);

    QSize pixelSize(qCeil(deviceSize.width() * devicePixelRatio),
                    qCeil(deviceSize.height() * devicePixelRatio));
    QImage img(pixelSize, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(devicePixelRatio);
    img.fill(m_exportTransparentBackground ? Qt::transparent : scene.backgroundColor);

    QPainterChartRenderer cpuRenderer;
    cpuRenderer.render(scene, &img);
    return img.save(path, "PNG");
}

bool QChartAbstractWidget::saveAsSvg(const QString& path, const QSize& size)
{
    if (path.isEmpty()) {
        return false;
    }
    if (!QFileInfo(path).absoluteDir().exists()) {
        return false;
    }

    const QChartScene scene = buildScene();
    QSizeF deviceSize = size.isEmpty() ? QSizeF(this->size()) : QSizeF(size);

    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(deviceSize.toSize());
    gen.setViewBox(QRectF(QPointF(0, 0), deviceSize));

    QPainterChartRenderer cpuRenderer;
    cpuRenderer.render(scene, &gen);
    return QFileInfo::exists(path);
}

bool QChartAbstractWidget::saveAsPdf(const QString& path, const QSize& size)
{
    if (path.isEmpty()) {
        return false;
    }
    if (!QFileInfo(path).absoluteDir().exists()) {
        return false;
    }

    const QChartScene scene = buildScene();
    QSizeF deviceSize = size.isEmpty() ? QSizeF(this->size()) : QSizeF(size);

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(deviceSize, QPageSize::Point));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setResolution(72);

    QPainterChartRenderer cpuRenderer;
    cpuRenderer.render(scene, &writer);
    return QFileInfo::exists(path);
}