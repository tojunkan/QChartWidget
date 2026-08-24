// QBarCategoryAxis.cpp —— 分类轴实现
#include "QBarCategoryAxis.h"
#include "QChartDebug.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logCategoryAxis, "chart.axis.category")

QBarCategoryAxis::QBarCategoryAxis(QObject* parent, Qt::Alignment alignment)
    : QChartAxis(parent, alignment) {}

// ===== 类别管理 =====
void QBarCategoryAxis::setCategories(const QStringList& cats) {
    m_categories = cats;
    int n = m_categories.size();
    m_numericMax = qMax(0.0, static_cast<qreal>(n - 1));
    m_sugarMin = -0.5;
    m_sugarMax = m_numericMax + 0.5;
    emit rangeChanged(m_sugarMin, m_sugarMax);
    emit styleChanged();
    qCDebug(logCategoryAxis) << "categories:" << cats
                             << "numeric range:[" << m_numericMin << "," << m_numericMax << "]";
}

void QBarCategoryAxis::setNumericMapping(qreal numericMin, qreal numericMax) {
    m_numericMin = numericMin;
    m_numericMax = numericMax;
    qCDebug(logCategoryAxis) << "numericMapping: [" << numericMin << "," << numericMax << "]";
    emit styleChanged();
}

void QBarCategoryAxis::appendCategory(const QString& cat) {
    m_categories.append(cat);
    m_sugarMax = m_categories.size() - 0.5;
    emit rangeChanged(m_sugarMin, m_sugarMax);
    emit styleChanged();
}

void QBarCategoryAxis::insertCategory(int index, const QString& cat) {
    if (index < 0 || index > m_categories.size()) {
        qWarning() << "QBarCategoryAxis::insertCategory: index out of range" << index;
        return;
    }
    m_categories.insert(index, cat);
    m_sugarMax = m_categories.size() - 0.5;
    emit rangeChanged(m_sugarMin, m_sugarMax);
    emit styleChanged();
}

void QBarCategoryAxis::removeCategory(int index) {
    if (index < 0 || index >= m_categories.size()) {
        qWarning() << "QBarCategoryAxis::removeCategory: index out of range" << index;
        return;
    }
    m_categories.removeAt(index);
    m_sugarMax = m_categories.size() - 0.5;
    emit rangeChanged(m_sugarMin, m_sugarMax);
    emit styleChanged();
}

void QBarCategoryAxis::clearCategories() {
    m_categories.clear();
    m_sugarMin = -0.5;
    m_sugarMax = -0.5;
    emit rangeChanged(m_sugarMin, m_sugarMax);
    emit styleChanged();
}

// ===== 刻度生成：每个类别位置一个 tick =====
QVector<qreal> QBarCategoryAxis::tickValues(qreal, qreal) const {
    QVector<qreal> ticks;
    for (int i = 0; i < m_categories.size(); ++i)
        ticks.append(static_cast<qreal>(i));
    return ticks;
}

QStringList QBarCategoryAxis::tickLabels(const QVector<qreal>& ticks) const {
    QStringList labels;
    for (qreal t : ticks) {
        int idx = static_cast<int>(t);
        labels.append((idx >= 0 && idx < m_categories.size()) ? m_categories[idx] : QString());
    }
    return labels;
}
