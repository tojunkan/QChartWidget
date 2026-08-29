// test_initialization_flow.cpp
#include "test_initialization_flow.h"
#include <QTest>
#include <QApplication>
#include <QTimer>
#include "QChartWidget.h"
#include "QChartLayer.h"
#include "QValueAxis.h"
#include "QLineSeries.h"

TestInitializationFlow::TestInitializationFlow(QObject* parent)
    : QObject(parent) {}

void TestInitializationFlow::test_emptyWidget_hasNullViewRect() {
    QChartWidget w;
    QVERIFY(w.projection() == nullptr);
    QVERIFY(w.layers().isEmpty());
    QVERIFY(w.axes().isEmpty());
    QVERIFY(w.viewRect().isNull());
}

void TestInitializationFlow::test_addLayer_createsProjection() {
    QChartWidget w;
    auto* layer = new QChartLayer();
    w.addLayer(layer);
    QVERIFY(w.projection() != nullptr);
    QVERIFY(!w.viewRect().isNull());
    QVERIFY(w.viewRect().width() > 0);
}

void TestInitializationFlow::test_addAxis_before_projection_fails() {
    QChartWidget w;
    auto* axis = new QValueAxis();
    w.addAxis(axis);
    QVERIFY(w.axes().isEmpty());
    QVERIFY(axis->parent() == nullptr);
    delete axis;
}

void TestInitializationFlow::test_fullInit_resize_preservesArea() {
    QChartWidget w;
    w.setViewRectFitMode(ViewRectFitMode::Preserve);
    auto* layer = new QChartLayer();
    w.addLayer(layer);
    auto* axisX = new QValueAxis();
    axisX->setAlignment(Qt::AlignBottom);
    w.addAxis(axisX);
    auto* axisY = new QValueAxis();
    axisY->setAlignment(Qt::AlignLeft);
    w.addAxis(axisY);
    auto* series = new QLineSeries();
    series->append(0, 0);
    series->append(10, 10);
    layer->addSeries(series);

    w.show();
    QTest::qWait(100);

    QRectF view = w.viewRect();
    QRectF plot = w.plotArea();
    qreal area = view.width() * view.height();

    QVERIFY(plot.width() > 0 && plot.height() > 0);
    QRectF dataBounds = w.dataBounds();
    QCOMPARE(dataBounds.width(), 10.0);
    QVERIFY(area > 1.0);
}