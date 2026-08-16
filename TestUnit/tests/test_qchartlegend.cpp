// test_qchartlegend.cpp —— QChartLegend 单元测试
#include <QtTest>
#include <QImage>
#include "../../QChartLegend.h"
#include "../../QChartWidget.h"
#include "../../QChartLayer.h"
#include "../../QScatterSeries.h"
#include "../../QPainterChartRenderer.h"
#include "test_qchartlegend.h"

namespace {
constexpr qreal INSET = 8.0;   // 与 QChartLegend.cpp 布局常量一致
}

// ===== 四角锚点正确、落在 plotArea 内 =====
void TestQChartLegend::boundingRect_fourCorners() {
    QChartLegend legend;
    QScatterSeries* a = new QScatterSeries("aa");
    QScatterSeries* b = new QScatterSeries("bbbb");
    const QList<QChartSeries*> items = { a, b };
    const QRectF pa(100, 100, 400, 300);

    legend.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QRectF tl = legend.boundingRect(pa, items);
    QVERIFY(pa.contains(tl));
    QVERIFY(qAbs(tl.left() - (pa.left() + INSET)) < 1e-6);
    QVERIFY(qAbs(tl.top() - (pa.top() + INSET)) < 1e-6);

    legend.setAlignment(Qt::AlignRight | Qt::AlignTop);
    QRectF tr = legend.boundingRect(pa, items);
    QVERIFY(pa.contains(tr));
    QVERIFY(qAbs(tr.right() - (pa.right() - INSET)) < 1e-6);
    QVERIFY(qAbs(tr.top() - (pa.top() + INSET)) < 1e-6);

    legend.setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    QRectF bl = legend.boundingRect(pa, items);
    QVERIFY(pa.contains(bl));
    QVERIFY(qAbs(bl.left() - (pa.left() + INSET)) < 1e-6);
    QVERIFY(qAbs(bl.bottom() - (pa.bottom() - INSET)) < 1e-6);

    legend.setAlignment(Qt::AlignRight | Qt::AlignBottom);
    QRectF br = legend.boundingRect(pa, items);
    QVERIFY(pa.contains(br));
    QVERIFY(qAbs(br.right() - (pa.right() - INSET)) < 1e-6);
    QVERIFY(qAbs(br.bottom() - (pa.bottom() - INSET)) < 1e-6);

    delete a;
    delete b;
}

// ===== seriesAt：命中色块行返回 series、空白返回 nullptr =====
void TestQChartLegend::seriesAt_hitAndMiss() {
    QChartLegend legend;
    QScatterSeries* a = new QScatterSeries("aa");
    QScatterSeries* b = new QScatterSeries("bbbb");
    const QList<QChartSeries*> items = { a, b };
    const QRectF pa(0, 0, 200, 200);

    const QRectF r0 = legend.itemRect(0, pa, items);
    QVERIFY(!r0.isEmpty());
    QVERIFY(legend.seriesAt(QPointF(r0.left() + 2.0, r0.center().y()), pa, items) == a);

    const QRectF r1 = legend.itemRect(1, pa, items);
    QVERIFY(legend.seriesAt(QPointF(r1.left() + 2.0, r1.center().y()), pa, items) == b);

    // 空白（box 外）
    QVERIFY(legend.seriesAt(QPointF(190, 190), pa, items) == nullptr);

    delete a;
    delete b;
}

// ===== widget 组装 legendItems 跳过空 name =====
void TestQChartLegend::legendItems_filterEmptyName() {
    QChartWidget w;
    auto* layer = new QChartLayer();
    w.addLayer(layer);
    auto* named = new QScatterSeries("named");
    auto* empty = new QScatterSeries("");   // 空 name → 应被跳过
    layer->addSeries(named);
    layer->addSeries(empty);

    QCOMPARE(w.legendItems().size(), 1);
    QVERIFY(w.legendItems().first() == named);
}

// ===== 渲染到 QImage：可见/隐藏像素不同（降透明度）=====
void TestQChartLegend::render_visibleVsHidden() {
    QChartLegend legend;
    QScatterSeries* s0 = new QScatterSeries("s0");
    QScatterSeries* s1 = new QScatterSeries("s1");
    s0->setColor(Qt::red);
    s1->setColor(Qt::blue);
    const QList<QChartSeries*> items = { s0, s1 };

    QChartScene scene;
    scene.plotArea = QRectF(0, 0, 200, 200);
    scene.legend = &legend;
    scene.legendItems = items;
    // 无 axes/layers → 只画图例

    QPainterChartRenderer renderer;
    renderer.setCachingEnabled(false);

    QImage imgA(200, 200, QImage::Format_ARGB32_Premultiplied);
    imgA.fill(Qt::transparent);
    renderer.render(scene, &imgA);

    s0->setVisible(false);   // 隐藏 → 降透明度
    QImage imgB(200, 200, QImage::Format_ARGB32_Premultiplied);
    imgB.fill(Qt::transparent);
    renderer.render(scene, &imgB);

    QVERIFY(imgA != imgB);

    delete s0;
    delete s1;
}

// ===== textColorChanged 信号（override 双槽：setTextColor/setThemeTextColor/clearTextColor）=====
void TestQChartLegend::textColorChanged_signal() {
    QChartLegend legend;
    QSignalSpy spy(&legend, &QChartLegend::textColorChanged);

    // setTextColor → override + 发信号
    legend.setTextColor(Qt::red);
    QCOMPARE(spy.count(), 1);
    QVERIFY(legend.textColor() == Qt::red);

    // setThemeTextColor：有 override → 不发信号、override 保留
    legend.setThemeTextColor(Qt::blue);
    QCOMPARE(spy.count(), 1);
    QVERIFY(legend.textColor() == Qt::red);

    // clearTextColor → 回主题默认（blue）+ 发信号
    legend.clearTextColor();
    QCOMPARE(spy.count(), 2);
    QVERIFY(legend.textColor() == Qt::blue);

    // setThemeTextColor：无 override → 发信号
    legend.setThemeTextColor(Qt::green);
    QCOMPARE(spy.count(), 3);
    QVERIFY(legend.textColor() == Qt::green);
}
