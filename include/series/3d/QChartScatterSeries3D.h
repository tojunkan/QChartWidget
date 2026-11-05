// QChartScatterSeries3D.h —— 3D 散点系列
// collectPrimitives：每点一个 Point 图元（Numeric 三元组；坐标由传入的三根轴 toNumeric 得到）。
// 投影/相机由 renderer 统一完成（与 2D 同构）。
#pragma once
#include "QChartSeries3D.h"

class QChartScatterSeries3D : public QChartSeries3D {
    Q_OBJECT
    Q_PROPERTY(qreal markerSize READ markerSize WRITE setMarkerSize NOTIFY markerSizeChanged)
public:
    explicit QChartScatterSeries3D(const QString& name = {}, QObject* parent = nullptr);

    qreal markerSize() const { return m_markerSize; }   // 默认 4.0（px）
    void setMarkerSize(qreal s);

    void collectPrimitives(QChartSegment& seg,
                           const QChartAxis* ax, const QChartAxis* ay,
                           const QChartAxis* az = nullptr) const override;

signals:
    void markerSizeChanged();

private:
    qreal m_markerSize = 4.0;
};
