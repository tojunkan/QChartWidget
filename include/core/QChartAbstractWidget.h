// QChartAbstractWidget.h —— 图表抽象基类（S0+批次 A：容器化首改）
// 目标架构决策（用户已确认，逐条落实）：
//   (1) widget 层 = 纯容器：不含相机、不含 buildScene、不含图例/导出/拾取/动画；
//       invalidateBackground/invalidateForeground 语义已由 dataDirty/viewDirty 取代。
//   (2) 容器管理：layers 列表、唯一 plotArea（布局计算）、plotAreaChanged/projectionChanged 广播。
//   (3) 相机归 layer（2D QChartLayer 自带 QChartCamera 值成员）。
//   (4) GL 宿主为 plotArea 对齐的 QOpenGLWidget 子控件；CPU 后端不建 GL 子控件。
//   (5) 事件分发钩子保留为空虚钩子（交互行为本阶段不实现）。
// 旧成员/方法一律注释保留并标注“待 <阶段> 阶段恢复”，不删除代码实体、不编译引用未入阶段源。
#ifndef QCHARTABSTRACTWIDGET_H
#define QCHARTABSTRACTWIDGET_H

#include <QWidget>
#include <QRectF>
#include <QColor>
#include <QList>
#include <memory>

class QChartAbstractLayer;   // 4a：层列表改抽象层类型（二维/三维层公共基类）
class QChartAbstractProjection;
class QPainterChartRenderer;
class QOpenGLChartRenderer;
class GlPlotWidget;   // 实现细节（QOpenGLWidget 子控件），定义于 .cpp

class QPainter;
class QPaintDevice;
class QPaintEvent;
class QResizeEvent;
class QMouseEvent;
class QWheelEvent;

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

    // ===== 渲染后端 =====
    void setRenderBackend(RenderBackend backend);
    RenderBackend renderBackend() const { return m_renderBackend; }
    /// GL 宿主控件（plotArea 对齐；OpenGL 后端激活时非空；测试/宿主 FBO 取证用）
    QWidget* glHostWidget() const { return m_glHostWidgetRaw; }

    // ===== 图层管理（容器）=====
    // 4a：层列表类型 = QChartAbstractLayer*（二维层 layers/2d/QChartLayer、三维层 layers/3d/QChartLayer3D 共同基类）
    void addLayer(QChartAbstractLayer* layer);
    void removeLayer(QChartAbstractLayer* layer);
    void clearLayers();
    QList<QChartAbstractLayer*> layers() const { return m_layers; }

    // ===== 布局 / plotArea =====
    /// 触发 layoutAxes()（重算 plotArea 并广播 plotAreaChanged）
    void relayout();
    QRectF plotArea() const { return m_plotArea; }

    // ===== 4f：最小交互开关 =====
    /// 交互开关（默认开）。关闭后鼠标事件（拖动/滚轮）**不产生任何视图变化**——
    /// 基类的 final 事件处理在派发到 onMouse* / onWheel 钩子前统一拦截；开启时行为不变。
    void setInteractionEnabled(bool on) { m_interactionEnabled = on; }
    bool isInteractionEnabled() const { return m_interactionEnabled; }

    // ===== 边距（边框轴 sizeHint 占用外边距的基值）=====
    void setMargins(qreal left, qreal top, qreal right, qreal bottom);
    qreal marginLeft() const { return m_marginLeft; }
    qreal marginTop() const { return m_marginTop; }
    qreal marginRight() const { return m_marginRight; }
    qreal marginBottom() const { return m_marginBottom; }

    // 旧 API 注释保留（待对应阶段恢复）：
    // void invalidateBackground();  —— 语义已被 renderer viewDirty / layer dataDirty 取代（批次 A 起不再提供）
    // void invalidateForeground();  —— 同上
    // bool saveAsPng(...); bool saveAsSvg(...); bool saveAsPdf(...);  待导出阶段恢复
    // QChartLegend* legend() ...;  待 Phase-1（图例）阶段恢复
    // QChartScene buildScene() ...; 已由「layer 快照 + renderer 管线」取代（见 QChartLayer::snapshotScene）

signals:
    void plotAreaChanged(const QRectF& newPlotArea);
    void projectionChanged(const QChartAbstractProjection* newProjection);

