#include "QRadialAxis.h"

// === QRadialAxis ===
QRadialAxis::QRadialAxis(QObject* p) :QChartAxis(p) { m_min = 0;m_max = 1; }
qreal QRadialAxis::mapToPixel(qreal v, qreal l)const { return qFuzzyCompare(m_max, m_min) ? 0 : (v - m_min) / (m_max - m_min) * l; }
qreal QRadialAxis::pixelToValue(qreal p, qreal l)const { return l > 0 ? m_min + p / l * (m_max - m_min) : m_min; }
QVector<qreal> QRadialAxis::tickValues()const { qreal s = (m_max - m_min) / m_tickCount;QVector<qreal> t;for (int i = 0;i <= m_tickCount;++i)t.append(m_min + i * s);return t; }
QStringList QRadialAxis::tickLabels()const { QStringList l;for (qreal v : tickValues())l.append(QString::number(v, 'f', 1));return l; }
