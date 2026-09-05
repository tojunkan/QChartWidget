// QChartWidget.h —— 图表控件
// 持有唯一 Projection、所有 Axis 和 Layer；viewRect 几何已抽到 QChartCamera2D，
// 这里保留 m_dataBounds（依赖 projection->computeDataBounds）与交互/绘制编排。
// 坐标链路：View Cartesian → ViewNorm → Pixel（由 QChartCamera2D 完成）
#ifndef QCHARTWIDGET_H
#define QCHARTWIDGET_H
#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <memory>
#include <optional>
#include "QChartCamera.h"  // ViewRectFitMode + 相机
#include "QChartLayer.h"
#include "QChartProjection.h"
#include "QChartRenderer.h" // QChartScene + 渲染器接口
#include "QChartTheme.h"    // QChartTheme + Preset
#include "QChartLegend.h"   // 图例（内联方法需完整类型）

class QChartSeries;

// 导出范围（C3：默认全 widget；「仅 plotArea」可选，会丢刻度标签/轴标题）
enum class QChartExportScope { WholeWidget, PlotArea };

class QChartWidget : public QChartAbstractWidget {
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
    /// 临时投影（动画用）：仅影响渲染路径（DrawContext），不参与管理逻辑。
    /// 动画结束后必须 clearTemporaryProjection()
    void setTemporaryProjection(QChartProjection* p) { m_tempProjection = p; invalidateForeground(); }
    void clearTemporaryProjection() { m_tempProjection = nullptr; invalidateForeground(); }
    QRectF viewRect() const { return m_camera->viewRect(); }
    QRectF dataBounds() const { return m_dataBounds; }

    // ===== 布局 =====
    QRectF plotArea() const { return m_plotArea; }
    void setMargins(qreal l, qreal t, qreal r, qreal b);

    // ===== viewRect 匹配策略 =====
    ViewRectFitMode viewRectFitMode() const { return m_camera->fitMode(); }
    void setViewRectFitMode(ViewRectFitMode mode);
    qreal scale() const { return m_camera->scale(); }
    void setScale(qreal ratio);

    // ===== 主题 =====
    /// 一键切换预设主题（A2）
    void setTheme(QChartTheme::Preset preset);
    /// 进阶：自定义主题（同 struct）
    void setTheme(const QChartTheme& theme);
    /// 当前应用的主题（base，不含 override）
    QChartTheme theme() const { return m_theme; }

    // 背景逐项覆盖（override 模式，与轴/网格/系列一致）
    void setBackgroundColor(const QColor& c);
    void clearBackgroundColor();
    QColor backgroundColor() const;

    // 系统深/浅自动跟随（A4：默认关）
    void setFollowSystemPalette(bool on);
    bool followSystemPalette() const { return m_followSystemPalette; }

    // ===== 图例（Phase 1 overlay）=====
    QChartLegend* legend() const { return m_legend; }
    void setLegendVisible(bool v) { m_legend->setVisible(v); }
    bool isLegendVisible() const { return m_legend->isVisible(); }
    void setLegendAlignment(Qt::Alignment a) { m_legend->setAlignment(a); }
    /// 当前图例条目（汇总所有 layer、跳过空 name、按 add 顺序；供测试/交互）
    QList<QChartSeries*> legendItems() const { return m_legendItems; }

    // ===== 导出（C1/C3/C4/C5）=====
    /// 便捷重载（默认 WholeWidget）
    bool saveAsPng(const QString& path, const QSize& size = {}, qreal devicePixelRatio = 1.0);
    bool saveAsSvg(const QString& path, const QSize& size = {});
    bool saveAsPdf(const QString& path, const QSize& size = {});
    /// 显式范围重载
    bool saveAsPng(const QString& path, QChartExportScope scope, const QSize& size = {}, qreal devicePixelRatio = 1.0);
    bool saveAsSvg(const QString& path, QChartExportScope scope, const QSize& size = {});
    bool saveAsPdf(const QString& path, QChartExportScope scope, const QSize& size = {});
    /// 透明背景开关（C5：默认 false = 用主题背景填充）
    void setExportTransparentBackground(bool v) { m_exportTransparentBackground = v; }
    bool exportTransparentBackground() const { return m_exportTransparentBackground; }

