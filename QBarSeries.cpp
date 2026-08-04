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
    m_rects.append(QRectF(left, top, right - left, bottom - top));
    emit dataChanged();
}

void QBarSeries::append(const QRectF& rect) {
    m_rects.append(rect);
    emit dataChanged();
}

void QBarSeries::replace(int i, const QRectF& rect) {
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

// ===== 绘制：矩形 → 检测轴对齐 → drawRect 快路径 / drawPolygon =====
void QBarSeries::draw(QPainter* painter,
                      std::function<QPointF(QVariant,QVariant)> toPixel) const {
    if (!painter || !toPixel || !m_visible) return;

    // Data 空间矩形 → 投影四个角 → 像素
    // 注意 QRectF 的 top 在 Data 空间是"大值"（Numeric 向上），
    // 投影后 Y 翻转由 toPixel 处理，四角关系保持
    painter->save();

    QColor fill = m_fillColor.isValid() ? m_fillColor : m_color;
    painter->setBrush(fill);
    painter->setPen(m_pen);

    int drawn = 0, fallback = 0;
    for (const auto& r : m_rects) {
        // 四个角（Data 空间）
        QPointF tl = toPixel(QVariant::fromValue(r.left()),  QVariant::fromValue(r.top()));
        QPointF tr = toPixel(QVariant::fromValue(r.right()), QVariant::fromValue(r.top()));
        QPointF br = toPixel(QVariant::fromValue(r.right()), QVariant::fromValue(r.bottom()));
        QPointF bl = toPixel(QVariant::fromValue(r.left()),  QVariant::fromValue(r.bottom()));

        if (!std::isfinite(tl.x()) || !std::isfinite(tl.y())
            || !std::isfinite(tr.x()) || !std::isfinite(tr.y())
            || !std::isfinite(br.x()) || !std::isfinite(br.y())
            || !std::isfinite(bl.x()) || !std::isfinite(bl.y())) {
            continue;  // NaN 角 → 跳过
        }

        // 轴对齐检测：tl/tr 同高、tl/bl 同宽（容差 0.5px）
        bool axisAligned =
            qAbs(tl.y() - tr.y()) < 0.5 && qAbs(tl.x() - bl.x()) < 0.5
            && qAbs(br.y() - bl.y()) < 0.5 && qAbs(br.x() - tr.x()) < 0.5;

        if (axisAligned) {
            // 快路径：drawRect（笛卡尔最常见）
            painter->drawRect(QRectF(tl, br));
            drawn++;
        } else {
            // 变形路径：投影后不是矩形（Polar/Functional）
            QPolygonF poly;
            poly << tl << tr << br << bl;
            painter->drawPolygon(poly);
            fallback++;
        }
    }

    qCDebug(logSeriesVerbose) << "QBarSeries::draw:" << drawn << "rects,"
                              << fallback << "polygons";
    painter->restore();
}

// ===== 命中检测：像素在矩形内 =====
int QBarSeries::hitTest(const QPointF& pixel,
                        std::function<QPointF(QVariant,QVariant)> toPixel) const {
    if (!toPixel || !m_visible) return -1;

    for (int i = 0; i < m_rects.size(); ++i) {
        const auto& r = m_rects[i];
        QPointF tl = toPixel(QVariant::fromValue(r.left()),  QVariant::fromValue(r.top()));
        QPointF br = toPixel(QVariant::fromValue(r.right()), QVariant::fromValue(r.bottom()));
        if (!std::isfinite(tl.x()) || !std::isfinite(br.x())) continue;

        // 归一化到轴对齐包围盒检测（变形后仍用 bbox 近似）
        QRectF bbox(tl, br);
        bbox = bbox.normalized();
        if (bbox.contains(pixel))
            return i;
    }
    return -1;
}
