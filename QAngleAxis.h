#pragma once
#ifndef QANGLEAXIS_H
#define QANGLEAXIS_H

#include "QChartAxis.h"
// ===== QAngleAxis =====
class QAngleAxis : public QChartAxis {
    Q_OBJECT
public: explicit QAngleAxis(QObject* p = nullptr);
    qreal mapToPixel(qreal v, qreal len) const override;
    qreal pixelToValue(qreal p, qreal len) const override;
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
};


#endif // !QANGLEAXIS_H