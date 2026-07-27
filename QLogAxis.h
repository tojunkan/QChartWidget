#pragma once
#ifndef QLOGAXIS_H
#define QLOGAXIS_H

#include "QChartAxis.h"

class QLogAxis : public QChartAxis {
    Q_OBJECT
    Q_PROPERTY(qreal base READ base WRITE setBase)

public:
    explicit QLogAxis(QObject* parent = nullptr,
                      Qt::Alignment alignment = Qt::AlignBottom);

    CoordinateSystem coordinateSystem() const override;

    qreal base() const { return m_base; }
    void setBase(qreal b);

    virtual void pan(qreal deltaNorm) override;
    virtual void zoom(qreal centerNorm, qreal factor) override;

    // ---------- 坐标映射（对数） ----------
    qreal valueToNormalized(qreal value) const override;
    qreal normalizedToValue(qreal norm) const override;

    // ---------- 刻度 ----------
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
    QVector<qreal> subTickValues() const override;

private:
    qreal m_base = 10.0;
};

#endif // QLOGAXIS_H
