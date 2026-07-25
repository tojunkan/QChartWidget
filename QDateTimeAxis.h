#pragma once
#ifndef QDATETIMEAXIS_H
#define QDATETIMEAXIS_H

#include "QChartAxis.h"
// ===== QDateTimeAxis =====
class QDateTimeAxis : public QChartAxis {
    Q_OBJECT
        Q_PROPERTY(QString format READ format WRITE setFormat)
public:
    explicit QDateTimeAxis(QObject* p = nullptr);
    QString format() const { return m_format; } void setFormat(const QString& f) { m_format = f; }
    void setRange(QDateTime min, QDateTime max);
    QDateTime dateTimeMin() const { return m_dtMin; } QDateTime dateTimeMax() const { return m_dtMax; }
    qreal mapToPixel(qreal v, qreal len) const override;
    qreal pixelToValue(qreal p, qreal len) const override;
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
    static qreal toEpoch(const QDateTime& dt) { return qreal(dt.toSecsSinceEpoch()); }
private: QString m_format = "yyyy-MM-dd"; QDateTime m_dtMin, m_dtMax;
};


#endif // !QDATETIMEAXIS_H