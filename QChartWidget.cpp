// QChartWidget.cpp —— 图表控件实现
// 持有唯一 Projection、管理 viewRect、协调 Axis/Layer 绘制
#include "QChartWidget.h"
#include "QChartSeries.h"
#include "QXYSeries.h"
#include "QBarSeries.h"
#include "QDataPoint.h"
#include "QChartProjectionFactory.h"
#include "QChartDebug.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logWidget, "chart.widget")
Q_LOGGING_CATEGORY(logProjection, "chart.projection")
Q_LOGGING_CATEGORY(logFactory, "chart.projection.factory")
Q_LOGGING_CATEGORY(logRender, "chart.render")

// ===== 构造 & 析构 =====
QChartWidget::QChartWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(200, 150);
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
            m_viewRect = m_projection->computeViewRect(m_dataBounds);
            m_viewInitialized = true;
            qCDebug(logWidget) << "viewRect initialized from defaultDataBounds:"
                               << m_viewRect;
            fitViewRectToPlotArea(FitStrategy::KeepCenter);
        }
    }

    // Widget 的 projection 是权威来源 → 同步给 Layer
    if (m_projection)
        g->setCoordinateSystem(m_projection->type());

    g->setParent(this);
    m_layers.append(g);

    // 动画覆盖层每帧变化 → 刷新前景缓存（不动背景/布局）
    connect(g, &QChartLayer::seriesAdded, this, [this](QChartSeries* s) {
        // 样式属性变化（含 QPropertyAnimation 驱动）→ 刷新前景
        connect(s, &QChartSeries::colorChanged,   this, [this]() { invalidateForeground(); });
        connect(s, &QChartSeries::opacityChanged, this, [this]() { invalidateForeground(); });
        connect(s, &QChartSeries::visibleChanged, this, [this]() { invalidateForeground(); });
        // 动画覆盖层每帧变化 → 刷新前景缓存
        if (auto* xy = qobject_cast<QXYSeries*>(s)) {
            connect(xy, &QXYSeries::renderOverrideChanged,
                    this, [this]() { invalidateForeground(); });
        } else if (auto* bar = qobject_cast<QBarSeries*>(s)) {
            connect(bar, &QBarSeries::renderOverrideChanged,
                    this, [this]() { invalidateForeground(); });
        }
    });

    invalidateForeground();
    qCDebug(logWidget) << "Layer added, total:" << m_layers.size();
}

void QChartWidget::removeLayer(QChartLayer* g) {
    m_layers.removeAll(g);
    delete g;
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
    if (m_fitMode == mode) return;
    m_fitMode = mode;
    qCDebug(logWidget) << "viewRectFitMode:" << (int)mode;
    // 立即按新模式重排
    fitViewRectToPlotArea(FitStrategy::KeepCenter);
    invalidateBackground();
    invalidateForeground();
}

void QChartWidget::setFixedAspectRatio(qreal ratio) {
    if (ratio <= 0.0) {
        qWarning() << "setFixedAspectRatio: ratio must be > 0, ignoring" << ratio;
        return;
    }
    m_fixedAspectRatio = ratio;
    qCDebug(logWidget) << "fixedAspectRatio:" << ratio;
    fitViewRectToPlotArea(FitStrategy::KeepCenter);
    invalidateBackground();
    invalidateForeground();
}

