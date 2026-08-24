// demo_polar.cpp —— Polar 五边形（曲线边验证）
#include "demos.h"
#include "QChartWidget.h"
#include "QChartProjectionFactory.h"
#include "QPolygonSeries.h"
#include "QScatterSeries.h"
#include "QValueAxis.h"
#include <QDebug>

QWidget* buildDemoPolar() {
    qDebug() << "\n========== Polar 五边形 ==========";

    auto* w = new QChartWidget();
    w->setWindowTitle("Polar 五边形 - 曲线边验证");
    w->setProjection(QChartProjectionFactory::create(CoordinateSystem::Polar));

    auto* angleAxis = new QValueAxis(w, Qt::AlignHCenter);
    w->addAxis(angleAxis);
    angleAxis->setLabelFormat("%g°");
    angleAxis->setRange(0, 360);

    auto* radialAxis = new QValueAxis(w, Qt::AlignVCenter);
    w->addAxis(radialAxis);
    radialAxis->setRange(0, 5);

    auto* layer = new QChartLayer(w);
    layer->setAxisX(angleAxis);
    layer->setAxisY(radialAxis);
    w->addLayer(layer);

    // 五边形顶点：θ=0°,72°,144°,216°,288°， r=4
    auto* penta = new QPolygonSeries("pentagon", layer);
    penta->setColor(QColor("#FF5722"));
    penta->setFillColor(QColor(255, 87, 34, 80));
    for (int i = 0; i < 5; ++i) {
        qreal theta = i * 72.0;
        penta->append(theta, 4.0);
    }
    layer->addSeries(penta);

    // 顶点加标记
    auto* vertexMarks = new QScatterSeries("vertices", layer);
    vertexMarks->setColor(QColor("#FFC107"));
    vertexMarks->setMarkerSize(8);
    for (int i = 0; i < 5; ++i)
        vertexMarks->append(i * 72.0, 4.0);
    layer->addSeries(vertexMarks);

    w->resize(600, 500);
    return w;
}
