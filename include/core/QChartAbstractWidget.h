// QChartAbstractWidget.h —— 图表抽象基类
#ifndef QCHARTABSTRACTWIDGET_H
#define QCHARTABSTRACTWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QRectF>
#include <QColor>
#include <memory>
#include <QList>

#include "QCube.h"
#include "QChartScene.h"
#include "QChartRenderer.h"
#include "QPainterChartRenderer.h"
#include "QOpenGLChartRenderer.h"
#include "QChartCamera.h"
#include "QChartLegend.h"
#include "QChartSeries.h"

class QChartAbstractWidget : public QWidget
{
    Q_OBJECT

public:
    enum class RenderBackend {
        QPainter,
        OpenGL
    };

    explicit QChartAbstractWidget(QWidget* parent = nullptr);
    ~QChartAbstractWidget() override;

    // ---- 渲染后端 ----
    void setRenderBackend(RenderBackend backend);
    RenderBackend renderBackend() const { return m_renderBackend; }

    // ---- 图例 ----
    QChartLegend* legend() const { return m_legend; }

    // ---- 数据范围 ----
    QRectF plotArea() const { return m_plotArea; }
    QChartAbstractProjection* projection() const { return m_projection.get(); }

    // ---- 导出（强制使用 QPainter） ----
    bool saveAsPng(const QString& path, const QSize& size = {}, qreal devicePixelRatio = 1.0);
    bool saveAsSvg(const QString& path, const QSize& size = {});
    bool saveAsPdf(const QString& path, const QSize& size = {});
    void setExportTransparentBackground(bool v) { m_exportTransparentBackground = v; }
    bool exportTransparentBackground() const { return m_exportTransparentBackground; }

    // ---- 缓存控制 ----
    void invalidateBackground();
    void invalidateForeground();
    void invalidateLayout();

signals:
    void plotAreaChanged(const QRectF& newPlotArea);
    void projectionChanged(const QChartAbstractProjection* newProjection);

protected:
    
    // 子类必须实现
    
    /// 计算 plotArea（基于当前尺寸和边距，考虑边框轴占用）
    virtual QRectF calculatePlotArea() const = 0;

    /// 从当前相机状态反算 dataBounds（QCube）

    /// dataBounds 变化时，反算相机状态
    /// 2D: dataBounds → viewRect（调用 projection->computeViewRect）
    /// 3D: dataBounds → viewCube（调用相机 setViewCubeToFit）

    /// 组装场景快照（由 paintEvent / paintGL 调用）
    /// 子类填充 primitives、labels、camera、projection、plotArea、legend 等
    
    // 子类可重写

    /// 绘制 plotArea 外部的内容（边框轴、标题等）
    /// GPU 模式下，plotArea 内部由 GlHost 覆盖，但外部仍由外层绘制
    virtual void drawExternalContent(QPainter& painter) { Q_UNUSED(painter); }

    /// 交互行为（子类实现具体 pan/zoom/orbit 逻辑）
    virtual void onMousePress(QMouseEvent* e) { Q_UNUSED(e); }
    virtual void onMouseMove(QMouseEvent* e) { Q_UNUSED(e); }
    virtual void onMouseRelease(QMouseEvent* e) { Q_UNUSED(e); }
    virtual void onWheel(QWheelEvent* e) { Q_UNUSED(e); }

    /// 在 GlHost 的 QPainter 覆盖层中绘制额外内容（数据标签等）
    virtual void drawOverlay(QPainter& painter, const QChartScene& scene) { Q_UNUSED(painter); Q_UNUSED(scene); }

    /// 布局入口（子类可重写，但应调用基类）
    virtual void layoutAxes();
    
    // 子类可访问的成员
    
    QChartAbstractCamera* camera() const { return m_camera.get(); }
    QChartRenderer* renderer() const { return m_renderer.get(); }
    QOpenGLChartRenderer* glRenderer() const { return m_glRenderer; }

    QRectF m_plotArea;

    QList<QChartSeries*> m_legendItems;   // 子类在 buildScene 中填充
    QChartLegend* m_legend = nullptr;

    qreal m_marginLeft   = 20.0;
    qreal m_marginTop    = 20.0;
    qreal m_marginRight  = 20.0;
    qreal m_marginBottom = 20.0;

    std::unique_ptr<QChartAbstractProjection> m_projection;

private:
    
    // 事件分发（final，子类不能重写）
    
    void paintEvent(QPaintEvent*) override final;
    void resizeEvent(QResizeEvent*) override final;
    void mousePressEvent(QMouseEvent*) override final;
    void mouseMoveEvent(QMouseEvent*) override final;
    void mouseReleaseEvent(QMouseEvent*) override final;
    void wheelEvent(QWheelEvent*) override final;
    
    // 内部辅助

    void layoutGlHost();
    void scheduleRepaint();
    bool handleLegendClick(const QPointF& pos);

    // GlHost（内嵌 QOpenGLWidget，按需创建）
    
    class GlHost;
    std::unique_ptr<GlHost> m_glHost;

    // 渲染器

    // 当前活跃的渲染器（CPU 或 GPU）
    std::unique_ptr<QChartRenderer> m_renderer;

    // ★ GL 特有指针：供 GlHost 调用 render()（无生命周期管理函数）
    QOpenGLChartRenderer* m_glRenderer = nullptr;

    RenderBackend m_renderBackend = RenderBackend::QPainter;
    bool m_layoutDirty = true;
    bool m_exportTransparentBackground = false;
};

#endif // QCHARTABSTRACTWIDGET_H