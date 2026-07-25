#include "QLogAxis.h"

// === QLogAxis ===
QLogAxis::QLogAxis(QObject* p) :QChartAxis(p) { m_min = 1;m_max = 1000; }
void QLogAxis::setBase(qreal b) { if (b <= 1)return;m_base = b; }
qreal QLogAxis::mapToPixel(qreal v, qreal l)const { if (v <= 0 || m_min <= 0)return 0;qreal lm = std::log(m_min) / std::log(m_base), lM = std::log(m_max) / std::log(m_base), lv = std::log(v) / std::log(m_base);return qFuzzyCompare(lM, lm) ? 0 : (lv - lm) / (lM - lm) * l; }
qreal QLogAxis::pixelToValue(qreal p, qreal l)const { if (l <= 0 || m_min <= 0)return m_min;qreal lm = std::log(m_min) / std::log(m_base), lM = std::log(m_max) / std::log(m_base), lv = lm + p / l * (lM - lm);return std::pow(m_base, lv); }
QVector<qreal> QLogAxis::tickValues()const { QVector<qreal> t;if (m_min <= 0)return t;qreal lm = std::floor(std::log(m_min) / std::log(m_base)), lM = std::ceil(std::log(m_max) / std::log(m_base));for (qreal e = lm;e <= lM;e += 1.) { qreal v = std::pow(m_base, e);if (v >= m_min && v <= m_max)t.append(v); }return t; }
QStringList QLogAxis::tickLabels()const { QStringList l;for (qreal v : tickValues()) { if (v >= 1e6 || v <= 1e-3)l.append(QString::number(v, 'e', 1));else l.append(QString::number(v, 'g', 4)); }return l; }
