#pragma once
#ifndef QVALUEAXIS_H
#define QVALUEAXIS_H

#include "QChartAxis.h"

class QValueAxis : public QChartAxis {
    Q_OBJECT
        Q_PROPERTY(qreal tickInterval READ tickInterval WRITE setTickInterval)
        Q_PROPERTY(int labelDecimals READ labelDecimals WRITE setLabelDecimals)
        Q_PROPERTY(QString labelFormat READ labelFormat WRITE setLabelFormat)

public:
    explicit QValueAxis(QObject* p = nullptr, Qt::Alignment alignment = Qt::AlignBottom);

    CoordinateSystem coordinateSystem() const override;

    // 坐标映射
    qreal valueToNormalized(qreal value) const override;
    qreal normalizedToValue(qreal norm) const override;

    // 刻度生成（核心）
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;

    // 次刻度生成（补上！）
    QVector<qreal> subTickValues() const override;
    QStringList subTickLabels() const override; // 虽然是普通虚函数，但数值轴可以重写（虽然返回空）

    // 数值轴特有接口
    qreal tickInterval() const { return m_tickInterval; }
    void setTickInterval(qreal v);

    int labelDecimals() const { return m_labelDecimals; }
    void setLabelDecimals(int n) { m_labelDecimals = n; }

    QString labelFormat() const { return m_labelFormat; }
    void setLabelFormat(const QString& f) { m_labelFormat = f; }

private:
    qreal niceStep(qreal r) const;

    qreal m_tickInterval = 0;
    int m_labelDecimals = -1;
    QString m_labelFormat;
};

#endif // !QVALUEAXIS_H