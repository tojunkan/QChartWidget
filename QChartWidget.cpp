#include "QChartWidget.h"
#include "QChartSeries.h"
#include "QChartProjection.h"
#include "QChartProjectionFactory.h"
#include "QChartDebug.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logAxis, "chart.axis")
Q_LOGGING_CATEGORY(logWidget, "chart.widget")
Q_LOGGING_CATEGORY(logGeometry, "chart.geometry")
Q_LOGGING_CATEGORY(logSeries, "chart.series")


QChartWidget::QChartWidget(QWidget* p) : QWidget(p) {
    setMouseTracking(true);
    setMinimumSize(200, 150);
}

QChartWidget::~QChartWidget() {
    qDeleteAll(m_geometries);
    qDeleteAll(m_axes);
}

void QChartWidget::addGeometry(QChartGeometry* g) {
    if (!g) return;
    if (m_projection == nullptr) {
        m_projection = QChartProjectionFactory::create(g->coordinateSystem());
    }
    else if (m_projection->type() != g->coordinateSystem()) {
        qWarning() << "Projection mismatched with the Geometry.";
        return;
    }
    g->setParent(this);
    m_geometries.append(g);
    // 如果已有轴，且几何体已绑定轴，无需额外操作（轴不再需要 setGeometry）
    invalidateForeground();
}

void QChartWidget::removeGeometry(QChartGeometry* g) {
    m_geometries.removeAll(g);
    delete g;
    invalidateForeground();
}

void QChartWidget::addAxis(QChartAxis* a) {
    if (!a) return;
    if (m_projection == nullptr) {
        m_projection = QChartProjectionFactory::create(a->coordinateSystem());
    }
    else if (m_projection->type() != a->coordinateSystem()) {
        qWarning() << "Projection mismatched with the Axis.";
        return;
    }
    a->setParent(this);
    m_axes.append(a);
    // 连接范围变化信号，刷新背景和前景
    connect(a, &QChartAxis::rangeChanged,
        this, [this]() {
            invalidateBackground();
            invalidateForeground();
        });
    // 连接范围变化信号到请求布局
    connect(a, &QChartAxis::rangeChanged, this, &QChartWidget::invalidateLayout);
    // 添加后立即请求一次布局
    invalidateLayout();
    invalidateBackground();
}

void QChartWidget::removeAxis(QChartAxis* a) {
    m_axes.removeAll(a);
    delete a;
    invalidateLayout();
    invalidateBackground();
}

void QChartWidget::invalidateBackground() {
    m_bgDirty = true;
    update();
}

void QChartWidget::invalidateForeground() {
    m_fgDirty = true;
    update();
}

void QChartWidget::invalidateLayout() {
    m_layoutDirty = true;
    update();
}

void QChartWidget::layoutAxes() {
    // 1. 先以默认边距为基准（或用户设置的值）
    qreal left = m_marginLeft;
    qreal top = m_marginTop;
    qreal right = m_marginRight;
    qreal bottom = m_marginBottom;

    // 2. 遍历所有轴，根据对齐方式累加 sizeHint
    QFont font = this->font(); // 使用当前字体
    for (auto* axis : m_axes) {
        if (!axis->isVisible()) continue;
        Qt::Alignment align = axis->alignment();
        QSizeF hint = axis->sizeHint(font);
        switch (align) {
        case Qt::AlignLeft:
            left = qMax(left, hint.width());
            break;
        case Qt::AlignRight:
            right = qMax(right, hint.width());
            break;
        case Qt::AlignTop:
            top = qMax(top, hint.height());
            break;
        case Qt::AlignBottom:
            bottom = qMax(bottom, hint.height());
            break;
        case Qt::AlignCenter:
            // 极轴不占外边距，忽略
            break;
        default:
            break;
        }
    }



    // 3. 计算绘图区
    m_plotArea = QRectF(
        left,
        top,
        width() - left - right,
        height() - top - bottom
    );
    qCDebug(logWidget) << "updated layout: " << m_plotArea;
}

void QChartWidget::resizeEvent(QResizeEvent*) {
    m_bgDirty = m_fgDirty = true;
    layoutAxes();
}

