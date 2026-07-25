#include "QBarCagegoryAxis.h"

// === QBarCategoryAxis ===
QBarCategoryAxis::QBarCategoryAxis(QObject* p) :QChartAxis(p) { m_min = 0;m_max = 1; }
qreal QBarCategoryAxis::mapToPixel(qreal v, qreal l)const { int n = m_categories.size();return n > 0 ? (v + .5) * l / n : 0; }
qreal QBarCategoryAxis::pixelToValue(qreal p, qreal l)const { int n = m_categories.size();return n > 0 ? std::floor(p / (l / n)) : 0; }
void QBarCategoryAxis::setCategories(const QStringList& c) { m_categories = c;m_max = qMax(1, c.size());emit categoriesChanged(); }
void QBarCategoryAxis::append(const QString& c) { m_categories.append(c);m_max = m_categories.size();emit categoriesChanged(); }
void QBarCategoryAxis::insert(int i, const QString& c) { m_categories.insert(i, c);m_max = m_categories.size();emit categoriesChanged(); }
void QBarCategoryAxis::remove(const QString& c) { m_categories.removeAll(c);m_max = qMax(1, m_categories.size());emit categoriesChanged(); }
void QBarCategoryAxis::clear() { m_categories.clear();m_max = 1;emit categoriesChanged(); }
QVector<qreal> QBarCategoryAxis::tickValues()const { QVector<qreal> t;for (int i = 0;i < m_categories.size();++i)t.append(i);return t; }
QStringList QBarCategoryAxis::tickLabels()const { return m_categories; }

