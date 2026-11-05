// QPolygonSeries.h —— 多边形系列（闭合填充）
// 数据点构成闭环（隐式闭合：末点连回起点）；NaN/Inf 断开 → 每段一个独立多边形。
// 上传哪些图元由 **series** 决定：
//   填充 → 一个 Polygon 图元（fillColor，renderer 负责三角化）；
//   描边 → 额外一条闭合 Path 图元（color + penWidth）。
// TODO(fill-style)：渐变/图案/纹理填充（以后扩图元字段或程序化着色器）。
// TODO(stroke-width)：描边宽度暂固定 1.0。
// TODO(holes)：带洞（多环）暂不支持。
#pragma once
#include "QXYSeries.h"

class QPolygonSeries : public QXYSeries
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY fillColorChanged)
    Q_PROPERTY(bool strokeVisible READ strokeVisible WRITE setStrokeVisible NOTIFY strokeChanged)
public:
    explicit QPolygonSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 图元收集：每段有效顶点 → Polygon（填充）；strokeVisible 时再补一条闭合 Path =====
    void collectPrimitives(QChartSegment& seg,
                           const QChartAxis* ax, const QChartAxis* ay,
                           const QChartAxis* az = nullptr) const override;

    // ===== 命中检测（旧签名保留，拾取轮随 QChartHitTester 一起替换）=====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel,
                const struct DrawContext* ctx = nullptr) const override;

    // ===== 样式 =====
    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor& c);
    bool strokeVisible() const { return m_strokeVisible; }
    void setStrokeVisible(bool v);

signals:
    void fillColorChanged();
    void strokeChanged();

private:
    QColor m_fillColor;               // alpha==0 → 用系列 color 的半透明版本
    bool m_strokeVisible = false;     // 是否额外发一条闭合描边 Path
};
