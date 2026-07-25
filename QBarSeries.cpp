#include "QBarSeries.h"
#include "QChartGeometry.h"
#include "QChartAxis.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <algorithm>

// ===== QBarSet =====
QBarSet::QBarSet(const QString& label, QObject* p) : QObject(p), m_label(label) {
    static const QList<QColor> pals{QColor("#2196F3"),QColor("#F44336"),QColor("#4CAF50"),QColor("#FF9800"),QColor("#9C27B0")};
    m_color = pals.at(qHash(label) % pals.size());
}
void QBarSet::setLabel(const QString& l){if(m_label!=l){m_label=l;emit labelChanged(l);}}
qreal QBarSet::valueAt(int i) const{return (i>=0&&i<m_values.size())?m_values[i]:0;}
void QBarSet::setValue(int i, qreal v){if(i>=m_values.size())m_values.resize(i+1);m_values[i]=v;emit valueChanged(i);emit valuesChanged();}
void QBarSet::setValues(const QVector<qreal>& v){m_values=v;emit valuesChanged();}
void QBarSet::setColor(const QColor& c){m_color=c;emit colorChanged(c);}

// ===== QBarSeries =====
QBarSeries::QBarSeries(const QString& name, QObject* parent)
    : QAbstractSeries(name, parent) { m_color = QColor("#2196F3"); }

void QBarSeries::addBarSet(QBarSet* set) {
    if(!set)return;set->setParent(this);m_barSets.append(set);
    connect(set,&QBarSet::valuesChanged,this,[this](){emit dataChanged();});
    emit dataChanged();
}
void QBarSeries::removeBarSet(QBarSet* set){m_barSets.removeAll(set);delete set;emit dataChanged();}


