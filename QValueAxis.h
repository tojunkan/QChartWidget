// QValueAxis.h —— 数值轴
// 最常用的线性数值轴。toNumeric/fromNumeric 为恒等映射。
// 刻度使用 niceStep 算法自动选择"漂亮"的步长（1, 2, 5, 10 × 10^n）
// 可通过 setLabelFormat 自定义标签格式（如 "%g°" 用于角度）
#pragma once
#ifndef QVALUEAXIS_H
#define QVALUEAXIS_H

#include "QChartAxis.h"

class QValueAxis : public QChartAxis {
    Q_OBJECT
    Q_PROPERTY(qreal tickInterval READ tickInterval WRITE setTickInterval)
    Q_PROPERTY(int labelPrecision READ labelPrecision WRITE setLabelPrecision)
    Q_PROPERTY(QString labelFormat READ labelFormat WRITE setLabelFormat)

public:
    explicit QValueAxis(QObject* parent = nullptr,
                        Qt::Alignment alignment = Qt::AlignBottom);

    // ===== 数值化：恒等 =====
    /// Data(qreal) → Numeric: data.toDouble()
    qreal toNumeric(QVariant data) const override;
    /// Numeric → Data(qreal): 直通，NaN→NaN, Inf→Inf
    QVariant fromNumeric(qreal num) const override;

    // ===== 刻度生成：niceStep 算法 =====
    QVector<qreal> tickValues(qreal numericMin, qreal numericMax) const override;
    QStringList tickLabels(const QVector<qreal>& ticks) const override;
    QVector<qreal> subTickValues(qreal numericMin, qreal numericMax) const override;

    // ===== 标签格式 =====
    /// 设置 printf 风格格式串。"%g°"→带后缀, "%.2f"→固定精度, ""→自动去零
    void setLabelFormat(const QString& fmt) { m_labelFormat = fmt; }
    QString labelFormat() const { return m_labelFormat; }

    /// 小数位数，-1 表示自动。setLabelFormat 优先级高于此字段
    int labelPrecision() const { return m_labelPrecision; }
    void setLabelPrecision(int n) { m_labelPrecision = n; }

    // ===== 固定步长 =====
    qreal tickInterval() const { return m_tickInterval; }
    void setTickInterval(qreal v);

private:
    /// niceStep 核心算法：给定范围，返回"漂亮"的步长
    qreal niceStep(qreal range) const;

    qreal m_tickInterval = 0.0;     // 0 = 自动计算步长
    int m_labelPrecision = -1;      // -1 = 自动去零
    QString m_labelFormat;          // 空 = 不使用 printf 格式
};

#endif // QVALUEAXIS_H
