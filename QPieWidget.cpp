#include "QPieWidget.h"
QPieWidget::QPieWidget(QWidget* p):QChartWidget(p){
    m_geom=new QPolarGeometry;addGeometry(m_geom);
    auto* angAx=new QAngleAxis;auto* radAx=new QRadialAxis;
    addAxis(angAx);addAxis(radAx);
    m_geom->setAxisX(angAx);m_geom->setAxisY(radAx);
    m_series=new QPieSeries("pie");m_geom->addSeries(m_series);
    radAx->setVisible(false);angAx->setVisible(false); // 饼图默认不显示轴
}
void QPieWidget::appendSlice(const QString& l,qreal v,const QColor& c){m_series->appendSlice(l,v,c);}
void QPieWidget::setHoleSize(qreal s){if(m_geom->axisY())m_geom->axisY()->setMin(qBound(0.,s,.99));}
void QPieWidget::setStartAngle(qreal d){if(m_geom->axisX())m_geom->axisX()->setMin(d);}
void QPieWidget::setSliceExploded(int i,bool on){m_series->setSliceExploded(i,on);}
