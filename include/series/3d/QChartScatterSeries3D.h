// QChartScatterSeries3D.h —— 3D 散点系列
// collectPrimitives：每点一个 Point 图元（dataIndex=点索引；投影 screen 非有限 → 跳过）
// draw：无排序直绘（收集后逐点画圆）
#pragma once
#include "QChartSeries3D.h"

class QChartScatterSeries3D : public QChartSeries3D {
    Q_OBJECT
    Q_PROPERTY(qreal markerSize READ markerSize WRITE setMarkerSize NOTIFY markerSizeChanged)
public:
    explicit QChartScatterSeries3D(const QString& name = {}, QObject* parent = nullptr);

    qreal markerSize() const { return m_markerSize; }   // 默认 4.0（px）
    void setMarkerSize(qreal s);

    void collectPrimitives(const ProjectFn3D& projectFn,
                           QVector<QChartPrimitive>& out) const override;
    void draw(QPainter* painter, const ProjectFn3D& projectFn,
              const DrawContext3D* ctx = nullptr) const override;

signals:
    void markerSizeChanged();

private:
    qreal m_markerSize = 4.0;
};
