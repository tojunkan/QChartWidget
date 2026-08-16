// QLineSeries.h —— 折线系列
// 数据点依次连接成折线。NaN 点断开路径（处理奇点）。
// 只存 Data，绘制用注入的 toPixel。不知道 Axis/Projection 类型。
#pragma once
#include "QXYSeries.h"

class QLineSeries : public QXYSeries
{
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(bool smooth READ smooth WRITE setSmooth NOTIFY smoothChanged)

public:
    explicit QLineSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 绘制：逐点连折线。远点走 Numeric Lerp→createPath 曲线边 =====
    void draw(QPainter* painter,
              std::function<QPointF(QVariant,QVariant)> toPixel,
              const DrawContext* ctx = nullptr) const override;

    // ===== 命中检测 =====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel,
                const DrawContext* ctx = nullptr) const override;

    // ===== 线样式（NOTIFY 供 QPropertyAnimation 驱动时实时刷新）=====
    qreal lineWidth() const { return m_lineWidth; }
    void setLineWidth(qreal w) {
        w = qMax(0.5, w);
        if (qFuzzyCompare(m_lineWidth, w)) return;
        m_lineWidth = w;
        emit lineWidthChanged();
    }
    Qt::PenStyle lineStyle() const { return m_lineStyle; }
    void setLineStyle(Qt::PenStyle s) { m_lineStyle = s; }
    bool smooth() const { return m_smooth; }
    void setSmooth(bool s) {
        if (m_smooth == s) return;
        m_smooth = s;
        emit smoothChanged();
    }

signals:
    void lineWidthChanged();
    void smoothChanged();

private:
    // 平滑路径：Catmull-Rom 采样
    QPainterPath smoothPath(const QVector<QPointF>& pts) const;

    qreal m_lineWidth = 2.0;
    Qt::PenStyle m_lineStyle = Qt::SolidLine;
    bool m_smooth = false;
};
