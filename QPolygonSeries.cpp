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
    if (!painter || !m_visible) return;
    // override 点已是 Numeric 空间，不需 toPixel；真实数据则必须要有
    if (!m_hasOverride && !toPixel) return;
    if (!m_hasOverride && m_points.size() < 3) return; // 至少 3 个点才能构成多边形

    // ── 点源统一为「Numeric 点 + 像素点」：曲线边逻辑只认 Numeric ──
    struct PtSrc { QPointF pixel; QPointF numeric; };
    QVector<PtSrc> pts;
    if (m_hasOverride) {
        pts.reserve(m_overridePoints.size());
        for (const auto& n : m_overridePoints) {
            QPointF px = ctx ? ctx->numericToPixel(n.x(), n.y())
                             : QPointF(qQNaN(), qQNaN());
            pts.append({px, n});
        }
    } else {
        pts.reserve(m_points.size());
        for (const auto& pt : m_points) {
            QPointF px = toPixel(pt.x(), pt.y());
            // 曲线边需要 Numeric；无 ctx/toNumeric 时留 NaN → 回退像素直线
            QPointF n(ctx && ctx->toNumeric0 ? ctx->toNumeric0(pt.x()) : qQNaN(),
                      ctx && ctx->toNumeric1 ? ctx->toNumeric1(pt.y()) : qQNaN());
            pts.append({px, n});
        }
    }
    if (pts.size() < 3) return;

    const qreal threshold = 20.0; // 像素距离阈值
    QPainterPath polyPath;

    // 首先将第一个点作为起点
    QPointF firstPx = pts[0].pixel;
    if (!std::isfinite(firstPx.x()) || !std::isfinite(firstPx.y())) return;
    polyPath.moveTo(firstPx);

    QPointF prevPx = firstPx;
    QPointF prevNumeric = pts[0].numeric;
    int curvesDrawn = 0, linesDrawn = 0;

    // 逐边 (i → i+1)
    for (int i = 0; i < pts.size(); ++i) {
        int next = (i + 1) % pts.size(); // 隐式闭合：末点连回第 0 点
        const auto& pt = pts[next];
        const QPointF& pixel = pt.pixel;

        if (!std::isfinite(pixel.x()) || !std::isfinite(pixel.y())) {
            // NaN 顶点 → 闭合当前子路径并重开
            polyPath.closeSubpath();
            if (next < pts.size() - 1) {
                const QPointF& restart = pts[next + 1].pixel;
                if (std::isfinite(restart.x()) && std::isfinite(restart.y()))
                    polyPath.moveTo(restart);
            }
            prevPx = pixel;
            prevNumeric = pt.numeric;
            continue;
        }

        qreal dist = std::hypot(pixel.x() - prevPx.x(), pixel.y() - prevPx.y());

        // 近点，或 Numeric 值不可用（无投影/转换失败）→ 像素直线
        if (dist < threshold || !ctx || !ctx->projection
            || !std::isfinite(pt.numeric.x()) || !std::isfinite(pt.numeric.y())
            || !std::isfinite(prevNumeric.x()) || !std::isfinite(prevNumeric.y())) {
            polyPath.lineTo(pixel);
            linesDrawn++;
        } else {
            // ── 远点：Numeric 空间 Lerp → createPath → 曲线 ──
            qreal dn0 = pt.numeric.x() - prevNumeric.x();
            qreal dn1 = pt.numeric.y() - prevNumeric.y();
            auto dataCurve = [prevNumeric, dn0, dn1](qreal t) -> QPointF {
                return QPointF(prevNumeric.x() + t * dn0,
                               prevNumeric.y() + t * dn1);
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
        prevPx = pixel;
        prevNumeric = pt.numeric;
    }
    polyPath.closeSubpath();

    // ── fill ──
    painter->save();
    QColor fill = m_fillColor.isValid() ? m_fillColor : color();
    fill.setAlpha(m_fillColor.isValid() ? fill.alpha() : 96);
    painter->setBrush(fill);
    if (m_strokeVisible) {
        QPen pen(color(), 1.5);
        pen.setCosmetic(true);
        painter->setPen(pen);
    } else {
        painter->setPen(Qt::NoPen);
    }
    polyPath.setFillRule(m_fillRule);
    painter->drawPath(polyPath);

    qCDebug(logSeriesVerbose) << "QPolygonSeries::draw:" << curvesDrawn << "curves,"
                              << linesDrawn << "lines," << pts.size() << "vertices";
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
