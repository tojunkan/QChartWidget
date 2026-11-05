// QChartLineSeries3D.h —— 3D 折线系列
// 与 2D 同构：数据点按顺序用 **Numeric 空间的直线段**连接；任一端 NaN/Inf → 断段
// （y=1/x 这类多分支仍是同一系列）。投影/相机由 renderer 统一完成。
//
// TODO(line-style)：lineStyle / smooth 本轮只保留字段，未参与收集。
// TODO(nonlinear)：Numeric 直线在 Cartesian 空间的插值由 renderer 负责（当前端点直连）。
// TODO(adaptive)：自适应细分 —— GL 侧暂不支持，搁置。
#pragma once
#include "QChartSeries3D.h"

class QChartLineSeries3D : public QChartSeries3D {
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(bool cullingEnabled READ isCullingEnabled WRITE setCullingEnabled NOTIFY cullingChanged)
public:
    explicit QChartLineSeries3D(const QString& name = {}, QObject* parent = nullptr);

    qreal lineWidth() const { return m_lineWidth; }     // 默认 2.0
    void setLineWidth(qreal w);
    bool isCullingEnabled() const { return m_cullingEnabled; }  // TODO：留后门，暂未参与收集
    void setCullingEnabled(bool v);

    // ===== 图元收集：相邻有效点 → 一条 Line（Numeric 直线段）；NaN/Inf 断段 =====
    void collectPrimitives(QChartSegment& seg,
                           const QChartAxis* ax, const QChartAxis* ay,
                           const QChartAxis* az = nullptr) const override;

signals:
    void lineWidthChanged();
    void cullingChanged();

private:
    qreal m_lineWidth = 2.0;
    bool m_cullingEnabled = true;
};
