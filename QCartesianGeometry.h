#pragma once
#include "QChartGeometry.h"
// ===== Cartesian =====
class QCartesianGeometry : public QChartGeometry {
    Q_OBJECT
public:
    using QChartGeometry::QChartGeometry;
    QPointF mapToPixel(qreal x, qreal y) const override;
    QPointF mapFromPixel(const QPointF& p) const override;
    void drawGrid(QPainter* p) override;
    void drawSeries(QPainter* p, QAbstractSeries* s) override;
    int hitTestSeries(QAbstractSeries* s, const QPointF& pos) const override;
private:
    void drawLineSeries(QPainter*, class QLineSeries*);
    void drawScatterSeries(QPainter*, class QScatterSeries*);
    void drawBarSeries(QPainter*, class QBarSeries*);
};