private:
    friend class GlPlotWidget;   // GL 子控件 paintGL 需要调用渲染编排（renderLayersGL）

protected:
    // ---- 子类必须实现 ----
    /// 计算像素绘制区（基于 margins 与边框轴 sizeHint 的外边距占用）
    virtual QRectF calculatePlotArea() const = 0;

    // ---- 子类可重写 ----
    /// 绘制 plotArea 外部的内容（边框轴 drawAtEdge/标题；GPU 模式下 plotArea 由 GL 子控件覆盖）
    virtual void drawExternalContent(QPainter& painter);
    /// 渲染前钩子（子类用于同步 viewRect/dataBounds 驱动链等）
    virtual void onBeforePaint() { }
    /// 4e：plotArea 变化钩子（像素侧驱动）——relayout() 中 plotArea 实际变化后调用；
    /// 子类据此重 fit 相机（2D：相机 fit 模式；3D：fitCameraConfig），默认空实现。
    virtual void onPlotAreaChanged(const QRectF& newPlotArea) { Q_UNUSED(newPlotArea); }
    /// 容器当前投影（子类持有唯一投影；pushContextToLayers 使用）
    virtual const QChartAbstractProjection* projection() const { return nullptr; }
    /// 场景背景色（默认白；主题阶段前由子类/调用方覆盖）
    virtual QColor sceneBackgroundColor() const { return Qt::white; }

    // ---- 事件分发钩子（4f：二维平移/缩放、三维旋转/推拉在此接线；开关见 setInteractionEnabled）----
    virtual void onMousePress(QMouseEvent* e) { Q_UNUSED(e); }
    virtual void onMouseMove(QMouseEvent* e) { Q_UNUSED(e); }
    virtual void onMouseRelease(QMouseEvent* e) { Q_UNUSED(e); }
    virtual void onWheel(QWheelEvent* e) { Q_UNUSED(e); }

    // ---- 渲染管线编排（CPU/GL 共用）----
    // ---- 渲染管线编排（CPU/GL 共用；批次 B2：虚化以支持 3D 容器覆写场景源）----
    /// 把 layer 内容渲染到 device（每 layer 一次快照渲染；CPU 后端）
    virtual void renderLayers(QPaintDevice* device);
    /// GL 后端渲染入口（GlPlotWidget::paintGL 调用；要求当前 GL 上下文）
    virtual void renderLayersGL(QPaintDevice* device);
    /// 同步 plotArea/投影上下文到各 layer（渲染前调用）
    virtual void pushContextToLayers();

    // ===== 成员 =====
    QList<QChartAbstractLayer*> m_layers;   // 非持有（调用方保证生命周期；4a 起为抽象层类型）
    QRectF m_plotArea;
    bool m_layoutDirty = true;
    bool m_interactionEnabled = true;   // 4f：最小交互开关（默认开；关闭后鼠标事件零效果）

    qreal m_marginLeft   = 20.0;
    qreal m_marginTop    = 20.0;
    qreal m_marginRight  = 20.0;
    qreal m_marginBottom = 20.0;

    std::unique_ptr<QPainterChartRenderer> m_cpuRenderer;
    std::unique_ptr<QOpenGLChartRenderer> m_glRenderer;   // OpenGL 后端激活时创建
    std::unique_ptr<GlPlotWidget> m_glHost;               // OpenGL 后端激活时创建（plotArea 对齐）
    QWidget* m_glHostWidgetRaw = nullptr;                 // 宿主裸指针（测试取证；随宿主创建/销毁维护）
    RenderBackend m_renderBackend = RenderBackend::QPainter;

private:
    // ---- 事件分发（final）----
    void paintEvent(QPaintEvent*) override final;
    void resizeEvent(QResizeEvent*) override final;
    void mousePressEvent(QMouseEvent*) override final;
    void mouseMoveEvent(QMouseEvent*) override final;
    void mouseReleaseEvent(QMouseEvent*) override final;
    void wheelEvent(QWheelEvent*) override final;

    void layoutGlHost();

protected:
    /// 请求重绘（GL 后端 = 宿主 update；CPU = 自身 update）
    void scheduleRepaint();
};

#endif // QCHARTABSTRACTWIDGET_H
