#include "QLineWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDebug>
#include <QtMath>
#include <cmath>

QLineWidget::QLineWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(200, 150);
}

QLineWidget::~QLineWidget() { qDeleteAll(m_series); }

void QLineWidget::addSeries(QXYSeries* s) {
    if (!s) return;
    s->setParent(this);
    m_series.append(s);
    connect(s, &QXYSeries::pointsChanged, this, [this]() { update(); });
    connect(s, &QXYSeries::colorChanged, this, [this](const QColor&) { update(); });
    qDebug() << "[QLineWidget] addSeries:" << s->name() << "count=" << s->count();
    update();
}

void QLineWidget::removeSeries(QXYSeries* s) { m_series.removeAll(s); delete s; update(); }
void QLineWidget::clear() { qDeleteAll(m_series); m_series.clear(); update(); }
void QLineWidget::setAxisX(QChartAxis* a) { m_axisX = a ? a : &m_defaultAxisX; update(); }
void QLineWidget::setAxisY(QChartAxis* a) { m_axisY = a ? a : &m_defaultAxisY; update(); }
void QLineWidget::setValuesMultiplier(qreal v) {
    v = qBound(0.0, v, 1.0);
    if (!qFuzzyCompare(m_valuesMultiplier, v)) { m_valuesMultiplier = v; update(); }
}

QRectF QLineWidget::plotArea() const {
    return QRectF(kMarginL, kMarginT, width() - kMarginL - kMarginR,
                  height() - kMarginT - kMarginB);
}

// ========== 平滑曲线（Catmull-Rom → Bezier） ==========
QPainterPath QLineWidget::smoothPath(const QVector<QPointF>& pts) const {
    QPainterPath path;
    int n = pts.size();
    if (n < 2) return path;
    if (n == 2) { path.moveTo(pts[0]); path.lineTo(pts[1]); return path; }

    path.moveTo(pts[0]);
    // Catmull-Rom → cubic Bezier: control points at 1/6 of chord length
    for (int i = 0; i < n - 1; ++i) {
        QPointF p0 = (i > 0) ? pts[i-1] : pts[i];
        QPointF p1 = pts[i];
        QPointF p2 = pts[i+1];
        QPointF p3 = (i < n - 2) ? pts[i+2] : pts[i+1];

        QPointF cp1 = p1 + (p2 - p0) / 6.0;
        QPointF cp2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(cp1, cp2, p2);
    }
    return path;
}

// ========== 绘制 ==========
void QLineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QRectF area = plotArea();

    p.fillRect(rect(), QColor("#FAFAFA"));
    p.fillRect(area, Qt::white);

    drawAxes(&p, area);
    drawLines(&p, area);
    if (m_pointsVisible) drawPoints(&p, area);
}

void QLineWidget::drawAxes(QPainter* p, const QRectF& area) {
    p->save();
    QFont f = p->font(); f.setPointSize(f.pointSize() - 1); p->setFont(f);
    qreal w = area.width(), h = area.height();

    // Y axis
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

    // X axis
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
    }

    p->setPen(QPen(Qt::black, 1));
    p->drawLine(area.topLeft(), area.bottomLeft());
    p->drawLine(area.bottomLeft(), area.bottomRight());
    p->restore();
}

void QLineWidget::drawLines(QPainter* p, const QRectF& area) {
    qreal w = area.width(), h = area.height();
    for (int si = 0; si < m_series.size(); ++si) {
        QXYSeries* s = m_series.at(si);
        if (s->count() < 2) continue;

        QVector<QPointF> screenPts;
        screenPts.reserve(s->count());
        for (int i = 0; i < s->count(); ++i) {
            QXYPoint pt = s->at(i);
            qreal x = area.left() + m_axisX->mapToPixel(pt.x, w);
            qreal y = area.bottom() - m_axisY->mapToPixel(pt.y * m_valuesMultiplier, h);
            screenPts.append(QPointF(x, y));
        }

        p->save();
        QColor lineColor = s->color();
        if (si == m_hoverSer) lineColor = lineColor.lighter(120);
        p->setPen(QPen(lineColor, m_lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        if (m_smooth) {
            QPainterPath sp = smoothPath(screenPts);
            p->setBrush(Qt::NoBrush);
            p->drawPath(sp);
        } else {
            QPainterPath path;
            path.moveTo(screenPts.first());
            for (int i = 1; i < screenPts.size(); ++i)
                path.lineTo(screenPts[i]);
            p->drawPath(path);
        }
        p->restore();
    }
}

void QLineWidget::drawPoints(QPainter* p, const QRectF& area) {
    qreal w = area.width(), h = area.height();
    for (int si = 0; si < m_series.size(); ++si) {
        QXYSeries* s = m_series.at(si);
        QColor fill = s->color();
        if (si == m_hoverSer) fill = fill.lighter(130);
        QColor border = fill.darker(140);

        for (int pi = 0; pi < s->count(); ++pi) {
            QXYPoint pt = s->at(pi);
            qreal x = area.left() + m_axisX->mapToPixel(pt.x, w);
            qreal y = area.bottom() - m_axisY->mapToPixel(pt.y * m_valuesMultiplier, h);
            qreal sz = (si == m_hoverSer && pi == m_hoverPt) ? m_pointMarkerSize * 1.5 : m_pointMarkerSize;

            p->setPen(QPen(border, 1));
            p->setBrush(fill);
            p->drawEllipse(QPointF(x, y), sz / 2, sz / 2);
        }
    }
}

// ========== 交互 ==========
QPair<int,int> QLineWidget::pointAtPos(const QPointF& pos) const {
    QRectF area = plotArea();
    if (!area.contains(pos)) return {-1, -1};
    qreal w = area.width(), h = area.height();
    qreal hitDist = qMax(m_pointMarkerSize * 2, 12.0);

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

void QLineWidget::mouseMoveEvent(QMouseEvent* e) {
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

void QLineWidget::mousePressEvent(QMouseEvent* e) {
    auto [si, pi] = pointAtPos(e->pos());
    m_pressSer = si; m_pressPt = pi;
    QWidget::mousePressEvent(e);
}

void QLineWidget::mouseReleaseEvent(QMouseEvent* e) {
    auto [si, pi] = pointAtPos(e->pos());
    if (si >= 0 && si == m_pressSer && pi == m_pressPt) {
        emit pointClicked(si, pi);
        qDebug() << "[QLineWidget] pointClicked:" << si << pi;
    }
    m_pressSer = -1; m_pressPt = -1;
    QWidget::mouseReleaseEvent(e);
}

void QLineWidget::leaveEvent(QEvent*) {
    if (m_hoverSer >= 0) {
        emit pointHovered(m_hoverSer, m_hoverPt, false);
        m_hoverSer = -1; m_hoverPt = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }
}
