// QLineSeries.cpp —— 折线系列实现
#include "QLineSeries.h"
#include "QChartDebug.h"
#include "QChartAxis.h"  // DrawContext
#include <QPainterPath>
#include <QDebug>
#include <QtMath>
#include <cmath>

QLineSeries::QLineSeries(const QString& name, QObject* parent)
    : QXYSeries(name, parent) {
    m_color = QColor("#2196F3");
}

// ===== 绘制：混合策略——近点直线，远点 Numeric Lerp→createPath 曲线边 =====
void QLineSeries::draw(QPainter* painter,
                       std::function<QPointF(QVariant,QVariant)> toPixel,
                       const DrawContext* ctx) const {
    if (!painter || !m_visible) return;
    // override 点已是 Numeric 空间，不需 toPixel；真实数据则必须要有
    if (!m_hasOverride && !toPixel) return;

    // ── 点源统一为「Numeric 点 + 像素点」： ──
    // 动画覆盖层（Numeric 空间）优先，否则真实数据（Data 空间，经 toPixel/toNumeric 转换）
    // 曲线边逻辑只认 Numeric —— 两种来源共用同一套判定
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
    if (pts.size() < 2) return;

    const qreal threshold = qMax(20.0, m_lineWidth * 4.0); // 像素距离阈值

    painter->save();
    QPen pen(m_color, m_lineWidth, m_lineStyle);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush); // 折线不需要填充

    QPainterPath path;
    bool firstValid = true;
    QPointF prevPixel;   // 上一个有效像素点
    QPointF prevNumeric; // 上一个有效 Numeric 点（曲线边用）

    int curvesDrawn = 0, linesDrawn = 0;

    for (const auto& pt : pts) {
        const QPointF& pixel = pt.pixel;
        if (!std::isfinite(pixel.x()) || !std::isfinite(pixel.y())) {
            firstValid = true; // NaN → 断开路径
            continue;
        }

        if (firstValid) {
            path.moveTo(pixel);
            prevPixel = pixel;
            prevNumeric = pt.numeric;
            firstValid = false;
            continue;
        }

        qreal dist = std::hypot(pixel.x() - prevPixel.x(), pixel.y() - prevPixel.y());

        // 近点，或 Numeric 值不可用（无投影/转换失败）→ 像素直线
        if (dist < threshold || !ctx || !ctx->projection
            || !std::isfinite(pt.numeric.x()) || !std::isfinite(pt.numeric.y())
            || !std::isfinite(prevNumeric.x()) || !std::isfinite(prevNumeric.y())) {
            path.lineTo(pixel);
            linesDrawn++;
        } else {
            // ── 远点：Numeric 空间 Lerp → createPath → Pixel 曲线边 ──
            // 在 Numeric 空间（真正的"坐标系直线"）线性插值
            qreal dn0 = pt.numeric.x() - prevNumeric.x();
            qreal dn1 = pt.numeric.y() - prevNumeric.y();
            auto dataCurve = [prevNumeric, dn0, dn1](qreal t) -> QPointF {
                return QPointF(prevNumeric.x() + t * dn0,
                               prevNumeric.y() + t * dn1);
            };

            // 每 3 像素一个采样段（保证视觉平滑）
            int segments = qMax(16, static_cast<int>(dist / 3.0));
            qCDebug(logAxisVerbose) << "Line curve: dist=" << dist
                                    << "n0=[" << prevNumeric.x() << "→" << pt.numeric.x()
                                    << "] n1=[" << prevNumeric.y() << "→" << pt.numeric.y()
                                    << "] segments=" << segments;
            QPainterPath curve = ctx->toPixelCurve(dataCurve, segments);

            // toPixelCurve 返回的路径起点是 moveTo ——需要改为已建立的路径继续
            // 直接把 curve 的元素追加到 path（跳过第一个 moveTo）
            for (int i = 0; i < curve.elementCount(); ++i) {
                const auto& el = curve.elementAt(i);
                if (i == 0 || el.isMoveTo())
                    path.moveTo(QPointF(el.x, el.y));
                else
                    path.lineTo(QPointF(el.x, el.y));
            }
            curvesDrawn++;
        }

        prevPixel = pixel;
        prevNumeric = pt.numeric;
    }

    painter->drawPath(path);

    if (curvesDrawn > 0 || linesDrawn > 0)
        qCDebug(logSeriesVerbose) << "QLineSeries::draw:" << curvesDrawn << "curves,"
                                  << linesDrawn << "lines,"
                                  << pts.size() << "total points";

    // ── 边界诊断：多少点在 plotArea 外被浪费了？ ──
    // 注：QPainter 最终会 clip，toPixel 的计算开销在此统计
    if (logAxisVerbose().isDebugEnabled()) {
        int inside = 0, outside = 0;
        for (const auto& pt : pts) {
            const QPointF& p = pt.pixel;
            if (!std::isfinite(p.x()) || !std::isfinite(p.y())) continue;
            if (p.x() >= 0 && p.y() >= 0 && ctx && ctx->plotArea.contains(p))
                inside++;
            else
                outside++;
        }
        qCDebug(logAxisVerbose) << "QLineSeries boundary: inside=" << inside
                                 << "outside=" << outside;
    }

    painter->restore();
}

// ===== 命中检测 =====
int QLineSeries::hitTest(const QPointF& pixel,
                         std::function<QPointF(QVariant,QVariant)> toPixel,
                         const DrawContext* ctx) const {
    Q_UNUSED(ctx); // 命中检测暂不优化曲线——像素距离已够用
    if (!toPixel || !m_visible || m_points.size() < 2) return -1;

    const qreal threshold = qMax(4.0, m_lineWidth) + 4.0;
    QVector<QPointF> screen;
    screen.reserve(m_points.size());
    for (const auto& pt : m_points) {
        QPointF p = toPixel(pt.x(), pt.y());
        screen.append(std::isfinite(p.x()) && std::isfinite(p.y())
                      ? p : QPointF(qQNaN(), qQNaN()));
    }

    int bestIndex = -1;
    qreal bestDist = threshold;
    for (int i = 0; i < screen.size() - 1; ++i) {
        const QPointF& a = screen[i];
        const QPointF& b = screen[i + 1];
        if (!std::isfinite(a.x()) || !std::isfinite(b.x())) continue;

        QPointF ab = b - a;
        qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
        qreal t = (len2 > 0)
            ? ((pixel.x() - a.x()) * ab.x() + (pixel.y() - a.y()) * ab.y()) / len2
            : 0.0;
        t = qBound(0.0, t, 1.0);
        QPointF closest = a + ab * t;
        qreal d = std::hypot(pixel.x() - closest.x(), pixel.y() - closest.y());

        if (d < bestDist) { bestDist = d; bestIndex = i; }
    }
    return bestIndex;
}

// ===== Catmull-Rom 平滑路径（暂不用——Numeric Lerp 替代了像素平滑）=====
QPainterPath QLineSeries::smoothPath(const QVector<QPointF>& pts) const {
    QPainterPath path;
    if (pts.size() < 2) return path;
    path.moveTo(pts[0]);
    for (int i = 0; i < pts.size() - 1; ++i) {
        const QPointF& p0 = pts[qMax(0, i - 1)];
        const QPointF& p1 = pts[i];
        const QPointF& p2 = pts[i + 1];
        const QPointF& p3 = pts[qMin(pts.size() - 1, i + 2)];
        QPointF c1 = p1 + (p2 - p0) / 6.0;
        QPointF c2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(c1, c2, p2);
    }
    return path;
}
