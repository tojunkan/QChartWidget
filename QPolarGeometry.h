#pragma once
#include "QChartGeometry.h"
// ===== Polar =====
class QPolarGeometry : public QChartGeometry {
    Q_OBJECT
public:
    explicit QPolarGeometry(QObject* parent = nullptr);
    void autoFit();
    QPointF mapToPixel(qreal x, qreal y) const override;
    QPointF mapFromPixel(const QPointF& p) const override;
    void drawGrid(QPainter* p) override;
    void drawSeries(QPainter* p, QAbstractSeries* s) override;
    int hitTestSeries(QAbstractSeries* s, const QPointF& pos) const override;
private:
    void drawPieSeries(QPainter*, class QPieSeries*);
};