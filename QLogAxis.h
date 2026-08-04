// QLogAxis.h —— 对数轴
// Data = qreal（> 0）
// Numeric = log10(v)（纯数字，喂给 Projection）
// tickValues 在 log 空间跨越数量级
#pragma once
#include "QChartAxis.h"

class QLogAxis : public QChartAxis {
    Q_OBJECT
    Q_PROPERTY(qreal base READ base WRITE setBase)

public:
    explicit QLogAxis(QObject* parent = nullptr,
                      Qt::Alignment alignment = Qt::AlignBottom);

    // ===== 数值化：Data(qreal>0) ↔ Numeric(log10) =====
    qreal toNumeric(QVariant data) const override {
        qreal v = data.toDouble();
        return (v > 0) ? std::log10(v) : qQNaN(); // ≤0 → NaN
    }
    QVariant fromNumeric(qreal num) const override {
        return QVariant::fromValue(std::pow(10.0, num));
    }

    // ===== 刻度生成 =====
    QVector<qreal> tickValues(qreal numericMin, qreal numericMax) const override;
    QStringList tickLabels(const QVector<qreal>& ticks) const override;
    QVector<qreal> subTickValues(qreal numericMin, qreal numericMax) const override;

    // ===== 底数 =====
    qreal base() const { return m_base; }
    void setBase(qreal b);

private:
    qreal m_base = 10.0; // log10 底数（暂仅支持 10）
};
