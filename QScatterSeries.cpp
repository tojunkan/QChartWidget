// QScatterSeries.cpp —— 散点系列实现
#include "QScatterSeries.h"
#include "QChartDebug.h"
#include <QDebug>
#include <QtMath>
#include <cmath>

QScatterSeries::QScatterSeries(const QString& name, QObject* parent)
    : QXYSeries(name, parent) {
    // 默认给个显眼颜色
    m_color = QColor("#F44336");
}

// ===== 绘制 =====
void QScatterSeries::draw(QPainter* painter,
                          std::function<QPointF(QVariant,QVariant)> toPixel) const {
    if (!painter || !toPixel || !m_visible) return;

    int drawn = 0, skipped = 0;
    for (const auto& pt : m_points) {
        QPointF pixel = toPixel(pt.x(), pt.y());
        if (!std::isfinite(pixel.x()) || !std::isfinite(pixel.y())) {
            skipped++;
            continue;
        }
        drawMarker(painter, pixel);
        drawn++;
    }
    qCDebug(logSeriesVerbose) << "QScatterSeries::draw:" << drawn << "drawn,"
                              << skipped << "skipped (NaN)";
}

// ===== 命中检测：像素到最近数据点距离 < marker 半径 =====
int QScatterSeries::hitTest(const QPointF& pixel,
                            std::function<QPointF(QVariant,QVariant)> toPixel) const {
    if (!toPixel || !m_visible) return -1;

    qreal threshold = m_markerSize * 1.5;   // 容差：marker 尺寸的 1.5 倍
    int bestIndex = -1;
    qreal bestDist = threshold;

    for (int i = 0; i < m_points.size(); ++i) {
        QPointF p = toPixel(m_points[i].x(), m_points[i].y());
        if (!std::isfinite(p.x()) || !std::isfinite(p.y())) continue;
        qreal d = std::hypot(p.x() - pixel.x(), p.y() - pixel.y());
        if (d < bestDist) {
            bestDist = d;
            bestIndex = i;
        }
    }
    return bestIndex;
}

// ===== marker 绘制 =====
void QScatterSeries::drawMarker(QPainter* p, const QPointF& pos) const {
    qreal r = m_markerSize / 2.0;

    // 填充 + 描边
    p->save();
    QColor fill = (m_fillColor.alpha() == 0) ? m_color : m_fillColor;
    QBrush brush(fill);
    if (m_pen.style() != Qt::NoPen)
        brush = QBrush(fill);  // 有描边时也填充
    p->setBrush(brush);
    p->setPen(m_pen);

    switch (m_markerShape) {
    case MarkerShape::Circle: {
        p->drawEllipse(pos, r, r);
        break;
    }
    case MarkerShape::Square: {
        p->drawRect(QRectF(pos.x() - r, pos.y() - r, m_markerSize, m_markerSize));
        break;
    }
    case MarkerShape::Triangle: {
        QPolygonF tri;
        tri << QPointF(pos.x(), pos.y() - r)
            << QPointF(pos.x() - r, pos.y() + r)
            << QPointF(pos.x() + r, pos.y() + r);
        p->drawPolygon(tri);
        break;
    }
    case MarkerShape::Diamond: {
        QPolygonF dia;
        dia << QPointF(pos.x(), pos.y() - r)
            << QPointF(pos.x() + r, pos.y())
            << QPointF(pos.x(), pos.y() + r)
            << QPointF(pos.x() - r, pos.y());
        p->drawPolygon(dia);
        break;
    }
    case MarkerShape::Plus: {
        // 加号：无填充，画两条线
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(pos.x() - r, pos.y()), QPointF(pos.x() + r, pos.y()));
        p->drawLine(QPointF(pos.x(), pos.y() - r), QPointF(pos.x(), pos.y() + r));
        break;
    }
    case MarkerShape::Cross: {
        // 叉号：无填充，画两条对角线
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(pos.x() - r, pos.y() - r), QPointF(pos.x() + r, pos.y() + r));
        p->drawLine(QPointF(pos.x() + r, pos.y() - r), QPointF(pos.x() - r, pos.y() + r));
        break;
    }
    }
    p->restore();
}
