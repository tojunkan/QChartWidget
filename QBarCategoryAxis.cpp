#include "QBarCategoryAxis.h"
#include <QtMath>

QBarCategoryAxis::QBarCategoryAxis(QObject* parent, Qt::Alignment alignment)
    : QChartAxis(parent, alignment)
{
    m_min = 0.0;
    m_max = 0.0;
    setZoomEnabled(false);
    setDragEnabled(false);
}

CoordinateSystem QBarCategoryAxis::coordinateSystem() const { return CoordinateSystem::Cartesian; }

// ===== 内部更新（只在这里改 m_min/m_max 并发信号） =====
// ===== 修改 updateRange：自动向外扩展 0.5 =====
void QBarCategoryAxis::updateRange() {
    if (m_categories.isEmpty()) {
        m_min = -0.5;
        m_max = 0.5;
    }
    else {
        // 核心改动：索引范围是 [0, N-1]，但数据映射范围扩展为 [-0.5, N-0.5]
        // 这样第一个柱子中心在 0，边缘在 -0.5；最后一个柱子中心在 N-1，边缘在 N-0.5
        m_min = -0.5;
        m_max = static_cast<qreal>(m_categories.size() - 1) + 0.5;
    }
    emit rangeChanged(m_min, m_max);
}
//
//// ===== valueToNormalized 完全不用改（自动继承新范围） =====
//qreal QBarCategoryAxis::valueToNormalized(qreal value) const {
//    if (m_categories.isEmpty() || qFuzzyCompare(m_max, m_min)) return 0.0;
//    // 此时如果传入 value=0，得到 (0 - (-0.5)) / (N-0.5+0.5) = 0.5/N
//    // 正好在画布上留出 1/(2N) 的边距
//    return (value - m_min) / (m_max - m_min);
//}

// ===== 全量替换（用户做 removeIf / move / swap 等复杂操作后调用） =====
void QBarCategoryAxis::setCategories(const QStringList& categories) {
    if (m_categories == categories) return;
    m_categories = categories;
    updateRange();
}

// ===== 原子操作 =====
void QBarCategoryAxis::append(const QString& category) {
    m_categories.append(category);
    updateRange();
}

void QBarCategoryAxis::insert(int index, const QString& category) {
    if (index < 0 || index > m_categories.size()) return;
    m_categories.insert(index, category);
    updateRange();
}

void QBarCategoryAxis::removeAt(int index) {
    if (index < 0 || index >= m_categories.size()) return;
    m_categories.removeAt(index);
    updateRange();
}

void QBarCategoryAxis::clear() {
    if (m_categories.isEmpty()) return;
    m_categories.clear();
    updateRange();
}

// ===== 重写 setMin/Max（限制索引范围） =====
void QBarCategoryAxis::setMin(qreal v) {
    Q_UNUSED(v);
    // 分类轴不接受自定义最小值，永远保持自动计算
    updateRange();
}

void QBarCategoryAxis::setMax(qreal v) {
    Q_UNUSED(v);
    updateRange();
}

// ===== 映射 =====
qreal QBarCategoryAxis::valueToNormalized(qreal value) const {
    if (m_categories.isEmpty() || qFuzzyCompare(m_max, m_min)) return 0.0;
    return (value - m_min) / (m_max - m_min);
}

qreal QBarCategoryAxis::normalizedToValue(qreal norm) const {
    if (m_categories.isEmpty()) return 0.0;
    return m_min + norm * (m_max - m_min);
}

// ===== 刻度生成 =====
QVector<qreal> QBarCategoryAxis::tickValues() const {
    QVector<qreal> ticks;
    if (m_categories.isEmpty()) return ticks;

    int start = qRound(m_min);
    int end = qRound(m_max);
    if (start < 0) start = 0;
    if (end >= m_categories.size()) end = m_categories.size() - 1;

    for (int i = start; i <= end; ++i) {
        ticks.append(static_cast<qreal>(i));
    }
    return ticks;
}

QStringList QBarCategoryAxis::tickLabels() const {
    QStringList labels;
    for (qreal v : tickValues()) {
        int idx = qRound(v);
        if (idx >= 0 && idx < m_categories.size())
            labels.append(m_categories[idx]);
    }
    return labels;
}