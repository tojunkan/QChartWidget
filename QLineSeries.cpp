// QLineSeries.cpp —— 折线系列实现
#include "QLineSeries.h"
#include "QChartDebug.h"
#include <QPainterPath>
#include <QDebug>
#include <QtMath>
#include <cmath>

QLineSeries::QLineSeries(const QString& name, QObject* parent)
    : QXYSeries(name, parent) {
    m_color = QColor("#2196F3");
}

// ===== 绘制：逐点连折线，NaN 断开 =====
void QLineSeries::draw(QPainter* painter,
                       std::function<QPointF(QVariant,QVariant)> toPixel) const {
    if (!painter || !toPixel || !m_visible) return;
    if (m_points.size() < 2) return;

    // 全部转换到像素
    QVector<QPointF> screen;
    screen.reserve(m_points.size());
    for (const auto& pt : m_points) {
        QPointF p = toPixel(pt.x(), pt.y());
        if (!std::isfinite(p.x()) || !std::isfinite(p.y())) {
            screen.append(QPointF(qQNaN(), qQNaN()));  // 标记断点
        } else {
            screen.append(p);
        }
    }

    painter->save();
    QPen pen(m_color, m_lineWidth, m_lineStyle);
    pen.setCosmetic(true);  // 线宽不随缩放变化
    painter->setPen(pen);

    if (m_smooth) {
        // 平滑：Catmull-Rom（分段处理，跳过 NaN 断点）
        int start = 0;
        for (int i = 0; i <= screen.size(); ++i) {
            bool isBreak = (i == screen.size())
                        || !std::isfinite(screen[i].x());
            if (isBreak) {
                if (i - start >= 2) {
                    QVector<QPointF> seg = screen.mid(start, i - start);
                    painter->drawPath(smoothPath(seg));
                }
                start = i + 1;
            }
        }
    } else {
        // 折线：逐段画，NaN 断点处断开
        QPainterPath path;
        bool firstValid = true;
        for (const auto& p : screen) {
            if (!std::isfinite(p.x())) {
                firstValid = true;
                continue;
            }
            if (firstValid) {
                path.moveTo(p);
                firstValid = false;
            } else {
                path.lineTo(p);
            }
        }
        painter->drawPath(path);
    }

    painter->restore();
}

// ===== 命中检测：像素到最近线段的垂直距离 < 阈值 =====
int QLineSeries::hitTest(const QPointF& pixel,
                         std::function<QPointF(QVariant,QVariant)> toPixel) const {
    if (!toPixel || !m_visible || m_points.size() < 2) return -1;

    const qreal threshold = qMax(4.0, m_lineWidth) + 4.0;  // 线宽 + 容差

    // 转像素，跳过 NaN
    QVector<QPointF> screen;
    screen.reserve(m_points.size());
    for (const auto& pt : m_points) {
        QPointF p = toPixel(pt.x(), pt.y());
        if (std::isfinite(p.x()) && std::isfinite(p.y()))
            screen.append(p);
        else
            screen.append(QPointF(qQNaN(), qQNaN()));
    }

    // 点到线段距离：遍历每段
    int bestIndex = -1;
    qreal bestDist = threshold;
    for (int i = 0; i < screen.size() - 1; ++i) {
        const QPointF& a = screen[i];
        const QPointF& b = screen[i + 1];
        if (!std::isfinite(a.x()) || !std::isfinite(b.x())) continue;

        // 点 P 到线段 AB 的距离
        QPointF ab = b - a;
        qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
        qreal t = (len2 > 0)
            ? ((pixel.x() - a.x()) * ab.x() + (pixel.y() - a.y()) * ab.y()) / len2
            : 0.0;
        t = qBound(0.0, t, 1.0);
        QPointF closest = a + ab * t;
        qreal d = std::hypot(pixel.x() - closest.x(), pixel.y() - closest.y());

        if (d < bestDist) {
            bestDist = d;
            bestIndex = i;
        }
    }
    return bestIndex;
}

// ===== Catmull-Rom 平滑路径 =====
QPainterPath QLineSeries::smoothPath(const QVector<QPointF>& pts) const {
    QPainterPath path;
    if (pts.size() < 2) return path;

    const int segmentsPerSpan = 8;
    path.moveTo(pts[0]);

    for (int i = 0; i < pts.size() - 1; ++i) {
        const QPointF& p0 = pts[qMax(0, i - 1)];
        const QPointF& p1 = pts[i];
        const QPointF& p2 = pts[i + 1];
        const QPointF& p3 = pts[qMin(pts.size() - 1, i + 2)];

        // Catmull-Rom → Bezier 控制点
        QPointF c1 = p1 + (p2 - p0) / 6.0;
        QPointF c2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(c1, c2, p2);
    }
    return path;
}
