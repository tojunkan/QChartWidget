// QLineSeries.h —— 折线系列
// 数据点依次连接成折线。NaN 点断开路径（处理奇点）。
// 只存 Data，绘制用注入的 toPixel。不知道 Axis/Projection 类型。
#pragma once
#include "QXYSeries.h"

class QLineSeries : public QXYSeries
{
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth)
    Q_PROPERTY(bool smooth READ smooth WRITE setSmooth)

public:
    explicit QLineSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 绘制：逐点连折线，NaN 断开 =====
    void draw(QPainter* painter,
              std::function<QPointF(QVariant,QVariant)> toPixel) const override;

    // ===== 命中检测：像素到最近线段的垂直距离 < 阈值 =====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel) const override;

    // ===== 线样式 =====
    qreal lineWidth() const { return m_lineWidth; }
    void setLineWidth(qreal w) { m_lineWidth = qMax(0.5, w); }
    Qt::PenStyle lineStyle() const { return m_lineStyle; }
    void setLineStyle(Qt::PenStyle s) { m_lineStyle = s; }
    bool smooth() const { return m_smooth; }
    void setSmooth(bool s) { m_smooth = s; }

private:
    // 平滑路径：Catmull-Rom 采样
    QPainterPath smoothPath(const QVector<QPointF>& pts) const;

    qreal m_lineWidth = 2.0;
    Qt::PenStyle m_lineStyle = Qt::SolidLine;
    bool m_smooth = false;
};
