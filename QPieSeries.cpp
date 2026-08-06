#include "QPieSeries.h"
#include "QChartLayer.h"
#include "QChartAxis.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QtMath>
#include <cmath>

static QColor autoPieColor(int idx) {
    static const QList<QColor> pals{QColor("#2196F3"),QColor("#F44336"),QColor("#4CAF50"),QColor("#FF9800"),QColor("#9C27B0"),QColor("#00BCD4"),QColor("#FF5722"),QColor("#3F51B5")};
    return pals[idx%pals.size()];
}

QPieSeries::QPieSeries(const QString& name, QObject* parent) : QAbstractSeries(name, parent) {}

void QPieSeries::appendSlice(const QString& label, qreal value, const QColor& color) {
    PieSlice s; s.label=label; s.value=value;
    s.color=color.isValid()?color:autoPieColor(m_slices.size());
    m_slices.append(s); emit dataChanged();
}

qreal QPieSeries::totalValue() const {
    qreal t=0; for(auto& s:m_slices) t+=s.value; return t;
}

void QPieSeries::setSliceExploded(int i, bool on) {
    if(i<0||i>=m_slices.size())return;
    auto& s=m_slices[i]; s.exploded=on; s.explodeTarget=on?1.0:0.0;
    if(s.anim){s.anim->stop();s.anim->deleteLater();}
    auto* a=new QVariantAnimation(this); a->setDuration(300);a->setEasingCurve(QEasingCurve::OutQuad);
    a->setStartValue(s.explodeOffset);a->setEndValue(s.explodeTarget);
    connect(a,&QVariantAnimation::valueChanged,this,[this,i](const QVariant& v){m_slices[i].explodeOffset=v.toReal();emit dataChanged();});
    a->start(); s.anim=a;
}

void QPieSeries::setSliceExplodeFactor(int i, qreal f) {
    if(i>=0&&i<m_slices.size()) m_slices[i].explodeFactor=qBound(0.0,f,1.0);
}



int QPieSeries::sliceAtAngle(qreal a, qreal r) const {
    // 内联命中检测
    if(r<0||r>1||m_holeSize>=0.99)return -1;
    qreal total=totalValue();if(total<=0)return -1;
    qreal cur=m_startAngle-90;if(cur<0)cur+=360;
    for(int i=0;i<m_slices.size();++i){qreal span=m_slices[i].value/total*360.;if(span<=0){cur+=span;continue;}
        qreal sS=fmod(-cur,360.);if(sS<0)sS+=360;qreal eS=fmod(-(cur+span),360.);if(eS<0)eS+=360;
        bool in;if(qFuzzyCompare(sS,eS))in=true;else if(sS>eS)in=(a>=eS&&a<sS);else in=(a<sS||a>=eS);
        if(in)return i;cur+=span;}
    return -1;
}

