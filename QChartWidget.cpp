#include "QChartWidget.h"
#include "QChartSeries.h"
#include "QChartProjection.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>

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
    a->setParent(this);
    m_axes.append(a);
    // 连接范围变化信号，刷新背景和前景
    connect(a, &QChartAxis::rangeChanged,
        this, [this]() {
            invalidateBackground();
            invalidateForeground();
        });
    invalidateBackground();
}

void QChartWidget::removeAxis(QChartAxis* a) {
    m_axes.removeAll(a);
    delete a;
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

void QChartWidget::layoutAxes() {
    // 简化布局：硬编码边距，子类可重写
    qreal left = 50, right = 20, top = 20, bottom = 40;
    // TODO: 根据 axis sizeHint 动态计算
    m_plotArea = QRectF(left, top,
        width() - left - right,
        height() - top - bottom);
}

void QChartWidget::resizeEvent(QResizeEvent*) {
    m_bgDirty = m_fgDirty = true;
    layoutAxes();
}

void QChartWidget::paintEvent(QPaintEvent*) {
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
            a->draw(p, m_plotArea);
    }
    // 绘制网格（取最后一个几何体，或遍历全部）
    if (!m_geometries.isEmpty())
        m_geometries.last()->drawGrid(p);
}

void QChartWidget::drawForeground(QPainter* p) {
    for (auto* g : m_geometries) {
        p->save();
        p->setClipRect(m_plotArea);
        g->drawAllSeries(p);
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
        QPointF normStart = QCartesianProjection::mapToNormalized(m_panStart, m_plotArea);
        QPointF normCurrent = QCartesianProjection::mapToNormalized(currentPos, m_plotArea);

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
    if (!m_zoomEnabled) return;
    QPointF pos = e->position();
    if (!m_plotArea.contains(pos)) return;

    // 将鼠标位置转为归一化坐标
    QPointF normPos = QCartesianProjection::mapToNormalized(pos, m_plotArea);

    qreal factor = (e->angleDelta().y() > 0) ? 0.9 : 1.1;

    for (auto* axis : m_axes) {
        if (!axis->isZoomEnabled()) continue;
        Qt::Alignment align = axis->alignment();
        if (align == Qt::AlignTop || align == Qt::AlignBottom) {
            axis->zoom(normPos.x(), factor);
        }
        else if (align == Qt::AlignLeft || align == Qt::AlignRight) {
            axis->zoom(normPos.y(), factor); // 同样，Y轴已反转
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