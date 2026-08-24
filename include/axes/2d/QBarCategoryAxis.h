// QBarCategoryAxis.h —— 分类轴
// Data = QString（类别名："苹果"、"香蕉"）
// Numeric = 类别索引（0, 1, 2, ... ±0.5 为中心对齐）
// 无次刻度、无语法糖 setRange（类别由数据决定）
#pragma once
#include "QChartAxis.h"
#include <QStringList>

class QBarCategoryAxis : public QChartAxis {
    Q_OBJECT
    Q_PROPERTY(QStringList categories READ categories WRITE setCategories)

public:
    explicit QBarCategoryAxis(QObject* parent = nullptr,
                              Qt::Alignment alignment = Qt::AlignBottom);

    // ===== 类别管理 =====
    QStringList categories() const { return m_categories; }
    void setCategories(const QStringList& cats);
    void appendCategory(const QString& cat);
    void insertCategory(int index, const QString& cat);
    void removeCategory(int index);
    void clearCategories();

    // 离散类别域没有平移/缩放意义（会撕裂标签与 bar 的对应关系）
    bool isInteractive() const override { return false; }

    // ===== 数值化：Data(QString/qreal) ↔ Numeric(线性映射) =====
    qreal toNumeric(QVariant data) const override {
        // qreal 直通——Bar 等用小数索引定位
        if (static_cast<QMetaType::Type>(data.typeId()) == QMetaType::Double
            || static_cast<QMetaType::Type>(data.typeId()) == QMetaType::Int
            || static_cast<QMetaType::Type>(data.typeId()) == QMetaType::LongLong)
            return data.toDouble();
        // 类别名 → 索引 → 线性映射到 [numericMin, numericMax]
        int idx = m_categories.indexOf(data.toString());
        if (idx < 0 || m_categories.isEmpty()) return qQNaN();
        // 线性映射：旧域[0,n-1] → 新域[numericMin,numericMax]
        qreal norm = static_cast<qreal>(idx) / (m_categories.size() - 1);
        return m_numericMin + norm * (m_numericMax - m_numericMin);
    }
    QVariant fromNumeric(qreal num) const override {
        // Numeric → 逆线性映射 → 索引 → 类别名
        if (!std::isfinite(num) || m_categories.isEmpty()) return QString();
        qreal norm = (num - m_numericMin) / (m_numericMax - m_numericMin);
        int idx = static_cast<int>(std::round(norm * (m_categories.size() - 1)));
        return (idx >= 0 && idx < m_categories.size()) ? m_categories[idx] : QString();
    }

    // ===== 线性映射区间 =====
    void setNumericMapping(qreal numericMin, qreal numericMax);
    qreal numericMin() const { return m_numericMin; }
    qreal numericMax() const { return m_numericMax; }

    // ===== 刻度生成 =====
    QVector<qreal> tickValues(qreal numericMin, qreal numericMax) const override;
    QStringList tickLabels(const QVector<qreal>& ticks) const override;
    QVector<qreal> subTickValues(qreal, qreal) const override { return {}; }

    // 语法糖 setRange 无效（类别由数据决定）
    void setRange(qreal, qreal) {
        qWarning() << "QBarCategoryAxis::setRange ignored — range is determined by categories";
    }

private:
    QStringList m_categories;
    qreal m_numericMin = 0.0;
    qreal m_numericMax = 0.0;  // setCategories 时自动更新
};
