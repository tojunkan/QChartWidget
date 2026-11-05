// QBarSeries.h —— 柱状系列
// 每个柱 = Data 空间的一个矩形（left, bottom, right, top）。
// 只存 Data；收集时由 layer 传入轴指针做四边 Data→Numeric，出一个 **Rect** 图元。
// 投影相关的快路径由 renderer 决定（恒等投影 → 轴对齐矩形快速绘制；非线性 → 转 Polygon），
// series 只负责选"上传哪个图元"。
// TODO(stroke-style)：pen 的线型（虚线边框）图元暂无对应字段。
// TODO(animation)：renderOverride（Numeric 矩形覆盖层）暂不参与收集。
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

    // ===== 图元收集：每个矩形 → 一个 Rect 图元（Numeric 轴对齐）；非法四边跳过 =====
    void collectPrimitives(QChartSegment& seg,
                           const QChartAxis* ax, const QChartAxis* ay,
                           const QChartAxis* az = nullptr) const override;

    // ===== 命中检测（旧签名保留，拾取轮随 QChartHitTester 一起替换）=====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel,
                const struct DrawContext* ctx = nullptr) const override;

    // ===== 动画覆盖层 —— 优先于真实数据（TODO：暂未参与收集）=====
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
    void renderOverrideChanged();   // 动画覆盖层变化（每帧可能多次）

private:
    QVector<QDataRect> m_rects;      // Data 空间矩形
    QVector<QRectF> m_overrideRects; // 动画临时矩形（Numeric 空间），优先于 m_rects
    bool m_hasOverride = false;
    QColor m_fillColor;              // 空 = 用系列色
    QPen m_pen = Qt::NoPen;
};
