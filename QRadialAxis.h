#pragma once
#ifndef QRADIALAXIS_H
#define QRADIALAXIS_H

#include "QChartAxis.h"

class QRadialAxis : public QChartAxis {
    Q_OBJECT

public:
    explicit QRadialAxis(QObject* parent = nullptr);

    CoordinateSystem coordinateSystem() const override;
    // ---------- 径向缩放（忽略 centerNorm，从原点均匀缩放） ----------
    void zoom(qreal centerNorm, qreal factor) override;

    // ---------- 坐标映射（线性） ----------
    qreal valueToNormalized(qreal value) const override;
    qreal normalizedToValue(qreal norm) const override;

    // ---------- 刻度 ----------
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
    QVector<qreal> subTickValues() const override;

    // ---------- 绘制 ----------
    void draw(QPainter* painter, const QRectF& plotArea, const QChartProjection* projection) const override;
};

#endif // QRADIALAXIS_H
