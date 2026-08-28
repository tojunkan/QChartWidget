// QChartWidget.cpp —— 图表控件实现
// 持有唯一 Projection、管理 viewRect、协调 Axis/Layer 绘制
#include "QChartWidget.h"
#include "QChartSeries.h"
#include "QXYSeries.h"
#include "QBarSeries.h"
#include "QDataPoint.h"
#include "QChartProjectionFactory.h"
#include "QPainterChartRenderer.h"
#include "QChartLegend.h"
#include "QChartDebug.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QDebug>
#include <QLoggingCategory>
#include <QGuiApplication>
#include <QImage>
#include <QSize>
#include <QFileInfo>
#include <QDir>
#include <QSvgGenerator>
#include <QPdfWriter>
#include <QPageSize>

Q_LOGGING_CATEGORY(logWidget, "chart.widget")
Q_LOGGING_CATEGORY(logProjection, "chart.projection")
Q_LOGGING_CATEGORY(logFactory, "chart.projection.factory")
Q_LOGGING_CATEGORY(logRender, "chart.render")

// ===== 构造 & 析构 =====
QChartWidget::QChartWidget(QWidget* parent)
    : QWidget(parent)
    , m_camera(std::make_unique<QChartCamera2D>())
    , m_renderer(std::make_unique<QPainterChartRenderer>()) {
    setMouseTracking(true);
    setMinimumSize(200, 150);

    m_legend = new QChartLegend(this);
    connect(m_legend, &QChartLegend::visibleChanged,   this, [this]() { invalidateForeground(); });
    connect(m_legend, &QChartLegend::alignmentChanged, this, [this]() { invalidateForeground(); });
    connect(m_legend, &QChartLegend::textColorChanged, this, [this]() { invalidateForeground(); });

    qCDebug(logWidget) << "QChartWidget created";
}

QChartWidget::~QChartWidget() {
    qDeleteAll(m_layers);
    qDeleteAll(m_axes);
}

// ===== 组件管理 =====
void QChartWidget::addLayer(QChartLayer* g) {
    if (!g) return;

    // 如果还没有 Projection，用 Layer 的坐标系类型创建（默认 Cartesian）
    if (!m_projection) {
        m_projection = QChartProjectionFactory::create(g->coordinateSystem());
        if (!m_viewInitialized && m_projection) {
            m_dataBounds = m_projection->defaultDataBounds();
            m_camera->setViewRect(m_projection->computeViewRect(m_dataBounds));
            m_viewInitialized = true;
            qCDebug(logWidget) << "viewRect initialized from defaultDataBounds:"
                               << m_camera->viewRect();
            fitViewRectToPlotArea(FitStrategy::KeepCenter);
        }
    }

    // Widget 的 projection 是权威来源 → 同步给 Layer
    if (m_projection)
        g->setCoordinateSystem(m_projection->type());

    g->setParent(this);
    m_layers.append(g);
    // 主题默认补推（网格色）
    g->setThemeGridColor(m_theme.gridColor);

    // 动画覆盖层每帧变化 → 刷新前景缓存（不动背景/布局）
    connect(g, &QChartLayer::seriesAdded, this, [this](QChartSeries* s) {
        // A5 调色板循环取色（仅无 override 才占位推进；显式色不占位）
        assignSeriesPaletteColor(s);
        // 样式属性变化（含 QPropertyAnimation 驱动）→ 刷新前景
        connect(s, &QChartSeries::colorChanged,   this, [this]() { invalidateForeground(); });
        connect(s, &QChartSeries::opacityChanged, this, [this]() { invalidateForeground(); });
        connect(s, &QChartSeries::visibleChanged, this, [this]() { invalidateForeground(); });
        connect(s, &QChartSeries::nameChanged, this, [this](const QString&) {
            rebuildLegendItems();          // name 变化影响是否被跳过
            invalidateForeground();
        });
        // 动画覆盖层每帧变化 → 刷新前景缓存
        if (auto* xy = qobject_cast<QXYSeries*>(s)) {
            connect(xy, &QXYSeries::renderOverrideChanged,
                    this, [this]() { invalidateForeground(); });
        } else if (auto* bar = qobject_cast<QBarSeries*>(s)) {
            connect(bar, &QBarSeries::renderOverrideChanged,
                    this, [this]() { invalidateForeground(); });
        }
        rebuildLegendItems();   // 新增 series → 重建图例条目
    });

    // series 移除 → 重建图例条目
    connect(g, &QChartLayer::seriesRemoved, this, [this](QChartSeries*) {
        rebuildLegendItems();
        invalidateForeground();
    });

    rebuildLegendItems();   // 兜底：layer 可能已带 series
    invalidateForeground();
    qCDebug(logWidget) << "Layer added, total:" << m_layers.size();
}

