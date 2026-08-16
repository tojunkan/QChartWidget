// QLineSeries.cpp —— 折线系列实现
#include "QLineSeries.h"
#include "QChartDebug.h"
#include "QChartAxis.h"  // DrawContext
#include <QPainterPath>
#include <QDebug>
#include <QtMath>
#include <cmath>

// ===== 折线粗筛常量与辅助（匿名 namespace）=====
namespace {
// 远段（曲线）过采样步数：仅稀疏段触发，判"弧是否拱进视口"
constexpr int  kFarSegmentSampleCount = 8;
// 远段采样近似误差余量（像素）：采样 bbox 与 plotArea 相离 > margin 才判"全在外"
constexpr qreal kRejectMargin = 4.0;

// 两端点像素 bbox（已归一化）
inline QRectF segBbox(const QPointF& a, const QPointF& b) {
    return QRectF(a, b).normalized();
}

// 远段（曲线）过采样判相交：在 Numeric 空间线性插值采样，落回像素累 bbox。
// 采样 bbox 与 plotArea 相离超过 margin 才判"全在外"（保守：靠边交给 clip）。
bool segmentOutsideViaSampling(const QPointF& nPrev, const QPointF& nCurr,
                               const DrawContext* ctx, int K, qreal margin) {
    if (!ctx || !ctx->projection) return false;   // 无 ctx → 保守不裁
    const QPointF dn = nCurr - nPrev;
    QRectF bbox;
    bool first = true;
    for (int i = 1; i < K; ++i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(K);
        const QPointF p = ctx->numericToPixel(nPrev.x() + dn.x() * t,
                                              nPrev.y() + dn.y() * t);
        // 采样 NaN（如穿过奇点）→ 保守不裁，交 clip
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()))
            return false;
        if (first) { bbox = QRectF(p, p); first = false; }
        else       { bbox = bbox.united(QRectF(p, p)); }
    }
    if (first) return false;   // 没采到点
    return !ctx->rectVisible(bbox, margin);
}
} // namespace

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

    const qreal threshold = qMax(20.0, m_lineWidth * 4.0); // 像素距离阈值（近/远段分界）

    painter->save();
    QPen pen(m_color, m_lineWidth, m_lineStyle);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush); // 折线不需要填充

    QPainterPath path;
    bool havePrev = false;          // 是否有可连的上一有效点
    bool skipRun = false;           // 是否处于连续屏外段
    QPointF pendingAnchor;          // 最近被跳过的屏外点（重入锚点）
    QPointF prevPixel;              // 上一个有效像素点
    QPointF prevNumeric;            // 上一个有效 Numeric 点（曲线边用）

    int curvesDrawn = 0, linesDrawn = 0, culled = 0;

    for (const auto& pt : pts) {
        const QPointF& pixel = pt.pixel;
        if (!std::isfinite(pixel.x()) || !std::isfinite(pixel.y())) {
            havePrev = false;   // NaN → 断开路径
            skipRun = false;
            prevPixel = pixel;
            prevNumeric = pt.numeric;
            continue;
        }

        if (!havePrev) {
            path.moveTo(pixel);
            havePrev = true;
            prevPixel = pixel;
            prevNumeric = pt.numeric;
            continue;
        }

        qreal dist = std::hypot(pixel.x() - prevPixel.x(), pixel.y() - prevPixel.y());

        // 近段：像素直线（近点，或 Numeric 不可用）；远段：Numeric Lerp 曲线
        bool near = (dist < threshold) || !ctx || !ctx->projection
            || !std::isfinite(pt.numeric.x()) || !std::isfinite(pt.numeric.y())
            || !std::isfinite(prevNumeric.x()) || !std::isfinite(prevNumeric.y());

        bool outside = false;
        if (near) {
            // 近段直线：端点 bbox 即真实线段 bbox，精确判相交（margin=线宽出血）
            outside = ctx && !ctx->rectVisible(segBbox(prevPixel, pixel), m_lineWidth);
        } else {
            // 远段曲线：过采样判相交，防"两端在外但弧拱进视口"漏画
            outside = segmentOutsideViaSampling(prevNumeric, pt.numeric, ctx,
                                                kFarSegmentSampleCount,
                                                m_lineWidth + kRejectMargin);
        }

        if (outside) {
            // 跳过整段：只记录锚点，不产生任何路径元素（折叠连续屏外段）
            pendingAnchor = pixel;
            skipRun = true;
            culled++;
        } else {
            if (skipRun) {
                path.moveTo(pendingAnchor);  // 重入：从最后一个屏外点连回
                skipRun = false;
            }
            if (near) {
                path.lineTo(pixel);
                linesDrawn++;
            } else {
                // ── 远点：Numeric 空间 Lerp → createPath → Pixel 曲线边 ──
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

                // toPixelCurve 返回的路径起点是 moveTo —— 追加到 path（跳过第一个 moveTo）
                for (int i = 0; i < curve.elementCount(); ++i) {
                    const auto& el = curve.elementAt(i);
                    if (i == 0 || el.isMoveTo())
                        path.moveTo(QPointF(el.x, el.y));
                    else
                        path.lineTo(QPointF(el.x, el.y));
                }
                curvesDrawn++;
            }
        }

        prevPixel = pixel;
        prevNumeric = pt.numeric;
    }

    painter->drawPath(path);

    if (curvesDrawn > 0 || linesDrawn > 0)
        qCDebug(logSeriesVerbose) << "QLineSeries::draw:" << curvesDrawn << "curves,"
                                  << linesDrawn << "lines,"
                                  << culled << "culled,"
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
