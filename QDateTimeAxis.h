#pragma once
#ifndef QDATETIMEAXIS_H
#define QDATETIMEAXIS_H

#include "QChartAxis.h"
#include "QChartProjection.h"
#include <QDateTime>

class QDateTimeAxis : public QChartAxis {
    Q_OBJECT
        Q_PROPERTY(QString format READ format WRITE setFormat)

public:
    explicit QDateTimeAxis(QObject* parent = nullptr,
        Qt::Alignment alignment = Qt::AlignBottom);

    CoordinateSystem coordinateSystem() const override;

    // ---------- 业务层便利接口（非虚，直接转换） ----------
    void setRange(const QDateTime& min, const QDateTime& max);
    QDateTime dateTimeMin() const;
    QDateTime dateTimeMax() const;

    // ---------- 重写基类接口（增加防负数保护） ----------
    void setMin(qreal v) override;
    void setMax(qreal v) override;

    // ---------- 格式管理 ----------
    QString format() const { return m_format; }
    void setFormat(const QString& f) { m_format = f; }

    // ---------- 核心映射（直接操作基类的 m_min/m_max） ----------
    qreal valueToNormalized(qreal value) const override;
    qreal normalizedToValue(qreal norm) const override;

    // ---------- 刻度生成 ----------
    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;
    QVector<qreal> subTickValues() const override;

    // ---------- 工具函数 ----------
    static qreal toEpoch(const QDateTime& dt) {
        return qreal(dt.toSecsSinceEpoch());
    }

private:
    // 内部辅助结构
    struct TimeStepInfo {
        qreal stepSeconds;   // 步长（秒）
        QString format;      // 对应的显示格式
        int subDivisions;    // 建议的次刻度数
    };

    TimeStepInfo calculateStepInfo() const;
    static QDateTime floorToStep(const QDateTime& dt, qreal stepSeconds);

private:
    QString m_format;        // 只存格式，不存日期对象
};

#endif // QDATETIMEAXIS_H