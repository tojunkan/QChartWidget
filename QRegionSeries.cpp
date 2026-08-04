// QRegionSeries.cpp —— 填充区域系列实现
#include "QRegionSeries.h"
#include "QChartDebug.h"
#include <QPainterPath>
#include <QDebug>
#include <cmath>

QRegionSeries::QRegionSeries(const QString& name, QObject* parent)
    : QXYSeries(name, parent) {}

// ===== 绘制：多边形 fill（隐式闭合）=====
void QRegionSeries::draw(QPainter* painter,
                         std::function<QPointF(QVariant,QVariant)> toPixel) const {
    if (!painter || !toPixel || !m_visible) return;
    if (m_points.size() < 3) return;

    // 全部转换到像素
    QPolygonF polygon;
    polygon.reserve(m_points.size());
    for (const auto& pt : m_points) {
        QPointF p = toPixel(pt.x(), pt.y());
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()))
            continue;  // 跳过 NaN 顶点
        polygon << p;
    }
    if (polygon.size() < 3) return;

    painter->save();

    // 填充色：默认用系列色的半透明版本
    QColor fill = m_fillColor.isValid() ? m_fillColor : m_color;
    fill.setAlpha(m_fillColor.isValid() ? fill.alpha() : 96);  // 默认 ~40% 透明度

    QPainterPath path;
    path.addPolygon(polygon);
    path.setFillRule(m_fillRule);

    painter->setBrush(fill);
    if (m_strokeVisible) {
        QPen pen(m_color, 1.5);
        pen.setCosmetic(true);
        painter->setPen(pen);
    } else {
        painter->setPen(Qt::NoPen);
    }
    painter->drawPath(path);

    painter->restore();
}

// ===== 命中检测：像素在多边形内部 =====
int QRegionSeries::hitTest(const QPointF& pixel,
                           std::function<QPointF(QVariant,QVariant)> toPixel) const {
    if (!toPixel || !m_visible || m_points.size() < 3) return -1;

    QPolygonF polygon;
    for (const auto& pt : m_points) {
        QPointF p = toPixel(pt.x(), pt.y());
        if (std::isfinite(p.x()) && std::isfinite(p.y()))
            polygon << p;
    }
    if (polygon.size() < 3) return -1;

    QPainterPath path;
    path.addPolygon(polygon);
    path.setFillRule(m_fillRule);

    return path.contains(pixel) ? 0 : -1;
}