void QChartWidget::fitViewRectToPlotArea(FitStrategy strategy) {
    if (!m_projection) return;

    // Stretch 模式：不调整 viewRect，直接拉伸
    if (m_fitMode == ViewRectFitMode::Stretch) return;

    if (m_plotArea.width() <= 0.0 || m_plotArea.height() <= 0.0) return;

    // 目标长宽比：Fixed 模式用用户指定的，否则用 plotArea 的
    qreal targetAspect = (m_fitMode == ViewRectFitMode::Fixed)
        ? m_fixedAspectRatio
        : m_plotArea.width() / m_plotArea.height();

    qreal viewAspect = m_viewRect.width() / m_viewRect.height();

    // 长宽比已经匹配（1% 容差）→ 跳过
    if (qAbs(targetAspect - viewAspect) < 0.01 * targetAspect) return;

    qCDebug(logWidget) << "fitViewRectToPlotArea: before" << m_viewRect
                       << "mode=" << (int)m_fitMode
                       << "strategy=" << (int)strategy
                       << "targetAspect=" << targetAspect << "viewAspect=" << viewAspect;

    bool expand; // true=扩张，false=收缩
    if (m_fitMode == ViewRectFitMode::Crop) {
        // Crop：收缩较大维度，裁掉超出部分
        expand = false;
    } else {
        // Fit / Fixed：扩张较小维度，数据完整
        expand = true;
    }

    if (expand) {
        switch (strategy) {
        case FitStrategy::KeepWidth:
            // 用户设了 dim0 → 锁宽度，只调高度
            {
                qreal newH = m_viewRect.width() / targetAspect;
                qreal d = (newH - m_viewRect.height()) / 2.0;
                m_viewRect.adjust(0.0, -d, 0.0, d);
            }
            break;
        case FitStrategy::KeepHeight:
            // 用户设了 dim1 → 锁高度，只调宽度
            {
                qreal newW = m_viewRect.height() * targetAspect;
                qreal d = (newW - m_viewRect.width()) / 2.0;
                m_viewRect.adjust(-d, 0.0, d, 0.0);
            }
            break;
        case FitStrategy::KeepCenter:
            // 初始化/布局变化 → 双向均等扩张
            if (targetAspect > viewAspect) {
                qreal newW = m_viewRect.height() * targetAspect;
                qreal d = (newW - m_viewRect.width()) / 2.0;
                m_viewRect.adjust(-d, 0.0, d, 0.0);
            } else {
                qreal newH = m_viewRect.width() / targetAspect;
                qreal d = (newH - m_viewRect.height()) / 2.0;
                m_viewRect.adjust(0.0, -d, 0.0, d);
            }
            break;
        }
    } else {
        // Crop：收缩较大维度
        if (viewAspect > targetAspect) {
            // 太宽 → 收缩宽度
            qreal newW = m_viewRect.height() * targetAspect;
            qreal d = (m_viewRect.width() - newW) / 2.0;
            m_viewRect.adjust(d, 0.0, -d, 0.0);
        } else {
            // 太高 → 收缩高度
            qreal newH = m_viewRect.width() / targetAspect;
            qreal d = (m_viewRect.height() - newH) / 2.0;
            m_viewRect.adjust(0.0, d, 0.0, -d);
        }
    }

    // 反算可见数据范围
    m_dataBounds = m_projection->computeDataBounds(m_viewRect);

    qCDebug(logWidget) << "fitViewRectToPlotArea: after" << m_viewRect
                       << "dataBounds=" << m_dataBounds;
}

// ===== 坐标转换（对所有投影类型通用）=====
QPointF QChartWidget::cartesianToPixel(qreal cx, qreal cy) const {
    // View Cartesian → ViewNorm → Pixel（线性）
	// c就是Cartesian；n就是Normalized；p就是Pixel
    qreal nx = (cx - m_viewRect.left()) / m_viewRect.width();
    qreal ny = (cy - m_viewRect.top())  / m_viewRect.height();
    qreal px = m_plotArea.left() + nx * m_plotArea.width();
    qreal py = m_plotArea.bottom() - ny * m_plotArea.height();

    static bool first = true;
    if (first) {
        qCDebug(logRender) << "cartesianToPixel: (cx,cy)=(" << cx << "," << cy
            << ") → (nx,ny)=(" << nx << "," << ny
            << ") → (px,py)=(" << px << "," << py << ")";
        first = false;
    }

    return QPointF(px, py);
}

QPointF QChartWidget::pixelToCartesian(const QPointF& pixel) const {
    // Pixel → ViewNorm → View Cartesian（逆线性）
    qreal nx = (pixel.x() - m_plotArea.left()) / m_plotArea.width();
    qreal ny = (m_plotArea.bottom() - pixel.y()) / m_plotArea.height();
    return QPointF(
        m_viewRect.left() + nx * m_viewRect.width(),
        m_viewRect.top()  + ny * m_viewRect.height()
    );
}

