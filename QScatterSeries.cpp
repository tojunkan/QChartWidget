#include "QScatterSeries.h"
#include "QChartGeometry.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QtMath>
#include <cmath>

QScatterSeries::QScatterSeries(const QString& name, QObject* parent)
    : QAbstractSeries(name, parent) { m_color = QColor("#F44336"); }

void QScatterSeries::append(qreal x, qreal y) { m_points.append({x,y}); emit dataChanged(); }
void QScatterSeries::clear() { m_points.clear(); emit dataChanged(); }
QPointF QScatterSeries::at(int i) const { return (i>=0&&i<m_points.size())?m_points[i]:QPointF(); }
void QScatterSeries::setPoints(const QVector<QPointF>& pts) { m_points=pts; emit dataChanged(); }

QRectF QScatterSeries::boundingRect() const {
    if (m_points.isEmpty()) return {};
    qreal xMin=m_points[0].x(),xMax=xMin,yMin=m_points[0].y(),yMax=yMin;
    for (auto& p:m_points){xMin=qMin(xMin,p.x());xMax=qMax(xMax,p.x());yMin=qMin(yMin,p.y());yMax=qMax(yMax,p.y());}
    return {xMin,yMin,xMax-xMin,yMax-yMin};
}





QScatterSeries* QScatterSeries::randomScatter(const QString& name, int n, qreal xMean, qreal xSd, qreal yMean, qreal ySd, QObject* parent){
    auto* s=new QScatterSeries(name,parent);
    auto* rng=QRandomGenerator::global();
    for(int i=0;i<n;++i){
        qreal u1=rng->generateDouble(),u2=rng->generateDouble();
        qreal z1=std::sqrt(-2*std::log(qMax(u1,0.0001)))*std::cos(2*M_PI*u2);
        qreal z2=std::sqrt(-2*std::log(qMax(u1,0.0001)))*std::sin(2*M_PI*u2);
        s->append(xMean+z1*xSd,yMean+z2*ySd);
    }
    return s;
}
