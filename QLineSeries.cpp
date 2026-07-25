#include "QLineSeries.h"
#include "QChartGeometry.h"
#include <QDebug>
#include <QtMath>
#include <cmath>

QLineSeries::QLineSeries(const QString& name, QObject* parent)
    : QAbstractSeries(name, parent)
{
    m_color = QColor("#2196F3");
}

void QLineSeries::append(qreal x, qreal y) {
    m_points.append({x, y});
    emit dataChanged();
}

void QLineSeries::append(const QPointF& p) { append(p.x(), p.y()); }

void QLineSeries::clear() { m_points.clear(); emit dataChanged(); }

QPointF QLineSeries::at(int i) const {
    return (i >= 0 && i < m_points.size()) ? m_points[i] : QPointF();
}

void QLineSeries::setPoints(const QVector<QPointF>& pts) {
    m_points = pts;
    emit dataChanged();
}

QRectF QLineSeries::boundingRect() const {
    if (m_points.isEmpty()) return {};
    qreal xMin = m_points[0].x(), xMax = xMin, yMin = m_points[0].y(), yMax = yMin;
    for (auto& p : m_points) {
        xMin = qMin(xMin, p.x()); xMax = qMax(xMax, p.x());
        yMin = qMin(yMin, p.y()); yMax = qMax(yMax, p.y());
    }
    return {xMin, yMin, xMax - xMin, yMax - yMin};
}




// Catmull-Rom → cubic Bezier
QPainterPath QLineSeries::smoothPath(const QVector<QPointF>& pts) const {
    QPainterPath path;
    int n = pts.size();
    if (n < 2) return path;
    if (n == 2) { path.moveTo(pts[0]); path.lineTo(pts[1]); return path; }
    path.moveTo(pts[0]);
    for (int i = 0; i < n - 1; ++i) {
        QPointF p0 = (i > 0) ? pts[i-1] : pts[i];
        QPointF p1 = pts[i], p2 = pts[i+1];
        QPointF p3 = (i < n - 2) ? pts[i+2] : pts[i+1];
        QPointF cp1 = p1 + (p2 - p0) / 6.0;
        QPointF cp2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(cp1, cp2, p2);
    }
    return path;
}

QLineSeries* QLineSeries::sinusoidal(const QString& name, int n, qreal amp, qreal freq, qreal phase, QObject* parent) {
    auto* s = new QLineSeries(name, parent);
    for (int i = 0; i < n; ++i) {
        qreal x = qreal(i) / (n-1) * 10.0;
        qreal y = amp * std::sin(freq * x + phase);
        s->append(x, y);
    }
    return s;
}
