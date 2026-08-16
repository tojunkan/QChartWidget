// demo_bar.cpp —— Cartesian 柱状图（drawRect 快路径）
#include "demos.h"
#include "../../QChartWidget.h"
#include "../../QChartProjectionFactory.h"
#include "../../QBarSeries.h"
#include "../../QValueAxis.h"
#include "../../QBarCategoryAxis.h"
#include <QDebug>

QChartWidget* buildDemoBar() {
    qDebug() << "\n========== Cartesian 柱状图 ==========";

    auto* w = new QChartWidget();
    w->setWindowTitle("Cartesian 柱状图 - drawRect 快路径");
    w->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    w->setViewRectFitMode(ViewRectFitMode::Stretch); // Cartesian 下直接拉伸即可

    auto* xAxis = new QBarCategoryAxis(w, Qt::AlignBottom);
    xAxis->setColor(Qt::white);
    w->addAxis(xAxis);
    xAxis->setCategories({"苹果","香蕉","橙子","葡萄","西瓜"});
    xAxis->setNumericMapping(-0.5, 4.5);  // 槽位居中：索引0→-0.5, 索引4→4.5

    auto* valAxis = new QValueAxis(w, Qt::AlignLeft);
    valAxis->setColor(Qt::white);
    w->addAxis(valAxis);
    valAxis->setRange(0, 100);

    auto* layer = new QChartLayer(w);
    layer->setAxisX(xAxis);
    layer->setAxisY(valAxis);
    w->addLayer(layer);

    auto* bars = new QBarSeries("bars", layer);
    bars->setColor(QColor("#4CAF50"));
    bars->setFillColor(QColor(76, 175, 80, 160));
    bars->append(-0.3, 0, 0.3, 45);
    bars->append(0.7, 0, 1.3, 72);
    bars->append(1.7, 0, 2.3, 33);
    bars->append(2.7, 0, 3.3, 88);
    bars->append(3.7, 0, 4.3, 56);
    layer->addSeries(bars);

    w->resize(600, 400);
    return w;
}
