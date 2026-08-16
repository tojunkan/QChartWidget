// test_legend_interact.cpp —— 图例点击交互集成测试（B4）
#include <QtTest>
#include <QApplication>
#include "../../QChartWidget.h"
#include "../../QChartLayer.h"
#include "../../QChartLegend.h"
#include "../../QScatterSeries.h"
#include "test_legend_interact.h"

// ===== 点击图例项 → 对应 series 可见性翻转，再点复原 =====
void TestLegendInteract::click_togglesVisibility() {
    QChartWidget w;
    auto* layer = new QChartLayer();
    w.addLayer(layer);
    auto* s0 = new QScatterSeries("s0");
    auto* s1 = new QScatterSeries("s1");
    layer->addSeries(s0);
    layer->addSeries(s1);

    w.resize(400, 300);
    w.show();
    QCoreApplication::processEvents();

    QVERIFY(w.isLegendVisible());
    QCOMPARE(w.legendItems().size(), 2);
    QVERIFY(w.plotArea().width() > 0);

    const QRectF r0 = w.legend()->itemRect(0, w.plotArea(), w.legendItems());
    QVERIFY(!r0.isEmpty());
    const QPoint pos = r0.center().toPoint();

    QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, pos);
    QVERIFY(!s0->isVisible());   // 翻转
    QVERIFY(s1->isVisible());

    QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, pos);
    QVERIFY(s0->isVisible());    // 复原
}

// ===== 点 plotArea 空白（图例外）不切换 series =====
void TestLegendInteract::click_emptyArea_noToggle() {
    QChartWidget w;
    auto* layer = new QChartLayer();
    w.addLayer(layer);
    auto* s0 = new QScatterSeries("s0");
    layer->addSeries(s0);

    w.resize(400, 300);
    w.show();
    QCoreApplication::processEvents();

    // plotArea 内远离图例（图例默认左上角）
    const QPointF emptyPos = w.plotArea().center();
    QVERIFY(!w.legend()->boundingRect(w.plotArea(), w.legendItems()).contains(emptyPos));

    QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, emptyPos.toPoint());
    QVERIFY(s0->isVisible());   // 未切换
}
