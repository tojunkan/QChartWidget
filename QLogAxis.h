#pragma once
#ifndef QLOGAXIS_H
#define QLOGAXIS_H

#include "QChartAxis.h"
// ===== QLogAxis =====
class QLogAxis : public QChartAxis {
    Q_OBJECT
        Q_PROPERTY(qreal base READ base WRITE setBase)
public:
    explicit QLogAxis(QObject* p = nullptr);
    qreal base() const { return m_base; } void setBase(qreal b);
    qreal mapToPixel(qreal v, qreal len) const override;
    qreal pixelToValue(qreal p, qreal len) const override;
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
private: qreal m_base = 10.0;
};


#endif // !QLOGAXIS_H