// ===== 视窗操作 =====
// ===== 绝对设置 viewRect（相机动画等场景）=====
// 按解耦哲学：viewRect 是数据窗口（相机状态），这里"设置什么就是什么"，
// 不做 fit 修正——长宽比由调用者负责（QViewRectAnimation 内部用 plotArea
// 快照保证）。fit 只在数据范围/投影变更时发生
void QChartWidget::setViewRect(const QRectF& r) {
    m_viewRect = r;
    if (m_projection)
        m_dataBounds = m_projection->computeDataBounds(m_viewRect);
    qCDebug(logWidget) << "setViewRect:" << r << "→ viewRect=" << m_viewRect
                       << "dataBounds=" << m_dataBounds;
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

void QChartWidget::panViewCartesian(qreal dx, qreal dy) {
    m_viewRect.translate(dx, dy);
    // 重算 dataBounds
    if (m_projection)
        m_dataBounds = m_projection->computeDataBounds(m_viewRect);
    qCDebug(logWidget) << "panViewCartesian: dx=" << dx << "dy=" << dy
                       << "→ viewRect=" << m_viewRect
                       << "dataBounds=" << m_dataBounds;
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

void QChartWidget::zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY) {
    if (factorX <= 0.0 || factorY <= 0.0 || !m_projection) return;
    // 以 (cx, cy) 为中心缩放 viewRect，两维独立（禁交互维度传 1.0）
    qreal newW = m_viewRect.width()  * factorX;
    qreal newH = m_viewRect.height() * factorY;
    qreal newLeft = cx - (cx - m_viewRect.left()) * factorX;
    qreal newTop  = cy - (cy - m_viewRect.top())  * factorY;
    m_viewRect = QRectF(newLeft, newTop, newW, newH);
    m_dataBounds = m_projection->computeDataBounds(m_viewRect);
    fitViewRectToPlotArea(FitStrategy::KeepCenter);
    qCDebug(logWidget) << "zoomViewCartesian: factorX=" << factorX
                       << "factorY=" << factorY
                       << "→ viewRect=" << m_viewRect
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
        m_viewRect = m_projection->computeViewRect(m_dataBounds);
        qCDebug(logWidget) << "setDataRangeDim0:" << min << "→" << max
                           << "viewRect=" << m_viewRect;
        fitViewRectToPlotArea(FitStrategy::KeepWidth);
    }
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

void QChartWidget::setDataRangeDim1(qreal min, qreal max) {
    m_dataBounds.setTop(min);
    m_dataBounds.setHeight(max - min);
    if (m_projection) {
        m_viewRect = m_projection->computeViewRect(m_dataBounds);
        qCDebug(logWidget) << "setDataRangeDim1:" << min << "→" << max
                           << "viewRect=" << m_viewRect;
        fitViewRectToPlotArea(FitStrategy::KeepHeight);
    }
    invalidateBackground();
    invalidateForeground();
    emit viewChanged();
}

void QChartWidget::setProjection(std::unique_ptr<QChartProjection> proj) {
    m_projection = std::move(proj);
    if (m_projection && !m_viewInitialized) {
        m_dataBounds = m_projection->defaultDataBounds();
        m_viewRect = m_projection->computeViewRect(m_dataBounds);
        m_viewInitialized = true;
        qCDebug(logWidget) << "setProjection: viewRect initialized from defaultDataBounds:"
                           << m_viewRect;
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

    m_plotArea = QRectF(left, top,
                        width() - left - right,
                        height() - top - bottom);
    qCDebug(logWidget) << "layoutAxes: plotArea=" << m_plotArea;
    // 注意：resize 只更新 plotArea，不动 viewRect。
    // viewRect 是数据窗口（相机状态），plotArea 是像素窗口——两者解耦：
    // 拉伸窗口 = 变相调整视图（像素映射拉伸），fit 只在数据范围/投影
    // 显式变更时发生（setDataRange/setProjection/setViewRectFitMode）。
    // 若 resize 也去 fit，会从"已收缩的当前值"反复收缩 → 累积漂移
}

// ===== 缓存控制 =====
void QChartWidget::invalidateBackground() { m_bgDirty = true; update(); }
void QChartWidget::invalidateForeground() { m_fgDirty = true; update(); }
void QChartWidget::invalidateLayout()      { m_layoutDirty = true; update(); }

// ===== 事件 =====
void QChartWidget::resizeEvent(QResizeEvent*) {
    m_bgDirty = m_fgDirty = true;
    layoutAxes();
}

void QChartWidget::paintEvent(QPaintEvent*) {
    if (m_layoutDirty) {
        layoutAxes();
        m_layoutDirty = false;
        m_bgDirty = true;
        m_fgDirty = true;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!m_cachingEnabled) {
        drawBackground(&p);
        drawForeground(&p);
        return;
    }

    qreal dpr = devicePixelRatioF();
    QSize sz = size() * dpr;

    if (m_bgDirty || m_bgCache.isNull()) {
        m_bgCache = QPixmap(sz);
        m_bgCache.setDevicePixelRatio(dpr);
        m_bgCache.fill(Qt::transparent);
        QPainter bg(&m_bgCache);
        bg.setRenderHint(QPainter::Antialiasing, true);
        drawBackground(&bg);
        m_bgDirty = false;
    }
    p.drawPixmap(0, 0, m_bgCache);

    if (m_fgDirty || m_fgCache.isNull()) {
        m_fgCache = QPixmap(sz);
        m_fgCache.setDevicePixelRatio(dpr);
        m_fgCache.fill(Qt::transparent);
        QPainter fg(&m_fgCache);
        fg.setRenderHint(QPainter::Antialiasing, true);
        drawForeground(&fg);
        m_fgDirty = false;
    }
    p.drawPixmap(0, 0, m_fgCache);
}

// ===== drawBackground =====
void QChartWidget::drawBackground(QPainter* p) {
    p->fillRect(m_plotArea, Qt::transparent);
    QFont f = p->font();
    f.setPointSize(f.pointSize() - 1);
    p->setFont(f);

    // 构建 DrawContext —— 所有 draw 调用共用
    DrawContext ctx;
    ctx.plotArea   = m_plotArea;
    ctx.dataBounds = m_dataBounds;
    ctx.viewRect   = m_viewRect;
    ctx.projection = m_tempProjection ? m_tempProjection : m_projection.get();

    qCDebug(logRender) << "drawBackground: plotArea=" << m_plotArea
        << "viewRect=" << m_viewRect
        << "dataBounds=" << m_dataBounds
        << "projection type=" << (m_projection ? (int)m_projection->type() : -1);

    // ── 绘制所有轴 ──
    for (auto* a : m_axes) {
        if (!a || !a->isVisible()) continue;

        qCDebug(logRender) << "drawBackground: drawing axis alignment=" << a->alignment()
            << "color=" << a->color()
            << "isInterior=" << (a->alignment() == Qt::AlignHCenter || a->alignment() == Qt::AlignVCenter);

        bool isInterior = (a->alignment() == Qt::AlignHCenter
                        || a->alignment() == Qt::AlignVCenter);
        if (isInterior) {
            // 数据主脊：画在 offset = 0 的位置（通过 dataBounds 确定默认位置）
            // offset=0 意味着画在 Numeric dim0=0 或 dim1=0 的等值线上
            // 对于 Cartesian，这条线通过 plotArea；Polar 下它通过原点
            qreal defaultOffset = 0.0;
            if (a->alignment() == Qt::AlignHCenter)
                defaultOffset = ctx.dataBounds.top();  // Y 维度在默认位置
            else
                defaultOffset = ctx.dataBounds.left(); // X 维度在默认位置

            QString nullLabel = "";
            p->save();
            p->setClipRect(m_plotArea);
            a->drawAtPosition(p, ctx, defaultOffset,
                              /*axisLine=*/true, /*labels=*/false, /*ticks=*/true, /*label=*/nullLabel, /*pen=*/nullptr);
            p->restore();
        } else {
            // 边框轴：画在 plotArea 边缘
            a->drawAtEdge(p, ctx,
                          /*axisLine=*/true, /*labels=*/true, /*ticks=*/true);
        }
    }

    // ── 绘制网格（取最后一个几何体）──
    if (!m_layers.isEmpty()) {
        auto* geo = m_layers.last();
        p->save();
        p->setClipRect(m_plotArea);
        geo->drawGrid(p, ctx);
        p->restore();
    }

    // ── 调试：黄色 plotArea 边框 ──
    if (logWidget().isDebugEnabled()) {
        p->save();
        p->setPen(Qt::yellow);
        p->drawRect(m_plotArea);
        p->restore();
    }
}

// ===== drawForeground =====
void QChartWidget::drawForeground(QPainter* p) {
    DrawContext ctx;
    ctx.plotArea   = m_plotArea;
    ctx.dataBounds = m_dataBounds;
    ctx.viewRect   = m_viewRect;
    ctx.projection = m_tempProjection ? m_tempProjection : m_projection.get();

    for (auto* g : m_layers) {
        p->save();
        p->setClipRect(m_plotArea);
        g->drawAllSeries(p, ctx);
        p->restore();
    }
}

// ===== 鼠标事件 =====
void QChartWidget::mousePressEvent(QMouseEvent* e) {
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
    ctx.viewRect   = m_viewRect;
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
