// QChartLegend.h —— 图例（Phase 1）
// 独立 QObject（非 QWidget），由 renderer 在 plotArea 内 overlay 绘制，交互由 widget 转发。
// 单列竖向：色块 + 名字；隐藏系列降透明度。
// 文字色采用 override 双槽（design_theme.md §2）：主题推 textColor 作默认，显式设色优先。
#ifndef QCHARTLEGEND_H
#define QCHARTLEGEND_H

#include <QObject>
#include <QColor>
#include <QRectF>
#include <QPointF>
#include <QList>
#include <QPainter>
#include <optional>

class QChartSeries;

class QChartLegend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment NOTIFY alignmentChanged)
public:
    explicit QChartLegend(QObject* parent = nullptr);

    bool isVisible() const { return m_visible; }
    void setVisible(bool v);

    // 四角：AlignLeft|AlignTop（默认）/ AlignRight|AlignTop /
    //        AlignLeft|AlignBottom / AlignRight|AlignBottom
    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment a);

    // 文字色：override 双槽
    void setTextColor(const QColor& c);
    void setThemeTextColor(const QColor& c);
    void clearTextColor();
    QColor textColor() const { return m_textColorOverride.value_or(m_themeTextColor); }
    std::optional<QColor> textColorOverride() const { return m_textColorOverride; }

    // 绘制（单列竖向：色块 + 名字；隐藏系列降透明度）
    void draw(QPainter* p, const QRectF& plotArea, const QList<QChartSeries*>& items) const;
    // 命中：返回命中的 series，未命中返回 nullptr
    QChartSeries* seriesAt(const QPointF& pos, const QRectF& plotArea,
                           const QList<QChartSeries*>& items) const;
    // 边界（供 widget 判点击、供将来外置布局预留）
    QRectF boundingRect(const QRectF& plotArea, const QList<QChartSeries*>& items) const;
    // 第 index 个图例项的可点击行矩形（测试/交互用；越界返回空）
    QRectF itemRect(int index, const QRectF& plotArea, const QList<QChartSeries*>& items) const;

signals:
    void visibleChanged();
    void alignmentChanged();
    void textColorChanged();

private:
    bool m_visible = true;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignTop;
    std::optional<QColor> m_textColorOverride;
    QColor m_themeTextColor = Qt::black;
};

#endif // QCHARTLEGEND_H
