// QPolygonSeries.h —— 多边形系列
// 数据点构成封闭多边形顶点环，绘制时隐式闭合（末点连回起点）并 fill。
// 每边用 near/far 策略：近点像素直线，远点 Numeric Lerp→createPath 曲线。
// 只存 Data，不知道 Axis/Projection 类型。
#pragma once
#include "QXYSeries.h"

class QPolygonSeries : public QXYSeries
{
    Q_OBJECT
public:
    explicit QPolygonSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 绘制：曲线边 + fill ====
    void draw(QPainter* painter,
              std::function<QPointF(QVariant,QVariant)> toPixel,
              const DrawContext* ctx = nullptr) const override;

    // ===== 命中检测：像素在多边形内部 ====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel,
                const DrawContext* ctx = nullptr) const override;

    // ===== 样式 ====
    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor& c) { m_fillColor = c; }
    Qt::FillRule fillRule() const { return m_fillRule; }
    void setFillRule(Qt::FillRule r) { m_fillRule = r; }
    bool strokeVisible() const { return m_strokeVisible; }
    void setStrokeVisible(bool v) { m_strokeVisible = v; }

private:
    QColor m_fillColor;              // 空 = 用系列 color 的半透明版本
    Qt::FillRule m_fillRule = Qt::OddEvenFill;
    bool m_strokeVisible = false;
};
