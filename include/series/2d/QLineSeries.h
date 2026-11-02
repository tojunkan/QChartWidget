// QLineSeries.h —— 折线系列
// 数据点按数据顺序用 **Numeric 空间的直线段**连接；NaN/Inf 点断开路径
// （y=1/x 这类多分支仍是**同一系列**，断线只是不连那一段）。
// 只存 Data，收集时由 layer 传入轴指针做 Data→Numeric；投影/相机由 renderer 完成。
//
// TODO(line-style)：lineStyle（虚线）/ smooth（Catmull-Rom）本轮只保留字段，图元暂无对应属性。
// TODO(nonlinear)：Numeric 直线在 Cartesian 空间的插值由 renderer 负责（当前先用端点直连的权宜做法）。
// TODO(adaptive)：自适应细分（按 Cartesian 距离相对 viewRect 的阈值）—— GL 侧暂不支持，搁置。
#pragma once
#include "QXYSeries.h"

class QLineSeries : public QXYSeries
{
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(bool smooth READ smooth WRITE setSmooth NOTIFY smoothChanged)

public:
    explicit QLineSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 图元收集：相邻有效点 → 一条 Line（Numeric 直线段）；NaN/Inf 断线 =====
    void collectPrimitives(QChartScene& scene,
                           const QChartAxis* ax, const QChartAxis* ay,
                           const QChartAxis* az = nullptr) const override;

    // ===== 命中检测（旧签名保留，拾取轮随 QChartHitTester 一起替换）=====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel,
                const struct DrawContext* ctx = nullptr) const override;

    // ===== 线样式（NOTIFY 供 QPropertyAnimation 驱动时实时刷新）=====
    qreal lineWidth() const { return m_lineWidth; }
    void setLineWidth(qreal w) {
        w = qMax(0.5, w);
        if (qFuzzyCompare(m_lineWidth, w)) return;
        m_lineWidth = w;
        emit lineWidthChanged();
    }
    Qt::PenStyle lineStyle() const { return m_lineStyle; }
    void setLineStyle(Qt::PenStyle s) { m_lineStyle = s; }   // TODO：图元无 pen style，暂未生效
    bool smooth() const { return m_smooth; }
    void setSmooth(bool s) {
        if (m_smooth == s) return;
        m_smooth = s;
        emit smoothChanged();
    }                                                         // TODO：Numeric 直线语义下暂未生效
    bool isCullingEnabled() const { return m_cullingEnabled; }
    void setCullingEnabled(bool v) { m_cullingEnabled = v; }  // TODO：renderer 裁剪待接

signals:
    void lineWidthChanged();
    void smoothChanged();

private:
    qreal m_lineWidth = 2.0;
    Qt::PenStyle m_lineStyle = Qt::SolidLine;   // TODO：留后门，暂未参与收集
    bool m_smooth = false;                      // TODO：留后门，暂未参与收集
    bool m_cullingEnabled = true;               // TODO：留后门，暂未参与收集
};
