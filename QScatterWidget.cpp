#include "QScatterWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDebug>
#include <QtMath>
#include <cmath>

QScatterWidget::QScatterWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(200, 150);
}

QScatterWidget::~QScatterWidget() { qDeleteAll(m_series); }

void QScatterWidget::addSeries(QXYSeries* s) {
    if (!s) return;
    s->setParent(this);
    m_series.append(s);
    connect(s, &QXYSeries::pointsChanged, this, [this]() { update(); });
    connect(s, &QXYSeries::colorChanged, this, [this](const QColor&) { update(); });
    qDebug() << "[QScatterWidget] addSeries:" << s->name() << "count=" << s->count();
    update();
}

void QScatterWidget::removeSeries(QXYSeries* s) {
    m_series.removeAll(s);
    delete s;
    update();
}

void QScatterWidget::clear() {
    qDeleteAll(m_series);
    m_series.clear();
    update();
}

void QScatterWidget::setAxisX(QChartAxis* a) { m_axisX = a ? a : &m_defaultAxisX; update(); }
void QScatterWidget::setAxisY(QChartAxis* a) { m_axisY = a ? a : &m_defaultAxisY; update(); }
void QScatterWidget::setValuesMultiplier(qreal v) {
    v = qBound(0.0, v, 1.0);
    if (!qFuzzyCompare(m_valuesMultiplier, v)) { m_valuesMultiplier = v; update(); }
}

QRectF QScatterWidget::plotArea() const {
    return QRectF(kMarginL, kMarginT, width() - kMarginL - kMarginR,
                  height() - kMarginT - kMarginB);
}

QColor QScatterWidget::borderColorFor(const QColor& fill) const {
    return fill.darker(140);
}

// ========== 绘制 ==========
void QScatterWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QRectF area = plotArea();

    p.fillRect(rect(), QColor("#FAFAFA"));
    p.fillRect(area, Qt::white);

    drawAxes(&p, area);
    drawMarkers(&p, area);
}

void QScatterWidget::drawAxes(QPainter* p, const QRectF& area) {
    p->save();
    p->setPen(QPen(Qt::black, 1));
    QFont f = p->font(); f.setPointSize(f.pointSize() - 1); p->setFont(f);

    qreal w = area.width(), h = area.height();

    // Y axis grid + labels
    if (m_axisY->isGridVisible()) {
        p->setPen(QPen(m_axisY->gridColor(), 1, Qt::DashLine));
        for (qreal t : m_axisY->tickValues()) {
            qreal y = area.bottom() - m_axisY->mapToPixel(t, h);
            p->drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        }
    }
    p->setPen(Qt::black);
    QVector<qreal> yTicks = m_axisY->tickValues();
    QStringList yLabels = m_axisY->tickLabels();
    for (int i = 0; i < yTicks.size() && i < yLabels.size(); ++i) {
        qreal y = area.bottom() - m_axisY->mapToPixel(yTicks[i], h);
        p->drawText(QRectF(area.left() - 45, y - 10, 40, 20),
                    Qt::AlignRight | Qt::AlignVCenter, yLabels[i]);
        p->drawLine(QPointF(area.left() - 2, y), QPointF(area.left(), y));
    }

    // X axis grid + labels
    if (m_axisX->isGridVisible()) {
        p->setPen(QPen(m_axisX->gridColor(), 1, Qt::DashLine));
        for (qreal t : m_axisX->tickValues()) {
            qreal x = area.left() + m_axisX->mapToPixel(t, w);
            p->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        }
    }
    p->setPen(Qt::black);
    QVector<qreal> xTicks = m_axisX->tickValues();
    QStringList xLabels = m_axisX->tickLabels();
    for (int i = 0; i < xTicks.size() && i < xLabels.size(); ++i) {
        qreal x = area.left() + m_axisX->mapToPixel(xTicks[i], w);
        p->drawText(QRectF(x - 20, area.bottom() + 2, 40, 20),
                    Qt::AlignHCenter | Qt::AlignTop, xLabels[i]);
        p->drawLine(QPointF(x, area.bottom()), QPointF(x, area.bottom() + 2));
    }

    // Axes lines
    p->setPen(QPen(Qt::black, 1));
    p->drawLine(area.topLeft(), area.bottomLeft());
    p->drawLine(area.bottomLeft(), area.bottomRight());
    p->restore();
}

