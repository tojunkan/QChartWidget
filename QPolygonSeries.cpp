// QPolygonSeries.cpp —— 多边形系列实现
// 每边用 near/far 策略（复用 LineSeries 逻辑），全部边连成封闭多边形后 fill。
#include "QPolygonSeries.h"
#include "QChartAxis.h" // DrawContext
#include "QChartDebug.h"
#include <QPainterPath>
#include <QDebug>
#include <cmath>

QPolygonSeries::QPolygonSeries(const QString& name, QObject* parent)
    : QXYSeries(name, parent) {}

// ===== 绘制：逐边 near/far 策略构建多边形路径，fill =====
void QPolygonSeries::draw(QPainter* painter,
                          std::function<QPointF(QVariant,QVariant)> toPixel,
                          const DrawContext* ctx) const {
    if (!painter || !toPixel || !m_visible) return;
    if (m_points.size() < 3) return; // 至少 3 个点才能构成多边形

    const qreal threshold = 20.0; // 像素距离阈值
    QPainterPath polyPath;

    // 首先将第一个点作为起点
    QPointF firstPx = toPixel(m_points[0].x(), m_points[0].y());
    if (!std::isfinite(firstPx.x()) || !std::isfinite(firstPx.y())) return;
    polyPath.moveTo(firstPx);

    QPointF prevPx = firstPx;
    QDataPoint prevPt = m_points[0];
    int curvesDrawn = 0, linesDrawn = 0;

    // 逐边 (i → i+1)
    for (int i = 0; i < m_points.size(); ++i) {
        int next = (i + 1) % m_points.size(); // 隐式闭合：末点连回第 0 点
        const auto& pt = m_points[next];
        QPointF pixel = toPixel(pt.x(), pt.y());

        if (!std::isfinite(pixel.x()) || !std::isfinite(pixel.y())) {
            // NaN 顶点 → 闭合当前子路径并重开
            polyPath.closeSubpath();
            if (next < m_points.size() - 1) {
                QPointF restart = toPixel(m_points[next + 1].x(), m_points[next + 1].y());
                if (std::isfinite(restart.x()) && std::isfinite(restart.y()))
                    polyPath.moveTo(restart);
            }
            prevPx = pixel;
            prevPt = pt;
            continue;
        }

        qreal dist = std::hypot(pixel.x() - prevPx.x(), pixel.y() - prevPx.y());

        // 近点或无可用 Numeric 转换 → 像素直线
        if (dist < threshold || !ctx || !ctx->toNumeric0 || !ctx->toNumeric1
            || !ctx->projection) {
            polyPath.lineTo(pixel);
            linesDrawn++;
        } else {
            // ── 远点：Numeric 空间 Lerp → createPath → 曲线 ──
            qreal n0_a = ctx->toNumeric0(prevPt.x());
            qreal n1_a = ctx->toNumeric1(prevPt.y());
            qreal n0_b = ctx->toNumeric0(pt.x());
            qreal n1_b = ctx->toNumeric1(pt.y());

            if (!std::isfinite(n0_a) || !std::isfinite(n1_a)
                || !std::isfinite(n0_b) || !std::isfinite(n1_b)) {
                polyPath.lineTo(pixel);
                linesDrawn++;
            } else {
                qreal dn0 = n0_b - n0_a;
                qreal dn1 = n1_b - n1_a;
                auto dataCurve = [=](qreal t) -> QPointF {
                    return QPointF(n0_a + t * dn0, n1_a + t * dn1);
                };
                int segments = qMax(16, static_cast<int>(dist / 3.0));
                QPainterPath curve = ctx->toPixelCurve(dataCurve, segments);
                // 追加曲线元素到 polyPath
                for (int j = 0; j < curve.elementCount(); ++j) {
                    const auto& el = curve.elementAt(j);
                    if (j == 0 || el.isMoveTo())
                        polyPath.moveTo(QPointF(el.x, el.y));
                    else
                        polyPath.lineTo(QPointF(el.x, el.y));
                }
                curvesDrawn++;
            }
        }
        prevPx = pixel;
        prevPt = pt;
    }
    polyPath.closeSubpath();

    // ── fill ──
    painter->save();
    QColor fill = m_fillColor.isValid() ? m_fillColor : m_color;
    fill.setAlpha(m_fillColor.isValid() ? fill.alpha() : 96);
    painter->setBrush(fill);
    if (m_strokeVisible) {
        QPen pen(m_color, 1.5);
        pen.setCosmetic(true);
        painter->setPen(pen);
    } else {
        painter->setPen(Qt::NoPen);
    }
    polyPath.setFillRule(m_fillRule);
    painter->drawPath(polyPath);

    qCDebug(logSeriesVerbose) << "QPolygonSeries::draw:" << curvesDrawn << "curves,"
                              << linesDrawn << "lines," << m_points.size() << "vertices";
    painter->restore();
}

// ===== 命中检测：像素在多边形内部 =====
int QPolygonSeries::hitTest(const QPointF& pixel,
                            std::function<QPointF(QVariant,QVariant)> toPixel,
                            const DrawContext* ctx) const {
    Q_UNUSED(ctx);
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
