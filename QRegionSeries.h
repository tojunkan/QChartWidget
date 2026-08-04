// QRegionSeries.h —— 填充区域系列
// 数据点构成封闭多边形（隐式闭合：末点连回起点），fill。
// 只存 Data，绘制用注入的 toPixel。不知道 Axis/Projection 类型。
#pragma once
#include "QXYSeries.h"

class QRegionSeries : public QXYSeries
{
    Q_OBJECT
public:
    explicit QRegionSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 绘制：多边形 fill（隐式闭合）=====
    void draw(QPainter* painter,
              std::function<QPointF(QVariant,QVariant)> toPixel) const override;

    // ===== 命中检测：像素在多边形内部 =====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel) const override;

    // ===== 样式 =====
    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor& c) { m_fillColor = c; }
    Qt::FillRule fillRule() const { return m_fillRule; }
    void setFillRule(Qt::FillRule r) { m_fillRule = r; }
    bool strokeVisible() const { return m_strokeVisible; }
    void setStrokeVisible(bool v) { m_strokeVisible = v; }

private:
    QColor m_fillColor;              // 空 = 用系列 color 的半透明版本
    Qt::FillRule m_fillRule = Qt::OddEvenFill;
    bool m_strokeVisible = false;    // 是否描边
};
