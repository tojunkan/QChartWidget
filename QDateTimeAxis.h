// QDateTimeAxis.h —— 日期时间轴
// Data = QDateTime（用户领域类型）
// Numeric = msecsSinceEpoch（纯数字，喂给 Projection）
// tickValues 自动选择合适的时间单位（年/月/天/时/分/秒）
#pragma once
#include "QChartAxis.h"
#include <QDateTime>

class QDateTimeAxis : public QChartAxis {
    Q_OBJECT
    Q_PROPERTY(QString format READ format WRITE setFormat)

public:
    explicit QDateTimeAxis(QObject* parent = nullptr,
                           Qt::Alignment alignment = Qt::AlignBottom);

    // ===== 数值化：Data(QDateTime) ↔ Numeric(epoch ms) =====
    qreal toNumeric(QVariant data) const override {
        return static_cast<qreal>(data.toDateTime().toMSecsSinceEpoch());
    }
    QVariant fromNumeric(qreal num) const override {
        bool ok = false;
        qint64 ms = static_cast<qint64>(num);
        return QVariant::fromValue(QDateTime::fromMSecsSinceEpoch(ms));
    }

    // ===== 刻度生成：时间范围上自适应步长 =====
    QVector<qreal> tickValues(qreal numericMin, qreal numericMax) const override;
    QStringList tickLabels(const QVector<qreal>& ticks) const override;
    QVector<qreal> subTickValues(qreal numericMin, qreal numericMax) const override;

    // ===== 便捷：QDateTime 版 setRange =====
    void setRange(const QDateTime& min, const QDateTime& max) {
        QChartAxis::setRange(toNumeric(QVariant::fromValue(min)),
                             toNumeric(QVariant::fromValue(max)));
    }

    // ===== 标签格式 =====
    QString format() const { return m_format; }
    void setFormat(const QString& f) { m_format = f; }

private:
    struct TimeStepInfo {
        qint64 stepMs;          // 步长（毫秒）
        QString format;         // 对应的显示格式
        int subDivisions;       // 建议次刻度数
    };
    TimeStepInfo chooseStep(qint64 rangeMs) const;

    QString m_format;  // 用户自定义格式；空 = 自适应
};
