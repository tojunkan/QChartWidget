// test_hittest.cpp —— Series 命中检测单元测试
// 用最小对象组装（QChartLayer + Series + 手动 DrawContext），无需 QChartWidget/QApplication。
// 坐标系：Cartesian + plotArea(0,0,100,100) + viewRect/dataBounds(0,0,10,10)，
// 故 Data(x,y) → Pixel(10x, 100-10y)。
#include <QtTest>
#include "../../QChartLayer.h"
#include "../../QValueAxis.h"
#include "../../QCartesianProjection.h"
#include "../../QScatterSeries.h"
#include "../../QLineSeries.h"
#include "../../QBarSeries.h"
#include "test_hittest.h"

namespace {
// 组装一个 Cartesian 命中检测上下文 + layer（axes 归 caller 删除，series 归 layer 删除）
struct HitFixture {
    QValueAxis* xAxis;
    QValueAxis* yAxis;
    QChartLayer layer;
    QCartesianProjection proj;
    DrawContext ctx;

    HitFixture() {
        xAxis = new QValueAxis(nullptr, Qt::AlignBottom);
        yAxis = new QValueAxis(nullptr, Qt::AlignLeft);
        layer.setAxisX(xAxis);
        layer.setAxisY(yAxis);
        ctx.plotArea   = QRectF(0, 0, 100, 100);
        ctx.dataBounds = QRectF(0, 0, 10, 10);
        ctx.viewRect   = QRectF(0, 0, 10, 10);
        ctx.projection = &proj;
    }
    ~HitFixture() { delete xAxis; delete yAxis; }
};
} // namespace

// ===== QScatterSeries：像素到点距离 < markerSize*1.5 命中 =====
void TestHitTest::scatter_hitAndMiss() {
    HitFixture fx;
    auto* s = new QScatterSeries("scatter");
    s->append(5, 5);   // → pixel (50, 50)
    s->append(1, 1);   // → pixel (10, 90)
    fx.layer.addSeries(s);

    QCOMPARE(fx.layer.hitTest(QPointF(50, 50), fx.ctx).index, 0);  // 命中点 0
    QCOMPARE(fx.layer.hitTest(QPointF(10, 90), fx.ctx).index, 1);  // 命中点 1
    QCOMPARE(fx.layer.hitTest(QPointF(50, 72), fx.ctx).index, -1); // 距点 0 距离 22 > 阈值 12
}

// ===== QLineSeries：像素到折线距离 < 阈值命中 =====
void TestHitTest::line_hitAndMiss() {
    HitFixture fx;
    auto* s = new QLineSeries("line");
    s->append(0, 0);    // → pixel (0, 100)
    s->append(10, 10);  // → pixel (100, 0)
    fx.layer.addSeries(s);

    // 对角线过 (50,50)
    QCOMPARE(fx.layer.hitTest(QPointF(50, 50), fx.ctx).index, 0);  // 命中线段 0
    // (80,80) 到对角线 x+y=100 距离约 42，远超阈值
    QCOMPARE(fx.layer.hitTest(QPointF(80, 80), fx.ctx).index, -1);
}

// ===== QBarSeries：像素落在矩形 bbox 内命中 =====
void TestHitTest::bar_rectHit() {
    HitFixture fx;
    auto* s = new QBarSeries("bar");
    s->append(2, 2, 4, 4);  // → bbox pixel (20,60)-(40,80)
    fx.layer.addSeries(s);

    QCOMPARE(fx.layer.hitTest(QPointF(30, 70), fx.ctx).index, 0);  // bbox 中心命中
    QCOMPARE(fx.layer.hitTest(QPointF(90, 10), fx.ctx).index, -1); // bbox 外
}
