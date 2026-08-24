// QBarSeries.h —— 柱状系列
// 每个柱 = Data 空间的一个矩形 (left, top, right, bottom)。
// 只存 Data，绘制用注入的 toPixel。不知道 Axis/Projection 类型。
// Cartesian 下投影后若四角构成轴对齐矩形 → drawRect 快路径；
// 否则（Polar/Functional 变形）→ drawPolygon。
#pragma once
#include "QChartSeries.h"
#include "QDataRect.h"
#include <QVector>
#include <QPen>

class QBarSeries : public QChartSeries
{
    Q_OBJECT
public:
    explicit QBarSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 数据：Data 空间矩形 =====
    int count() const override { return m_rects.size(); }
    void append(qreal left, qreal top, qreal right, qreal bottom);
    void append(const QDataRect& rect);
    void replace(int i, const QDataRect& rect);
    void remove(int i);
    void clear();
    QDataRect at(int i) const { return m_rects.at(i); }
    const QVector<QDataRect>& rectangles() const { return m_rects; }

    // ===== 绘制 =====
    void draw(QPainter* painter,
              std::function<QPointF(QVariant,QVariant)> toPixel,
              const DrawContext* ctx = nullptr) const override;

    // ===== 命中检测：像素在矩形内 =====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel,
                const DrawContext* ctx = nullptr) const override;

    // ===== 动画覆盖层 —— 优先于真实数据被 draw() 使用 =====
    // 柱集动画（排序演示等）期间，Numeric 空间矩形临时替换真实 Data 矩形
    void setRenderOverride(const QVector<QRectF>& numericRects);
    void clearRenderOverride();
    bool hasRenderOverride() const { return m_hasOverride; }
    const QVector<QRectF>& renderOverride() const { return m_overrideRects; }

    // ===== 样式 =====
    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor& c) { m_fillColor = c; }
    QPen pen() const { return m_pen; }
    void setPen(const QPen& p) { m_pen = p; }

signals:
    void dataChanged();
    void renderOverrideChanged();   // 动画覆盖层变化（每帧可能多次）

private:
    QVector<QDataRect> m_rects;     // Data 空间矩形
    QVector<QRectF> m_overrideRects; // 动画临时矩形（Numeric 空间），优先于 m_rects
    bool m_hasOverride = false;
    QColor m_fillColor;           // 空 = 用系列色
    QPen m_pen = Qt::NoPen;
};