void QChartWidget::paintEvent(QPaintEvent*) {

    //在paintEvent处理是为了保证性能，如果每pan或者zoom一次就处理的话太过耗时了。
    if (m_layoutDirty) {
        layoutAxes();
        m_layoutDirty = false;
        // 布局变化必然导致背景和前景失效
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

void QChartWidget::drawBackground(QPainter* p) {
    p->fillRect(m_plotArea, Qt::transparent);
    QFont f = p->font();
    f.setPointSize(f.pointSize() - 1);
    p->setFont(f);

    for (auto* a : m_axes) {
        if (a && a->isVisible())
            a->draw(p, m_plotArea, m_projection.get());
    }
    // 绘制网格（取最后一个几何体，或遍历全部）
    if (!m_geometries.isEmpty())
        m_geometries.last()->drawGrid(p, m_projection.get());

    if (logWidget().isDebugEnabled()) {
        p->save();
        p->setPen(Qt::yellow);
        p->drawRect(plotArea());
        p->restore();
    }
}

void QChartWidget::drawForeground(QPainter* p) {
    for (auto* g : m_geometries) {
        p->save();
        p->setClipRect(m_plotArea);
        g->drawAllSeries(p, m_projection.get());
        p->restore();
    }
}

// ---- 鼠标事件 ----
void QChartWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_panEnabled) {
        m_panStart = e->pos();
        m_panning = true;
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(e);
}

void QChartWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_panning) {
        QPointF currentPos = e->pos();
        // 将像素坐标转为归一化坐标（0~1，Y轴向上）
        QPointF normStart = m_projection->mapToNormalized(m_panStart, m_plotArea);
        QPointF normCurrent = m_projection->mapToNormalized(currentPos, m_plotArea);

        // 计算归一化位移
        qreal deltaNormX = normCurrent.x() - normStart.x();
        qreal deltaNormY = normCurrent.y() - normStart.y(); // 注意：Y轴已反转，直接使用

        // 更新起始位置（归一化坐标，以便下次计算增量）
        m_panStart = currentPos; // 保存像素位置，或保存归一化位置都可以

        for (auto* axis : m_axes) {
            if (!axis->isDragEnabled()) continue;
            Qt::Alignment align = axis->alignment();
            if (align == Qt::AlignTop || align == Qt::AlignBottom) {
                axis->pan(deltaNormX);
            }
            else if (align == Qt::AlignLeft || align == Qt::AlignRight) {
                axis->pan(deltaNormY); // 因为 mapToNormalized 已经翻转了Y，所以这里直接用正号
            }
        }
        return;
    }

    // 悬停检测
    for (auto* g : m_geometries) {
        auto [s, idx] = g->hitTest(e->pos());
        if (s) {
            if (s != m_hoverSeries || idx != m_hoverIndex) {
                if (m_hoverSeries)
                    emit seriesHovered(m_hoverSeries, m_hoverIndex, false);
                m_hoverSeries = s;
                m_hoverIndex = idx;
                emit seriesHovered(s, idx, true);
                setCursor(Qt::PointingHandCursor);
            }
            return;
        }
    }
    // 未命中，清除悬停
    if (m_hoverSeries) {
        emit seriesHovered(m_hoverSeries, m_hoverIndex, false);
        m_hoverSeries = nullptr;
        m_hoverIndex = -1;
        setCursor(Qt::ArrowCursor);
    }
}

void QChartWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        // 可发射点击信号（可选）
        // 如果需要点击，可以在 mouseRelease 时判断是否没有拖动
    }
    QWidget::mouseReleaseEvent(e);
}

void QChartWidget::wheelEvent(QWheelEvent* e) {
    if (!m_zoomEnabled || !m_projection) return;
    QPointF pos = e->position();
    if (!m_plotArea.contains(pos)) return;

    // 获取滚动角度（通常 y 方向）
    int delta = e->angleDelta().y();
    if (delta == 0) return;

    // 将角度转换为缩放因子：滚动向上（正）放大，向下（负）缩小
    static constexpr qreal SCALE_SENSITIVITY = 0.1; // 可调参数
    qreal factor = std::exp(-delta * SCALE_SENSITIVITY / 120.0);
    // 限制范围防止过激
    if (factor < 0.8 || factor > 1.25)qWarning() << "too fast. Clamp.";
    factor = qBound(0.8, factor, 1.25); // 可选

    // 归一化坐标
    QPointF normPos = m_projection->mapToNormalized(pos, m_plotArea);

    // 🔥 调试输出
    qCDebug(logWidget) << "wheelEvent: pos =" << pos
        << "normPos =" << normPos
        << "plotArea =" << m_plotArea
        << "normPos =" << normPos
        << "factor =" << factor;

    for (auto* axis : m_axes) {
        if (!axis->isZoomEnabled()) continue;
        Qt::Alignment align = axis->alignment();
        if (align == Qt::AlignTop || align == Qt::AlignBottom) {
            axis->zoom(normPos.x(), factor);
        }
        else if (align == Qt::AlignLeft || align == Qt::AlignRight) {
            axis->zoom(normPos.y(), factor);
        }
        else if (align == Qt::AlignCenter) {
            // 极轴：固定中心缩放，中心归一化值为 0（原点是中心）
            axis->zoom(0.0, factor);
        }
    }
}

void QChartWidget::leaveEvent(QEvent*) {
    if (m_hoverSeries) {
        emit seriesHovered(m_hoverSeries, m_hoverIndex, false);
        m_hoverSeries = nullptr;
        m_hoverIndex = -1;
        setCursor(Qt::ArrowCursor);
    }
}