void QChartWidget::removeLayer(QChartLayer* g) {
    m_layers.removeAll(g);
    delete g;
    rebuildLegendItems();
    invalidateForeground();
}

void QChartWidget::addAxis(QChartAxis* a) {
    if (!a) return;

    // Axis 没有 coordinateSystem()——如果 Projection 尚未创建，报错
    if (!m_projection) {
        qWarning() << "QChartWidget::addAxis: no projection set — call addLayer() or setProjection() first";
        return;
    }

    a->setParent(this);
    m_axes.append(a);
    // 主题默认补推（轴色）
    a->setThemeColor(m_theme.axisColor);

    // 语法糖：rangeChanged 信号 → setDataRangeDim0/Dim1
    connect(a, &QChartAxis::rangeChanged, this, [this, a](qreal min, qreal max) {
        Qt::Alignment align = a->alignment();
        if (align == Qt::AlignBottom || align == Qt::AlignTop || align == Qt::AlignHCenter) {
            setDataRangeDim0(min, max);
        } else if (align == Qt::AlignLeft || align == Qt::AlignRight || align == Qt::AlignVCenter) {
            setDataRangeDim1(min, max);
        }
    });

    // 可见性/样式变化 → 刷新背景
    connect(a, &QChartAxis::visibleChanged, this, [this]() { invalidateBackground(); });
    connect(a, &QChartAxis::styleChanged,  this, [this]() { invalidateBackground(); });
    connect(a, &QChartAxis::tickCountChanged, this, [this]() { invalidateBackground(); });

    invalidateLayout();
    invalidateBackground();
    qCDebug(logWidget) << "Axis added, total:" << m_axes.size();
}

void QChartWidget::removeAxis(QChartAxis* a) {
    m_axes.removeAll(a);
    delete a;
    invalidateLayout();
    invalidateBackground();
}

// ===== viewRect ↔ plotArea 长宽比同步 =====
void QChartWidget::setViewRectFitMode(ViewRectFitMode mode) {
    if (m_camera->fitMode() == mode) return;
    m_camera->setFitMode(mode);
    qCDebug(logWidget) << "viewRectFitMode:" << (int)mode;
    // 立即按新模式重排
    fitViewRectToPlotArea(FitStrategy::KeepCenter);
    invalidateBackground();
    invalidateForeground();
}

void QChartWidget::setScale(qreal ratio) {
    if (ratio <= 0.0) {
        qWarning() << "setScale: ratio must be > 0, ignoring" << ratio;
        return;
    }
    m_camera->setScale(ratio);
    qCDebug(logWidget) << "Scale:" << ratio;
    invalidateBackground();
    invalidateForeground();
}

void QChartWidget::fitViewRectToPlotArea(FitStrategy strategy) {
    if (!m_projection) return;
    if (m_plotArea.width() <= 0.0 || m_plotArea.height() <= 0.0) return;

    // 相机只做 viewRect 几何拟合；dataBounds 依赖 projection，故由 Widget 反算。
    // 仅在 viewRect 实际变化时反算，保持与旧实现一致（避免 Polar 下
    // computeDataBounds(computeViewRect(dataBounds)) 的往返漂移）。
    if (m_camera->fitViewRectToPlotArea(m_plotArea, strategy)) {
        m_dataBounds = m_projection->computeDataBounds(m_camera->viewRect());
        qCDebug(logWidget) << "fitViewRectToPlotArea: after" << m_camera->viewRect()
                           << "dataBounds=" << m_dataBounds;
    }
}

