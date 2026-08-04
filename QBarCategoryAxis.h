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

    // ===== 数值化：Data(QString) ↔ Numeric(索引) =====
    qreal toNumeric(QVariant data) const override {
        // 类别名 → 索引；未找到返回 NaN
        int idx = m_categories.indexOf(data.toString());
        return (idx >= 0) ? static_cast<qreal>(idx) : qQNaN();
    }
    QVariant fromNumeric(qreal num) const override {
        // 索引 → 类别名；NaN/越界返回空字符串
        if (!std::isfinite(num)) return QString();
        int idx = static_cast<int>(num);
        return (idx >= 0 && idx < m_categories.size()) ? m_categories[idx] : QString();
    }

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
};