void QScatterWidget::drawSingleMarker(QPainter* p, const QPointF& c, qreal size,
                                       MarkerShape shape, const QColor& fill, const QColor& border) {
    QPainterPath path;
    qreal r = size / 2;
    switch (shape) {
    case Circle:
        path.addEllipse(c, r, r); break;
    case Rectangle:
        path.addRect(c.x() - r, c.y() - r, size, size); break;
    case Triangle:
        path.moveTo(c.x(), c.y() - r);
        path.lineTo(c.x() + r, c.y() + r);
        path.lineTo(c.x() - r, c.y() + r); path.closeSubpath(); break;
    case Diamond:
        path.moveTo(c.x(), c.y() - r);
        path.lineTo(c.x() + r, c.y());
        path.lineTo(c.x(), c.y() + r);
        path.lineTo(c.x() - r, c.y()); path.closeSubpath(); break;
    case Star: {
        qreal outerR = r, innerR = r * 0.4;
        for (int i = 0; i < 10; ++i) {
            qreal a = -M_PI / 2 + i * M_PI / 5;
            qreal rr = (i % 2 == 0) ? outerR : innerR;
            QPointF pt(c.x() + rr * std::cos(a), c.y() + rr * std::sin(a));
            if (i == 0) path.moveTo(pt); else path.lineTo(pt);
        }
        path.closeSubpath(); break;
    }
    case Cross:
        path.moveTo(c.x() - r * 0.6, c.y() - r);
        path.lineTo(c.x() + r * 0.6, c.y() + r);
        path.moveTo(c.x() + r * 0.6, c.y() - r);
        path.lineTo(c.x() - r * 0.6, c.y() + r); break;
    }
    p->setPen(QPen(border, 1));
    p->setBrush(fill);
    p->drawPath(path);
}

void QScatterWidget::drawMarkers(QPainter* p, const QRectF& area) {
    qreal w = area.width(), h = area.height();
    for (int si = 0; si < m_series.size(); ++si) {
        QXYSeries* s = m_series.at(si);
        QColor fill = s->color();
        // 悬停高亮
        if (si == m_hoverSer) fill = fill.lighter(130);
        QColor border = borderColorFor(fill);

        for (int pi = 0; pi < s->count(); ++pi) {
            QXYPoint pt = s->at(pi);
            qreal x = area.left() + m_axisX->mapToPixel(pt.x, w);
            qreal yVal = pt.y * m_valuesMultiplier;
            qreal y = area.bottom() - m_axisY->mapToPixel(yVal, h);

            // 裁剪
            if (x < area.left() - m_markerSize || x > area.right() + m_markerSize ||
                y < area.top() - m_markerSize || y > area.bottom() + m_markerSize)
                continue;

            qreal sz = (si == m_hoverSer && pi == m_hoverPt) ? m_markerSize * 1.5 : m_markerSize;
            drawSingleMarker(p, QPointF(x, y), sz, m_markerShape, fill, border);
        }
    }
}

// ========== 交互 ==========
QPair<int,int> QScatterWidget::pointAtPos(const QPointF& pos) const {
    QRectF area = plotArea();
    if (!area.contains(pos)) return {-1, -1};
    qreal w = area.width(), h = area.height();
    qreal hitDist = m_markerSize * 2;

    for (int si = m_series.size() - 1; si >= 0; --si) {
        QXYSeries* s = m_series.at(si);
        for (int pi = 0; pi < s->count(); ++pi) {
            QXYPoint pt = s->at(pi);
            qreal px = area.left() + m_axisX->mapToPixel(pt.x, w);
            qreal py = area.bottom() - m_axisY->mapToPixel(pt.y * m_valuesMultiplier, h);
            if (std::hypot(px - pos.x(), py - pos.y()) < hitDist)
                return {si, pi};
        }
    }
    return {-1, -1};
}

void QScatterWidget::mouseMoveEvent(QMouseEvent* e) {
    auto [si, pi] = pointAtPos(e->pos());
    if (si != m_hoverSer || pi != m_hoverPt) {
        if (m_hoverSer >= 0) emit pointHovered(m_hoverSer, m_hoverPt, false);
        m_hoverSer = si; m_hoverPt = pi;
        if (si >= 0) emit pointHovered(si, pi, true);
        setCursor(si >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(e);
}

void QScatterWidget::mousePressEvent(QMouseEvent* e) {
    auto [si, pi] = pointAtPos(e->pos());
    m_pressSer = si; m_pressPt = pi;
    QWidget::mousePressEvent(e);
}

void QScatterWidget::mouseReleaseEvent(QMouseEvent* e) {
    auto [si, pi] = pointAtPos(e->pos());
    if (si >= 0 && si == m_pressSer && pi == m_pressPt) {
        emit pointClicked(si, pi);
        qDebug() << "[QScatterWidget] pointClicked:" << si << pi;
    }
    m_pressSer = -1; m_pressPt = -1;
    QWidget::mouseReleaseEvent(e);
}

void QScatterWidget::leaveEvent(QEvent*) {
    if (m_hoverSer >= 0) {
        emit pointHovered(m_hoverSer, m_hoverPt, false);
        m_hoverSer = -1; m_hoverPt = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }
}
