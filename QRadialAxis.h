#pragma once
#ifndef QRADIALAXIS_H
#define QRADIALAXIS_H

#include "QChartAxis.h"
// ===== QRadialAxis =====
class QRadialAxis : public QChartAxis {
    Q_OBJECT
public: explicit QRadialAxis(QObject* p = nullptr);
    qreal mapToPixel(qreal v, qreal len) const override;
    qreal pixelToValue(qreal p, qreal len) const override;
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
};
#endif // !QRADIALAXIS_H