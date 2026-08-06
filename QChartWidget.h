// QChartWidget.h —— 图表控件
// 持有唯一 Projection、所有 Axis 和 Layer，管理 viewRect + plotArea
// 坐标链路：View Cartesian → ViewNorm → Pixel（在此完成）
#ifndef QCHARTWIDGET_H
#define QCHARTWIDGET_H
#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <memory>
#include "QChartLayer.h"
#include "QChartProjection.h"

class QChartSeries;

// viewRect 与 plotArea 长宽比匹配策略
enum class ViewRectFitMode {
    Stretch,  // 不调整 viewRect——cartesianToPixel 直接拉伸，图形可能变形
    Fit,      // 扩张 viewRect 较小维度以匹配 plotArea 长宽比，数据始终完整（默认）
    Crop,     // 收缩 viewRect 较大维度以匹配 plotArea 长宽比，可能裁掉部分数据
    Fixed     // 强制 viewRect 匹配指定长宽比（m_fixedAspectRatio），忽略 plotArea
};

class QChartWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool panEnabled  READ isPanEnabled  WRITE setPanEnabled)
    Q_PROPERTY(bool zoomEnabled READ isZoomEnabled WRITE setZoomEnabled)
    Q_PROPERTY(bool cachingEnabled READ isCachingEnabled WRITE setCachingEnabled)
public:
    explicit QChartWidget(QWidget* parent = nullptr);
    ~QChartWidget() override;

    // ===== 组件管理 =====
    void addLayer(QChartLayer* g);
    void removeLayer(QChartLayer* g);
    QList<QChartLayer*> layers() const { return m_layers; }

    void addAxis(QChartAxis* a);
    void removeAxis(QChartAxis* a);
    QList<QChartAxis*> axes() const { return m_axes; }

    // ===== 坐标转换（对所有投影类型通用）=====
    /// View Cartesian → Pixel：线性映射 viewRect → plotArea
    QPointF cartesianToPixel(qreal cx, qreal cy) const;
    /// Pixel → View Cartesian：逆线性映射
    QPointF pixelToCartesian(const QPointF& pixel) const;

    // ===== 视窗操作 =====
    /// 绝对设置 viewRect（动画等），自动重算 dataBounds + fit + invalidate
    void setViewRect(const QRectF& r);
    /// 平移 viewRect（dx/dy 在 View Cartesian 空间）
    void panViewCartesian(qreal dx, qreal dy);
    /// 以 (cx,cy) 为中心缩放 viewRect。factorX/factorY 独立控制两维
    /// （禁交互轴所在维度传 1.0 = 不缩放）。factor<1=放大，>1=缩小
    void zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY);
    /// 语法糖落地：修改 dataBounds dim0 → 重算 viewRect
    void setDataRangeDim0(qreal min, qreal max);
    /// 语法糖落地：修改 dataBounds dim1 → 重算 viewRect
    void setDataRangeDim1(qreal min, qreal max);

    // ===== Projection 与视窗状态 =====
    void setProjection(std::unique_ptr<QChartProjection> proj);
    const QChartProjection* projection() const { return m_projection.get(); }
    QRectF viewRect() const { return m_viewRect; }
    QRectF dataBounds() const { return m_dataBounds; }

    // ===== 布局 =====
    QRectF plotArea() const { return m_plotArea; }
    void setMargins(qreal l, qreal t, qreal r, qreal b);

    // ===== viewRect 匹配策略 =====
    ViewRectFitMode viewRectFitMode() const { return m_fitMode; }
    void setViewRectFitMode(ViewRectFitMode mode);
    qreal fixedAspectRatio() const { return m_fixedAspectRatio; }
    void setFixedAspectRatio(qreal ratio);

    // ===== 缓存与交互 =====
    bool isCachingEnabled() const { return m_cachingEnabled; }
    void setCachingEnabled(bool v) { m_cachingEnabled = v; update(); }
    bool isPanEnabled() const { return m_panEnabled; }
    void setPanEnabled(bool v) { m_panEnabled = v; }
    bool isZoomEnabled() const { return m_zoomEnabled; }
    void setZoomEnabled(bool v) { m_zoomEnabled = v; }

    void invalidateBackground();
    void invalidateForeground();
    void invalidateLayout();

signals:
    void seriesHovered(QChartSeries*, int, bool);
    void viewChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;

    virtual void layoutAxes();
    virtual void drawBackground(QPainter* p);
    virtual void drawForeground(QPainter* p);

    /// 调整 viewRect 使长宽比匹配 plotArea——Polar 下圆不变椭圆
    enum class FitStrategy { KeepWidth, KeepHeight, KeepCenter };
    void fitViewRectToPlotArea(FitStrategy strategy);

    /// 悬停 tooltip 内容：命中点的 Data → Numeric 坐标
    QString buildHoverTooltip(QChartLayer* g, QChartSeries* s, int index) const;

    /// 该维度（0=dim0 水平, 1=dim1 垂直）是否允许交互：
    /// 任一绑定轴 isInteractive()==false → 禁止（如分类轴）
    bool dimensionInteractive(int dim) const;

    // ===== 视窗状态 =====
    std::unique_ptr<QChartProjection> m_projection;
    QRectF m_viewRect;              // View Cartesian 窗口（主状态）
    QRectF m_dataBounds;            // 对应的 Numeric 范围（从 viewRect 反算）
    bool m_viewInitialized = false; // 是否已初始化 viewRect

    // viewRect 与 plotArea 的匹配策略
    ViewRectFitMode m_fitMode = ViewRectFitMode::Fit;
    qreal m_fixedAspectRatio = 1.0; // Fixed 模式下使用

    QList<QChartLayer*> m_layers;
    QList<QChartAxis*> m_axes;
    QRectF m_plotArea;

    // 缓存
    QPixmap m_bgCache, m_fgCache;
    bool m_bgDirty = true, m_fgDirty = true, m_layoutDirty = true, m_cachingEnabled = true;

    // 交互
    bool m_panEnabled = true, m_zoomEnabled = true;
    QPointF m_panStart;
    bool m_panning = false;
    QChartSeries* m_hoverSeries = nullptr;
    int m_hoverIndex = -1;

    // 布局
    qreal m_marginLeft   = 20.0;
    qreal m_marginTop    = 20.0;
    qreal m_marginRight  = 20.0;
    qreal m_marginBottom = 20.0;
};

#endif