    // ===== 缓存与交互 =====
    bool isCachingEnabled() const { return m_renderer->isCachingEnabled(); }
    void setCachingEnabled(bool v) { m_renderer->setCachingEnabled(v); update(); }
    bool isPanEnabled() const { return m_panEnabled; }
    void setPanEnabled(bool v) { m_panEnabled = v; }
    bool isZoomEnabled() const { return m_zoomEnabled; }
    void setZoomEnabled(bool v) { m_zoomEnabled = v; }

    virtual void invalidateBackground();
    virtual void invalidateForeground();
    void invalidateLayout();

signals:
    void seriesHovered(QChartSeries*, int, bool);
    void viewChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool event(QEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;

    /// §8.2 钩子：组装屏显场景快照（默认 = 2D 场景组装；3D 子类重写填 3D 段）
    /// 由 paintEvent 调用；渲染器只依赖快照 + 目标 device，不反向依赖 Widget
    virtual QChartScene buildScreenScene() const;

    /// §8.2 钩子：组装导出场景（按 scope/size 计算设备尺寸与 plotArea）；
    /// 改 protected virtual，3D 子类可注入 3D 段（导出 3D 场景低成本；验收不要求）
    virtual QChartScene buildExportScene(QChartExportScope scope, const QSize& size,
                                         QSizeF& outDeviceSize) const;

    virtual void layoutAxes();

    /// 调整 viewRect 使长宽比匹配 plotArea——Polar 下圆不变椭圆
    /// 只负责触发相机拟合几何 + 反算 dataBounds（dataBounds 依赖 projection，故留在 Widget）
    using FitStrategy = QChartCamera2D::FitStrategy;
    void fitViewRectToPlotArea(FitStrategy strategy);

    /// 悬停 tooltip 内容：命中点的 Data → Numeric 坐标
    QString buildHoverTooltip(QChartLayer* g, QChartSeries* s, int index) const;

    /// 该维度（0=dim0 水平, 1=dim1 垂直）是否允许交互：
    /// 任一绑定轴 isInteractive()==false → 禁止（如分类轴）
    bool dimensionInteractive(int dim) const;

    // ===== 视窗状态 =====
    std::unique_ptr<QChartProjection> m_projection;
    QChartProjection* m_tempProjection = nullptr; // 动画临时投影（非持有，仅渲染用）
    std::unique_ptr<QChartCamera2D> m_camera;   // viewRect 几何 + fit 策略 + View↔Pixel 映射
    std::unique_ptr<QChartRenderer> m_renderer; // 渲染后端（缓存 + 绘制编排）
    QRectF m_dataBounds;            // 对应的 Numeric 范围（从 viewRect 反算，Widget 持有）
    bool m_viewInitialized = false; // 是否已初始化 viewRect

    QList<QChartLayer*> m_layers;
    QList<QChartAxis*> m_axes;
    QRectF m_plotArea;

    // 布局脏标记（缓存脏标记已迁入渲染器）
    bool m_layoutDirty = true;

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

private:
    /// 推送当前主题默认色到所有子组件（axis/layer/series/legend，A5 调色板循环）
    void pushTheme();
    /// A5：给无 override 的 series 分配 palette[index % size]，推进索引；
    /// 显式色/空调色板时不分配、不推进。返回是否分配。
    bool assignSeriesPaletteColor(QChartSeries* s);
    /// 重建 m_legendItems（汇总所有 layer、跳过空 name、按 add 顺序）
    void rebuildLegendItems();
    /// 给定尺寸下重算 plotArea（复用 layoutAxes 的 margin/sizeHint 逻辑，供 WholeWidget 导出）
    QRectF plotAreaForSize(const QSize& size) const;

    QChartTheme m_theme = QChartTheme::light();
    std::optional<QColor> m_backgroundColorOverride;   // 显式背景覆盖（setBackgroundColor）
    bool m_followSystemPalette = false;
    int m_seriesColorIndex = 0;                        // A5 全局 add 顺序索引（跨 layer）
    QChartLegend* m_legend = nullptr;                  // 图例（构造函数创建，parented）
    QList<QChartSeries*> m_legendItems;                // 图例条目（paint 前重建）
    bool m_exportTransparentBackground = false;        // C5：导出透明背景开关
};

#endif
