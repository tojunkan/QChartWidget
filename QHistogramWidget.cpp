#include "QHistogramWidget.h"
#include "QChartGeometry.h"
#include "QHistogramSeries.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QtMath>
#include <algorithm>

QHistogramWidget::QHistogramWidget(QWidget* p):QBarWidget(p){
    // Replace the default QBarSeries with QHistogramSeries
    // QBarWidget creates a QBarSeries by default. We'll reuse it but set a histogram series.
    // For simplicity, just override the internal series behavior.
}

void QHistogramWidget::setRawData(const QVector<qreal>& d){
    m_rawData=d;std::sort(m_rawData.begin(),m_rawData.end());
    qreal dMin=m_rawData.first(),dMax=m_rawData.last(),rng=dMax-dMin;
    int nB=m_binCountHint>0?m_binCountHint:qMax(1,(int)std::ceil(std::log2(m_rawData.size())+1));
    qreal bw=rng/nB;QVector<qreal> freqs(nB,0);
    for(qreal v:m_rawData){int idx=qBound(0,int((v-dMin)/bw),nB-1);freqs[idx]++;}
    QStringList cats;for(int i=0;i<nB;++i)cats.append(QString::number(dMin+(i+.5)*bw,'f',1));
    qDeleteAll(barSeries()->barSets());barSeries()->barSets().clear();
    auto* bs=new QBarSet("频数");QVector<qreal> vs;for(qreal f:freqs)vs.append(f);bs->setValues(vs);barSeries()->addBarSet(bs);
    barSeries()->setCategories(cats);
    qreal maxF=*std::max_element(freqs.begin(),freqs.end());
    if(m_horizontal){barSeries()->setOrientation(QBarSeries::Horizontal);axisX()->setRange(0,maxF*1.2);axisY()->setRange(0,nB);}
    else{axisY()->setRange(0,maxF*1.2);axisX()->setRange(0,nB);}
    qDebug()<<"[HistogramWidget] nBins:"<<nB<<"maxF:"<<maxF<<"freqs:"<<freqs;
    update();
}
void QHistogramWidget::setHorizontal(bool on){m_horizontal=on;barSeries()->setOrientation(on?QBarSeries::Horizontal:QBarSeries::Vertical);}
void QHistogramWidget::setDensityCurveVisible(bool v){m_densityVisible=v;update();}
void QHistogramWidget::setNormalCurveVisible(bool v){m_normalVisible=v;update();}

void QHistogramWidget::paintEvent(QPaintEvent* e){QBarWidget::paintEvent(e);if(m_densityVisible||m_normalVisible){QPainter p(this);drawCurves(&p);}}

static qreal normalPdf(qreal x,qreal mu,qreal sigma){if(sigma<=0)return 0;qreal z=(x-mu)/sigma;return std::exp(-.5*z*z)/(sigma*std::sqrt(2*M_PI));}

void QHistogramWidget::drawCurves(QPainter* p){
    if(m_rawData.isEmpty())return;
    auto* g=m_geom;QRectF a=g->plotArea();qreal w=a.width(),h=a.height();
    qreal dMin=m_rawData.first(),dMax=m_rawData.last();
    auto* axYVal=m_horizontal?axisX():axisY();
    auto* axXVal=m_horizontal?axisY():axisX();
    // Density curve
    if(m_densityVisible){
        int n=m_rawData.size();qreal bw=(dMax-dMin)/qMax(1,m_binCountHint>0?m_binCountHint:(int)std::ceil(std::log2(n)+1));
        int nBins=qMax(1,m_binCountHint>0?m_binCountHint:(int)std::ceil(std::log2(n)+1));
        QVector<qreal> freq(nBins,0);for(qreal v:m_rawData){int idx=qBound(0,int((v-dMin)/bw),nBins-1);freq[idx]++;}
        QPainterPath path;bool first=true;
        for(int i=0;i<nBins;++i){qreal xv=dMin+(i+.5)*bw;qreal yv=freq[i]/(n*bw);
            QPointF pt;if(m_horizontal)pt=QPointF(a.left()+axYVal->mapToPixel(yv,w),a.bottom()-axXVal->mapToPixel(xv,h));
            else pt=QPointF(a.left()+axXVal->mapToPixel(xv,w),a.bottom()-axYVal->mapToPixel(yv,h));
            if(first){path.moveTo(pt);first=false;}else path.lineTo(pt);}
        p->save();p->setPen(QPen(m_densityColor,2));p->setBrush(Qt::NoBrush);p->drawPath(path);p->restore();}
    // Normal curve
    if(m_normalVisible){int n=m_rawData.size();qreal mu=0;for(qreal v:m_rawData)mu+=v;mu/=n;qreal sigma=0;for(qreal v:m_rawData)sigma+=(v-mu)*(v-mu);sigma=std::sqrt(sigma/(n-1));
        QPainterPath path;bool first=true;
        for(int i=0;i<=100;++i){qreal xv=dMin+(dMax-dMin)*i/100;qreal yv=normalPdf(xv,mu,sigma);
            QPointF pt;if(m_horizontal)pt=QPointF(a.left()+axYVal->mapToPixel(yv,w),a.bottom()-axXVal->mapToPixel(xv,h));
            else pt=QPointF(a.left()+axXVal->mapToPixel(xv,w),a.bottom()-axYVal->mapToPixel(yv,h));
            if(first){path.moveTo(pt);first=false;}else path.lineTo(pt);}
        p->save();p->setPen(QPen(m_normalColor,2));p->setBrush(Qt::NoBrush);p->drawPath(path);p->restore();}
}