// ===== 坐标转换（对所有投影类型通用）=====
// 线性映射的唯一实现在 QChartCamera2D；此处仅为 Widget 公共 API 转发。
QPointF QChartWidget::cartesianToPixel(qreal cx, qreal cy) const {
    return m_camera->cartesianToPixel(m_plotArea, cx, cy);
}

QPointF QChartWidget::pixelToCartesian(const QPointF& pixel) const {
    return m_camera->pixelToCartesian(m_plotArea, pixel);
}

// ===== 视窗操作 =====
// ===== 绝对设置 viewRect（相机动画等场景）=====
// 按解耦哲学：viewRect 是数据窗口（相机状态），这里"设置什么就是什么"，
// 不做 fit 修正——长宽比由调用者负责（QViewRectAnimation 内部用 plotArea
// 快照保证）。fit 只在数据范围/投影变更时发生
void QChartWidget::setViewRect(const QRectF& r) {
    m_camera->setViewRect(r);
    if (m_projection)
        m_dataBounds = m_projection->computeDataBounds(m_camera->viewRect());
    qCDebug(logWidget) << "setViewRect:" << r << "→ viewRect=" << m_camera->viewRect()
                       << "dataBounds=" << m_dataBounds;
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

void QChartWidget::panViewCartesian(qreal dx, qreal dy) {
    m_camera->panViewCartesian(dx, dy);
    // 重算 dataBounds
    if (m_projection)
        m_dataBounds = m_projection->computeDataBounds(m_camera->viewRect());
    qCDebug(logWidget) << "panViewCartesian: dx=" << dx << "dy=" << dy
                       << "→ viewRect=" << m_camera->viewRect()
                       << "dataBounds=" << m_dataBounds;
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

void QChartWidget::zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY) {
    if (factorX <= 0.0 || factorY <= 0.0 || !m_projection) return;
    m_camera->zoomViewCartesian(cx, cy, factorX, factorY);
    m_dataBounds = m_projection->computeDataBounds(m_camera->viewRect());
    fitViewRectToPlotArea(FitStrategy::KeepCenter);
    qCDebug(logWidget) << "zoomViewCartesian: factorX=" << factorX
                       << "factorY=" << factorY
                       << "→ viewRect=" << m_camera->viewRect()
                       << "dataBounds=" << m_dataBounds;
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

// ===== 维度交互检查 =====
// 分类轴等离散域轴不可交互——任一绑定轴禁用则该维度整体禁 pan/zoom
bool QChartWidget::dimensionInteractive(int dim) const {
    for (auto* g : m_layers) {
        QChartAxis* a = (dim == 0) ? g->axisX() : g->axisY();
        if (a && !a->isInteractive())
            return false;
    }
    return true;
}

void QChartWidget::setDataRangeDim0(qreal min, qreal max) {
    m_dataBounds.setLeft(min);
    m_dataBounds.setWidth(max - min);
    if (m_projection) {
        m_camera->setViewRect(m_projection->computeViewRect(m_dataBounds));
        qCDebug(logWidget) << "setDataRangeDim0:" << min << "→" << max
                           << "viewRect=" << m_camera->viewRect();
        fitViewRectToPlotArea(FitStrategy::KeepCenter);
    }
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

void QChartWidget::setDataRangeDim1(qreal min, qreal max) {
    m_dataBounds.setTop(min);
    m_dataBounds.setHeight(max - min);
    if (m_projection) {
        m_camera->setViewRect(m_projection->computeViewRect(m_dataBounds));
        qCDebug(logWidget) << "setDataRangeDim1:" << min << "→" << max
                           << "viewRect=" << m_camera->viewRect();
        fitViewRectToPlotArea(FitStrategy::KeepCenter);
    }
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

void QChartWidget::setProjection(std::unique_ptr<QChartProjection> proj) {
    m_projection = std::move(proj);
    if (m_projection && !m_viewInitialized) {
        m_dataBounds = m_projection->defaultDataBounds();
        m_camera->setViewRect(m_projection->computeViewRect(m_dataBounds));
        m_viewInitialized = true;
        qCDebug(logWidget) << "setProjection: viewRect initialized from defaultDataBounds:"
                           << m_camera->viewRect();
        fitViewRectToPlotArea(FitStrategy::KeepCenter);
    }
    // 同步 projection 类型给所有 Layer
    if (m_projection) {
        auto type = m_projection->type();
        for (auto* g : m_layers)
            g->setCoordinateSystem(type);
    }
    invalidateBackground();
    invalidateForeground();
}

// ===== 布局 =====
void QChartWidget::setMargins(qreal l, qreal t, qreal r, qreal b) {
    m_marginLeft = l; m_marginTop = t;
    m_marginRight = r; m_marginBottom = b;
    layoutAxes();
    update();
}

void QChartWidget::layoutAxes() {
    m_plotArea = plotAreaForSize(this->size());
    qCDebug(logWidget) << "layoutAxes: plotArea=" << m_plotArea;
    // m_camera->fitViewRectToPlotArea(m_plotArea, FitStrategy::KeepCenter);
    // 注意：resize 只更新 plotArea，不动 viewRect。
    // viewRect 是数据窗口（相机状态），plotArea 是像素窗口——两者解耦：
    // 拉伸窗口 = 变相调整视图（像素映射拉伸），fit 只在数据范围/投影
    // 显式变更时发生（setDataRange/setProjection/setViewRectFitMode）。
    // 若 resize 也去 fit，会从"已收缩的当前值"反复收缩 → 累积漂移
}

QRectF QChartWidget::plotAreaForSize(const QSize& size) const {
    qreal left   = m_marginLeft;
    qreal top    = m_marginTop;
    qreal right  = m_marginRight;
    qreal bottom = m_marginBottom;

    QFont font = this->font();
    for (auto* axis : m_axes) {
        if (!axis || !axis->isVisible()) continue;
        QSizeF hint = axis->sizeHint(font);
        switch (axis->alignment()) {
        case Qt::AlignLeft:   left   = qMax(left,   hint.width());  break;
        case Qt::AlignRight:  right  = qMax(right,  hint.width());  break;
        case Qt::AlignTop:    top    = qMax(top,    hint.height()); break;
        case Qt::AlignBottom: bottom = qMax(bottom, hint.height()); break;
        default: break; // HCenter/VCenter 不占边距
        }
    }

    return QRectF(left, top,
                  size.width() - left - right,
                  size.height() - top - bottom);
}

// ===== 缓存控制 =====
void QChartWidget::invalidateBackground() { m_renderer->invalidateBackground(); update(); }
void QChartWidget::invalidateForeground() { m_renderer->invalidateForeground(); update(); }
void QChartWidget::invalidateLayout()      { m_layoutDirty = true; update(); }

// ===== 事件 =====
void QChartWidget::resizeEvent(QResizeEvent*) {
    m_renderer->invalidateBackground();
    m_renderer->invalidateForeground();
    layoutAxes();
}

void QChartWidget::paintEvent(QPaintEvent*) {
    if (m_layoutDirty) {
        layoutAxes();
        m_layoutDirty = false;
        m_renderer->invalidateBackground();
        m_renderer->invalidateForeground();
    }

    // 组装场景快照（§8.2 钩子：3D 子类重写 buildScreenScene 注入 3D 段），
    // 渲染器只依赖快照 + 目标 device，不反向依赖 Widget
    const QChartScene scene = buildScreenScene();

    m_renderer->render(scene, this);
}

// §8.2：默认屏显场景组装（= 原 paintEvent 内逻辑，行为保持；3D 子类重写）
QChartScene QChartWidget::buildScreenScene() const {
    QChartScene scene;
    scene.plotArea   = m_plotArea;
    scene.dataBounds = m_dataBounds;
    scene.viewRect   = m_camera->viewRect();
    scene.projection = m_tempProjection ? m_tempProjection : m_projection.get();
    scene.axes       = m_axes;
    scene.layers     = m_layers;
    scene.backgroundColor = backgroundColor();   // 有效色（override 或主题默认）
    scene.legend     = m_legend;
    scene.legendItems = m_legendItems;
    return scene;
}

// ===== 主题 =====
void QChartWidget::setTheme(QChartTheme::Preset preset) {
    setTheme(preset == QChartTheme::Preset::Dark ? QChartTheme::dark()
                                                 : QChartTheme::light());
}

void QChartWidget::setTheme(const QChartTheme& theme) {
    m_theme = theme;
    pushTheme();
    invalidateBackground();
    invalidateForeground();
}

void QChartWidget::setBackgroundColor(const QColor& c) {
    m_backgroundColorOverride = c;
    invalidateBackground();
}

void QChartWidget::clearBackgroundColor() {
    m_backgroundColorOverride.reset();
    invalidateBackground();
}

QColor QChartWidget::backgroundColor() const {
    return m_backgroundColorOverride.value_or(m_theme.backgroundColor);
}

void QChartWidget::setFollowSystemPalette(bool on) {
    m_followSystemPalette = on;
}

bool QChartWidget::event(QEvent* e) {
    // A4：系统深/浅自动跟随。paletteChanged 信号在 Qt 6.4 已废弃，
    // 改用 QEvent::ApplicationPaletteChange（setPalette 时同步投递给所有 widget）。
    if (e->type() == QEvent::ApplicationPaletteChange && m_followSystemPalette) {
        const QPalette& pal = QGuiApplication::palette();
        setTheme(pal.color(QPalette::Window).lightness() < 128
                 ? QChartTheme::Preset::Dark
                 : QChartTheme::Preset::Light);
    }
    return QWidget::event(e);
}

void QChartWidget::pushTheme() {
    // 轴 / 网格 / 系列 / 图例文字色（A5：系列按 add 顺序循环取色，重排索引）
    for (auto* a : m_axes)
        if (a) a->setThemeColor(m_theme.axisColor);

    if (m_legend)
        m_legend->setThemeTextColor(m_theme.textColor);

    m_seriesColorIndex = 0;   // 重排：按 m_layers add 顺序重新分配调色板
    for (auto* g : m_layers) {
        if (!g) continue;
        g->setThemeGridColor(m_theme.gridColor);
        for (auto* s : g->seriesList())
            assignSeriesPaletteColor(s);
    }
}

bool QChartWidget::assignSeriesPaletteColor(QChartSeries* s) {
    if (!s) return false;
    if (s->colorOverride()) return false;               // 显式色不占位（可预测）
    if (m_theme.seriesPalette.isEmpty()) return false;  // 空调色板退化为不配色
    s->setThemeColor(m_theme.seriesPalette[m_seriesColorIndex % m_theme.seriesPalette.size()]);
    ++m_seriesColorIndex;
    return true;
}

void QChartWidget::rebuildLegendItems() {
    m_legendItems.clear();
    for (auto* g : m_layers) {
        if (!g) continue;
        for (auto* s : g->seriesList()) {
            if (!s) continue;
            if (s->name().isEmpty()) continue;   // B3：跳过空 name
            m_legendItems.append(s);
        }
    }
}

// ===== 导出（C1/C3/C4/C5，统一走 renderUncached）=====
QChartScene QChartWidget::buildExportScene(QChartExportScope scope, const QSize& size,
                                           QSizeF& outDeviceSize) const {
    QChartScene scene;
    scene.dataBounds = m_dataBounds;
    scene.viewRect   = m_camera->viewRect();
    scene.projection = m_projection.get();   // 导出用真实投影（不用 temp 动画投影）
    scene.axes       = m_axes;
    scene.layers     = m_layers;
    scene.legend     = m_legend;
    scene.legendItems = m_legendItems;
    scene.exportMode = true;   // 导出模式：跳过调试黄框等屏显专用绘制
    // C5：透明开关开启 → 背景置 invalid（不填充）
    scene.backgroundColor = m_exportTransparentBackground ? QColor() : backgroundColor();

    if (scope == QChartExportScope::PlotArea) {
        outDeviceSize = size.isEmpty() ? m_plotArea.size() : QSizeF(size);
        scene.plotArea = QRectF(QPointF(0, 0), outDeviceSize);   // 整设备 = plotArea
    } else {
        outDeviceSize = size.isEmpty() ? QSizeF(this->size()) : QSizeF(size);
        scene.plotArea = plotAreaForSize(outDeviceSize.toSize()); // 等比重算（含边距/轴）
    }
    return scene;
}

bool QChartWidget::saveAsPng(const QString& path, const QSize& size, qreal devicePixelRatio) {
    return saveAsPng(path, QChartExportScope::WholeWidget, size, devicePixelRatio);
}

bool QChartWidget::saveAsPng(const QString& path, QChartExportScope scope,
                             const QSize& size, qreal devicePixelRatio) {
    if (path.isEmpty() || devicePixelRatio <= 0.0) return false;
    QSizeF deviceSize;
    const QChartScene scene = buildExportScene(scope, size, deviceSize);

    // PNG 输出像素 = size * dpr（写进 QImage 的 dpr）
    QSize pixelSize(qCeil(deviceSize.width() * devicePixelRatio),
                    qCeil(deviceSize.height() * devicePixelRatio));
    QImage img(pixelSize, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(devicePixelRatio);
    img.fill(Qt::transparent);
    m_renderer->renderUncached(scene, &img);
    return img.save(path, "PNG");
}

bool QChartWidget::saveAsSvg(const QString& path, const QSize& size) {
    return saveAsSvg(path, QChartExportScope::WholeWidget, size);
}

bool QChartWidget::saveAsSvg(const QString& path, QChartExportScope scope, const QSize& size) {
    if (path.isEmpty()) return false;
    if (!QFileInfo(path).absoluteDir().exists()) return false;   // 非法路径
    QSizeF deviceSize;
    const QChartScene scene = buildExportScene(scope, size, deviceSize);

    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(deviceSize.toSize());
    gen.setViewBox(QRectF(QPointF(0, 0), deviceSize));
    gen.setTitle("QChartWidget");
    m_renderer->renderUncached(scene, &gen);
    return QFileInfo::exists(path);   // 写失败则文件不存在 → false
}

bool QChartWidget::saveAsPdf(const QString& path, const QSize& size) {
    return saveAsPdf(path, QChartExportScope::WholeWidget, size);
}

bool QChartWidget::saveAsPdf(const QString& path, QChartExportScope scope, const QSize& size) {
    if (path.isEmpty()) return false;
    if (!QFileInfo(path).absoluteDir().exists()) return false;   // 非法路径
    QSizeF deviceSize;
    QChartScene scene = buildExportScene(scope, size, deviceSize);
    // PDF 无 alpha：忽略透明开关，始终填充背景（design_export.md C5）
    scene.backgroundColor = backgroundColor();

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(deviceSize, QPageSize::Point));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setResolution(72);   // 1 point = 1 px，保证 1:1
    m_renderer->renderUncached(scene, &writer);
    return QFileInfo::exists(path);   // 写失败则文件不存在 → false
}

// ===== 鼠标事件 =====
void QChartWidget::mousePressEvent(QMouseEvent* e) {
    // B4：图例点击切换系列可见性（先于 pan 分支）
    if (e->button() == Qt::LeftButton && m_legend->isVisible()) {
        QChartSeries* s = m_legend->seriesAt(e->pos(), m_plotArea, m_legendItems);
        if (s) {
            s->setVisible(!s->isVisible());
            return;   // 命中图例：不进入 pan
        }
    }

    if (e->button() == Qt::LeftButton && m_panEnabled) {
        m_panStart = e->pos();
        m_panning = true;
        setCursor(Qt::ClosedHandCursor);
        qCDebug(logWidget) << "Pan started at" << m_panStart;
    }
    QWidget::mousePressEvent(e);
}

void QChartWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_panning && m_projection) {
        QPointF currentPos = e->pos();
        // 像素位移 → View Cartesian 位移
        QPointF startCart = pixelToCartesian(m_panStart);
        QPointF currCart = pixelToCartesian(currentPos);
        qreal dx = startCart.x() - currCart.x();
        qreal dy = startCart.y() - currCart.y();
        // 禁交互维度（分类轴）不平移——离散域拖动会撕裂标签与数据关系
        if (!dimensionInteractive(0)) dx = 0.0;
        if (!dimensionInteractive(1)) dy = 0.0;
        if (dx == 0.0 && dy == 0.0) {
            m_panStart = currentPos; // 仍要更新起点，否则恢复交互后跳变
            return;
        }
        panViewCartesian(dx, dy);
        m_panStart = currentPos;
        return;
    }

    // ── 悬停检测：命中 Series 数据点 → 显示 tooltip（Numeric 坐标）──
    DrawContext ctx;
    ctx.plotArea   = m_plotArea;
    ctx.dataBounds = m_dataBounds;
    ctx.viewRect   = m_camera->viewRect();
    ctx.projection = m_tempProjection ? m_tempProjection : m_projection.get();

    QChartSeries* hoverSeries = nullptr;
    int hoverIndex = -1;
    QPointF hoverPos;

    for (auto* g : m_layers) {
        auto result = g->hitTest(e->pos(), ctx);
        if (result.series) {
            hoverSeries = result.series;
            hoverIndex = result.index;
            hoverPos = e->pos();
            break;
        }
    }

    // 命中状态变化 → 更新 tooltip / 清空
    if (hoverSeries != m_hoverSeries || hoverIndex != m_hoverIndex) {
        m_hoverSeries = hoverSeries;
        m_hoverIndex = hoverIndex;

        if (hoverSeries) {
            setCursor(Qt::PointingHandCursor);
            emit seriesHovered(hoverSeries, hoverIndex, true);

            // 显示 tooltip：Numeric 坐标
            // 通过 Layer 的轴把命中点的 Data 转成 Numeric 显示
            // 注意：QToolTip::showText 需要全局坐标，hoverPos 是本地坐标 → mapToGlobal
            if (auto* g = qobject_cast<QChartLayer*>(hoverSeries->parent())) {
                QString tip = buildHoverTooltip(g, hoverSeries, hoverIndex);
                QToolTip::showText(mapToGlobal(hoverPos.toPoint()), tip, this);
            }
        } else {
            setCursor(Qt::ArrowCursor);
            QToolTip::hideText();
            if (m_hoverSeries)
                emit seriesHovered(m_hoverSeries, m_hoverIndex, false);
        }
    }
    QWidget::mouseMoveEvent(e);
}

// ── tooltip 内容：命中点的 Data → Numeric 坐标 ──
QString QChartWidget::buildHoverTooltip(QChartLayer* g,
                                        QChartSeries* s, int index) const {
    // 只对 QXYSeries 有点索引；其他类型返回系列名
    auto* xy = qobject_cast<QXYSeries*>(s);
    if (!xy || index < 0 || index >= xy->count())
        return s->name();

    QDataPoint pt = xy->at(index);
    QString dim0Name = m_projection ? m_projection->dimensionName(0) : "x";
    QString dim1Name = m_projection ? m_projection->dimensionName(1) : "y";

    // Data → Numeric（Axis 转换）
    qreal num0 = g->axisX() ? g->axisX()->toNumeric(pt.x()) : pt.x().toDouble();
    qreal num1 = g->axisY() ? g->axisY()->toNumeric(pt.y()) : pt.y().toDouble();

    return QString("%1 (%2, %3)")
        .arg(s->name())
        .arg(dim0Name + "=" + QString::number(num0, 'g', 6),
             dim1Name + "=" + QString::number(num1, 'g', 6));
}

void QChartWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(e);
}

void QChartWidget::wheelEvent(QWheelEvent* e) {
    if (!m_zoomEnabled || !m_projection) return;
    QPointF pos = e->position();
    if (!m_plotArea.contains(pos)) return;

    int delta = e->angleDelta().y();
    if (delta == 0) return;

    // 缩放因子：向上滚 = 放大（factor<1），向下滚 = 缩小（factor>1）
    static constexpr qreal SCALE_SENSITIVITY = 0.001;
    qreal factor = std::exp(-delta * SCALE_SENSITIVITY);
    factor = qBound(0.8, factor, 1.25);

    // 禁交互维度（分类轴）不缩放——离散域缩放没有意义
    qreal factorX = dimensionInteractive(0) ? factor : 1.0;
    qreal factorY = dimensionInteractive(1) ? factor : 1.0;
    if (factorX == 1.0 && factorY == 1.0) return;

    // 以鼠标位置的 View Cartesian 坐标为中心缩放
    QPointF cartCenter = pixelToCartesian(pos);
    zoomViewCartesian(cartCenter.x(), cartCenter.y(), factorX, factorY);
}

void QChartWidget::leaveEvent(QEvent*) {
    if (m_hoverSeries) {
        emit seriesHovered(m_hoverSeries, m_hoverIndex, false);
        m_hoverSeries = nullptr;
        m_hoverIndex = -1;
        setCursor(Qt::ArrowCursor);
    }
}
