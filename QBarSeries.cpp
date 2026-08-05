// QBarSeries.cpp —— 柱状系列实现
#include "QBarSeries.h"
#include "QChartDebug.h"
#include <QPainterPath>
#include <QDebug>
#include <cmath>

QBarSeries::QBarSeries(const QString& name, QObject* parent)
    : QChartSeries(name, parent) {
    m_color = QColor("#2196F3");
}

// ===== 数据操作 =====
void QBarSeries::append(qreal left, qreal top, qreal right, qreal bottom) {
    // QDataRect(left, bottom, right, top) — matemathical order
    m_rects.append(QDataRect(left, bottom, right, top));
    emit dataChanged();
}

void QBarSeries::append(const QDataRect& rect) {
    m_rects.append(rect);
    emit dataChanged();
}

void QBarSeries::replace(int i, const QDataRect& rect) {
    if (i < 0 || i >= m_rects.size()) return;
    m_rects[i] = rect;
    emit dataChanged();
}

void QBarSeries::remove(int i) {
    if (i < 0 || i >= m_rects.size()) return;
    m_rects.removeAt(i);
    emit dataChanged();
}

void QBarSeries::clear() {
    if (m_rects.isEmpty()) return;
    m_rects.clear();
    emit dataChanged();
}

// ===== 绘制：四边 near/far 策略，Cartesian 走 drawRect 快路径 =====
void QBarSeries::draw(QPainter* painter,
                      std::function<QPointF(QVariant,QVariant)> toPixel,
                      const DrawContext* ctx) const {
    Q_UNUSED(ctx);
    if (!painter || !toPixel || !m_visible) return;

    painter->save();
    QColor fill = m_fillColor.isValid() ? m_fillColor : m_color;
    painter->setBrush(fill);
    painter->setPen(m_pen);

    int rects = 0, polys = 0;
    for (const auto& r : m_rects) {
        QPointF tl = toPixel(r.left(),  r.top());
        QPointF tr = toPixel(r.right(), r.top());
        QPointF br = toPixel(r.right(), r.bottom());
        QPointF bl = toPixel(r.left(),  r.bottom());

        if (!std::isfinite(tl.x()) || !std::isfinite(tr.x())
            || !std::isfinite(br.x()) || !std::isfinite(bl.x())) continue;

        // 轴对齐检测（Cartesian 投影下为真 → drawRect 快路径）
        bool axisAligned =
            qAbs(tl.y() - tr.y()) < 0.5 && qAbs(tl.x() - bl.x()) < 0.5
            && qAbs(br.y() - bl.y()) < 0.5 && qAbs(br.x() - tr.x()) < 0.5;

        if (axisAligned) {
            painter->drawRect(QRectF(tl, br));
            rects++;
        } else {
            // 变形投影 → 四边各走 near/far 构建多边形
            // 四角已算出：tl, tr, br, bl
            QPolygonF poly;
            poly << tl << tr << br << bl;
            painter->drawPolygon(poly);
            polys++;
            // 注：近/far 曲线边策略（toPixelCurve）在 Bar 的四边上复用
            // 当前 polys 路径仅画直线边——完全曲线化留待后续优化
        }
    }

    qCDebug(logSeriesVerbose) << "QBarSeries::draw:" << rects << "rects," << polys << "polygons";
    painter->restore();
}

// ===== 命中检测：像素在矩形内 =====
int QBarSeries::hitTest(const QPointF& pixel,
                        std::function<QPointF(QVariant,QVariant)> toPixel,
                        const DrawContext* ctx) const {
    Q_UNUSED(ctx);
    if (!toPixel || !m_visible) return -1;

    for (int i = 0; i < m_rects.size(); ++i) {
        const auto& r = m_rects[i];
        QPointF tl = toPixel(QVariant::fromValue(r.left()),  QVariant::fromValue(r.top()));
        QPointF br = toPixel(QVariant::fromValue(r.right()), QVariant::fromValue(r.bottom()));
        if (!std::isfinite(tl.x()) || !std::isfinite(br.x())) continue;

        // 像素空间 bbox 检测（Cartesian 下精确，变形下近似）
        QRectF bbox(tl, br);
        if (bbox.normalized().contains(pixel))
            return i;
    }
    return -1;
}
