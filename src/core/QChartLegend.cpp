// QChartLegend.cpp —— 图例实现（Phase 1：单列竖向，四角 overlay）
#include "QChartLegend.h"
#include "QChartSeries.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logLegend, "chart.legend.debug")   // 图例调试（Phase 1 简化：单列竖向，四角 overlay）

namespace {
// 布局常量（Phase 1 简化：固定行高/色块，文字宽度按字符数估算）
constexpr qreal PADDING   = 8.0;    // 图例内边距
constexpr qreal ROW_HEIGHT = 18.0;  // 每行高度
constexpr qreal COLOR_BOX = 12.0;   // 色块边长
constexpr qreal GAP       = 4.0;    // 色块与文字间距
constexpr qreal INSET     = 8.0;    // 距 plotArea 角落内缩
constexpr qreal CHAR_W    = 8.0;    // 每字符估算宽度

struct Layout {
    QRectF box;
    QVector<QRectF> rows;
};

Layout computeLayout(Qt::Alignment alignment, const QRectF& plotArea,
                     const QList<QChartSeries*>& items, const QFontMetricsF& fm = QFontMetricsF(QFont())) {
    Layout L;
    const int n = items.size();
    if (n == 0) return L;

    qreal textW = 0.0;
    for (const QChartSeries* s : items)
        if (s) textW = qMax(textW, fm.horizontalAdvance(s->name()));

    const qreal contentW = COLOR_BOX + GAP + textW;
    const qreal boxW = contentW + 2.0 * PADDING;
    const qreal boxH = n * ROW_HEIGHT + 2.0 * PADDING;

    qreal x, y;
    if (alignment & Qt::AlignRight) x = plotArea.right() - boxW - INSET;
    else                            x = plotArea.left() + INSET;
    if (alignment & Qt::AlignBottom) y = plotArea.bottom() - boxH - INSET;
    else                             y = plotArea.top() + INSET;

    L.box = QRectF(x, y, boxW, boxH);
    for (int i = 0; i < n; ++i)
        L.rows.append(QRectF(x + PADDING, y + PADDING + i * ROW_HEIGHT, contentW, ROW_HEIGHT));
    return L;
}
} // namespace

QChartLegend::QChartLegend(QObject* parent) : QObject(parent) {}

void QChartLegend::setVisible(bool v) {
    if (m_visible == v) return;
    m_visible = v;
    emit visibleChanged();
}

void QChartLegend::setAlignment(Qt::Alignment a) {
    if (m_alignment == a) return;
    m_alignment = a;
    emit alignmentChanged();
}

void QChartLegend::setTextColor(const QColor& c) {
    if (m_textColorOverride && *m_textColorOverride == c) return;
    m_textColorOverride = c;
    emit textColorChanged();
}

void QChartLegend::setThemeTextColor(const QColor& c) {
    m_themeTextColor = c;
    if (!m_textColorOverride) emit textColorChanged();   // 仅无 override 时真正变化
}

void QChartLegend::clearTextColor() {
    if (!m_textColorOverride) return;
    m_textColorOverride.reset();
    emit textColorChanged();
}

void QChartLegend::draw(QPainter* p, const QRectF& plotArea,
                        const QList<QChartSeries*>& items) const {
    if (!p || !m_visible || items.isEmpty()) return;
    const Layout L = computeLayout(m_alignment, plotArea, items, p->fontMetrics());
    if (L.box.isEmpty()) return;

    p->save();
    p->setClipRect(plotArea);

    // 半透明背景：与文字色互补，保证两种主题下可读
    const QColor text = textColor();
    const QColor bg = text.lightness() < 128 ? QColor(255, 255, 255, 200)
                                             : QColor(0, 0, 0, 60);
    p->fillRect(L.box, bg);

    for (int i = 0; i < items.size(); ++i) {
        const QChartSeries* s = items[i];
        if (!s) continue;
        const QRectF row = L.rows[i];
        const qreal opacity = s->isVisible() ? 1.0 : 0.35;   // 隐藏项降透明
        const QRectF colorBox(row.left(),
                              row.top() + (ROW_HEIGHT - COLOR_BOX) / 2.0,
                              COLOR_BOX, COLOR_BOX);

        p->setOpacity(opacity);
        p->fillRect(colorBox, s->color());
        p->setPen(text);
        p->setBrush(Qt::NoBrush);
        p->drawText(row.adjusted(COLOR_BOX + GAP, 0, 0, 0),
                    Qt::AlignVCenter | Qt::AlignLeft, s->name());
        qCDebug(logLegend) << "row:" << row << "textRect:" << row.adjusted(COLOR_BOX + GAP, 0, 0, 0);
    }
    p->restore();
}

QChartSeries* QChartLegend::seriesAt(const QPointF& pos, const QRectF& plotArea,
                                     const QList<QChartSeries*>& items) const {
    if (!m_visible) return nullptr;
    const Layout L = computeLayout(m_alignment, plotArea, items);
    if (L.box.isEmpty() || !L.box.contains(pos)) return nullptr;
    for (int i = 0; i < L.rows.size() && i < items.size(); ++i)
        if (L.rows[i].contains(pos)) return items[i];
    return nullptr;
}

QRectF QChartLegend::boundingRect(const QRectF& plotArea,
                                  const QList<QChartSeries*>& items) const {
    return computeLayout(m_alignment, plotArea, items).box;
}

QRectF QChartLegend::itemRect(int index, const QRectF& plotArea,
                              const QList<QChartSeries*>& items) const {
    const Layout L = computeLayout(m_alignment, plotArea, items);
    if (index < 0 || index >= L.rows.size()) return QRectF();
    return L.rows[index];
}
