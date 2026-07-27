#pragma once
#ifndef QANGLEAXIS_H
#define QANGLEAXIS_H

#include "QChartAxis.h"

class QAngleAxis : public QChartAxis {
    Q_OBJECT

public:
    explicit QAngleAxis(QObject* parent = nullptr);

    CoordinateSystem coordinateSystem() const override;

    // ---------- 坐标映射（线性） ----------
    qreal valueToNormalized(qreal value) const override;
    qreal normalizedToValue(qreal norm) const override;

    // ---------- 刻度 ----------
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
    QVector<qreal> subTickValues() const override;
};

#endif // QANGLEAXIS_H
