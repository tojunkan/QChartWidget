#include "QDateTimeAxis.h"

// === QDateTimeAxis ===
QDateTimeAxis::QDateTimeAxis(QObject* p) :QChartAxis(p) { m_dtMin = QDateTime::currentDateTime();m_dtMax = m_dtMin.addSecs(86400);m_min = toEpoch(m_dtMin);m_max = toEpoch(m_dtMax); }
void QDateTimeAxis::setRange(QDateTime min, QDateTime max) { m_dtMin = min;m_dtMax = max;m_min = toEpoch(min);m_max = toEpoch(max);emit rangeChanged(m_min, m_max); }
qreal QDateTimeAxis::mapToPixel(qreal v, qreal l)const { return qFuzzyCompare(m_max, m_min) ? 0 : (v - m_min) / (m_max - m_min) * l; }
qreal QDateTimeAxis::pixelToValue(qreal p, qreal l)const { return l > 0 ? m_min + p / l * (m_max - m_min) : m_min; }
QVector<qreal> QDateTimeAxis::tickValues()const { QValueAxis h;h.setMin(m_min);h.setMax(m_max);h.setTickCount(m_tickCount);return h.tickValues(); }
QStringList QDateTimeAxis::tickLabels()const { QStringList l;for (qreal v : tickValues()) { qint64 s = qint64(v);l.append(QDateTime::fromSecsSinceEpoch(s).toString(m_format)); }return l; }
