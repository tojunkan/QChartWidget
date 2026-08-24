// QViewRectAnimation.cpp —— 视窗相机动画实现
#include "QViewRectAnimation.h"
#include "QChartWidget.h"
#include <QDebug>
#include <QtMath>

QViewRectAnimation::QViewRectAnimation(QObject* parent)
    : QChartAnimation(parent) {}

// ===== 模式 A：lerp ─====
void QViewRectAnimation::setTargetViewRect(const QRectF& target) {
    m_dstRect = target;
    m_useGenerator = false;
}

void QViewRectAnimation::setWaypoint(const QPointF& center) {
    m_waypoint = center;
    m_hasWaypoint = true;
}

void QViewRectAnimation::setSizeCurve(std::function<qreal(qreal)> curve) {
    m_sizeCurve = std::move(curve);
}

// ===== 模式 B：Generator ─====
void QViewRectAnimation::setGenerator(Generator gen) {
    m_gen = std::move(gen);
    m_useGenerator = true;
}

// ===== 每帧更新 ─====
// 首次 animate 时自动快照 src 和长宽比；后续直接使用
void QViewRectAnimation::animate(qreal alpha) {
    if (!m_widget) return;

    if (m_useGenerator && m_gen) {
        QRectF out;
        m_gen(alpha, out);
        m_widget->setViewRect(out);
        return;
    }

    // ── 首次调用快照源 ──
    if (!m_srcRect.isValid()) {
        m_srcRect = m_widget->viewRect();
        if (!m_dstRect.isValid())
            m_dstRect = m_srcRect; // 未设目标 → 停原地
        // 快照长宽比（由 plotArea 决定，动画全程锁定）
        auto pa = m_widget->plotArea();
        if (pa.width() > 0.0 && pa.height() > 0.0)
            m_aspectRatio = pa.width() / pa.height();
        else
            m_aspectRatio = m_srcRect.width() / m_srcRect.height();
    }

    const QRectF& s = m_srcRect;
    const QRectF& d = m_dstRect;

    // ── 中心路径 ──
    QPointF srcCenter(s.center()), dstCenter(d.center());
    QPointF center;
    if (m_hasWaypoint) {
        // Quad Bézier: (1-α)²*P0 + 2(1-α)α*P1 + α²*P2
        qreal t1 = (1.0 - alpha) * (1.0 - alpha);
        qreal t2 = 2.0 * (1.0 - alpha) * alpha;
        qreal t3 = alpha * alpha;
        center = QPointF(t1 * srcCenter.x() + t2 * m_waypoint.x() + t3 * dstCenter.x(),
                         t1 * srcCenter.y() + t2 * m_waypoint.y() + t3 * dstCenter.y());
    } else {
        // 直线 lerp
        center = srcCenter + (dstCenter - srcCenter) * alpha;
    }

    // ── 视窗大小 ──
    qreal w;
    if (m_sizeCurve) {
        w = m_sizeCurve(alpha);
    } else {
        w = s.width() + (d.width() - s.width()) * alpha;
    }
    qreal h = w / m_aspectRatio;

    QRectF r(QPointF(center.x() - w / 2.0, center.y() - h / 2.0), QSizeF(w, h));
    m_widget->setViewRect(r);
}
