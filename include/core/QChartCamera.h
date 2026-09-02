// QChartCamera.h —— 2D 相机（退化特例）
#ifndef QCHARTCAMERA_H
#define QCHARTCAMERA_H

#include "QChartAbstractCamera.h"
#include <QRectF>
#include <QPointF>

enum class ViewRectFitMode {
    Stretch,
    Expand,
    Crop,
    Preserve
};

enum class FitStrategy {
    KeepCenter,
    KeepTopLeft,
    KeepTopRight,
    KeepBottomLeft,
    KeepBottomRight,
    KeepTop,
    KeepBottom,
    KeepLeft,
    KeepRight
};

class QChartCamera : public QChartAbstractCamera {
    Q_OBJECT
    Q_PROPERTY(QRectF viewRect READ viewRect WRITE setViewRect NOTIFY viewChanged)
    Q_PROPERTY(QPointF center READ center WRITE setCenter NOTIFY viewChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)

public:
    explicit QChartCamera(QObject* parent = nullptr);

    // ---- 2D 特有状态 ----
    QRectF viewRect() const { return m_viewRect; }
    void setViewRect(const QRectF& r);

    QPointF center() const { return m_viewRect.center(); }
    void setCenter(const QPointF& c);

    qreal zoom() const { return m_viewRect.width(); }
    void setZoom(qreal z);

    // ---- 交互操作 ----
    void panViewCartesian(qreal dx, qreal dy);
    void zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY);

    // ---- fit 相关 ----
    bool fitToPlotArea(const QRectF& plotArea) override;
    void setFitMode(ViewRectFitMode mode) { m_fitMode = mode; }
    ViewRectFitMode fitMode() const { return m_fitMode; }
    void setFitStrategy(FitStrategy strategy) { m_fitStrategy = strategy; }
    FitStrategy fitStrategy() const { return m_fitStrategy; }
    void setScale(qreal ratio) { m_scale = ratio; }
    qreal scale() const { return m_scale; }

    // ---- 实现统一矩阵接口 ----
    QMatrix4x4 viewMatrix() const override;
    QMatrix4x4 projectionMatrix(qreal aspect) const override;
    QChartProjectedPoint project(const QVector3D& cart, const QRectF& plotArea) const override;
    Ray unproject(const QPointF& pixel, const QRectF& plotArea) const override;

private:
    QRectF m_viewRect;
    ViewRectFitMode m_fitMode = ViewRectFitMode::Preserve;
    FitStrategy m_fitStrategy = FitStrategy::KeepCenter;
    qreal m_scale = 1.0;
};

#endif // QCHARTCAMERA_H