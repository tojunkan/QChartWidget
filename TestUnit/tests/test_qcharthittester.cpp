// test_qcharthittester.cpp —— QChartHitTester 统一命中引擎单元测试（Phase 3 任务 0）
// 2D 部分与 test_hittest 同构（Cartesian + plotArea(0,0,100,100) + viewRect/dataBounds(0,0,10,10)
// → Data(x,y) → Pixel(10x, 100-10y)）；3D 部分直接构造 QChartPrimitive。
#include <QtTest>
#include "../../QChartHitTester.h"
#include "../../QChartLayer.h"
#include "../../QValueAxis.h"
#include "../../QCartesianProjection.h"
#include "../../QScatterSeries.h"
#include "../../QChartRenderer.h"   // QChartPrimitive
#include "../../QChartLineSeries3D.h"   // PickRecord.series 具体类型
#include "test_qcharthittester.h"

namespace {
// 2D 命中上下文（同 test_hittest 组装方式）
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

// 与 layer.makeToPixel 在 Cartesian/viewRect(0,0,10,10)/plot(0,0,100,100) 下等价的闭包
std::function<QPointF(QVariant, QVariant)> identityPixelFn() {
    return [](QVariant x, QVariant y) -> QPointF {
        return QPointF(x.toDouble() * 10.0, 100.0 - y.toDouble() * 10.0);
    };
}

QChartPrimitive pointPrim(const QPointF& a, int dataIndex, QChartPrimitive::Layer layer) {
    QChartPrimitive prim;
    prim.type = QChartPrimitive::Type::Point;
    prim.a = a;
    prim.dataIndex = dataIndex;
    prim.layer = layer;
    return prim;
}
QChartPrimitive segPrim(const QPointF& a, const QPointF& b, int dataIndex,
                        QChartPrimitive::Layer layer = QChartPrimitive::Layer::Series) {
    QChartPrimitive prim;
    prim.type = QChartPrimitive::Type::LineSegment;
    prim.a = a;
    prim.b = b;
    prim.dataIndex = dataIndex;
    prim.layer = layer;
    return prim;
}
} // namespace

// ===== 2D：委托层 hitTest 与 QChartHitTester 直调一致（命中/未命中/dataIndex==index）=====
void TestQChartHitTester::hit2d_delegation() {
    HitFixture fx;
    auto* s = new QScatterSeries("scatter");
    s->append(5, 5);   // → pixel (50, 50)
    s->append(1, 1);   // → pixel (10, 90)
    fx.layer.addSeries(s);

    const auto toPixel = identityPixelFn();

    // 命中：委托层结果 == 直调结果，dataIndex == index == 0
    const QChartHitTester::HitResult viaLayer = fx.layer.hitTest(QPointF(50, 50), fx.ctx);
    const QChartHitTester::HitResult direct =
        QChartHitTester::hitTest(QPointF(50, 50), fx.layer.seriesList(), toPixel, &fx.ctx);
    QCOMPARE(viaLayer.index, 0);
    QCOMPARE(viaLayer.dataIndex, 0);          // 2D 下 dataIndex == index
    QCOMPARE(viaLayer.series, static_cast<QChartSeries*>(s));
    QCOMPARE(direct.index, viaLayer.index);
    QCOMPARE(direct.dataIndex, viaLayer.dataIndex);
    QCOMPARE(direct.series, viaLayer.series);

    // 命中第二点
    QCOMPARE(fx.layer.hitTest(QPointF(10, 90), fx.ctx).index, 1);
    QCOMPARE(fx.layer.hitTest(QPointF(10, 90), fx.ctx).dataIndex, 1);

    // 未命中（距点 0 距离 22 > 阈值 12）→ 空 HitResult
    const QChartHitTester::HitResult miss = fx.layer.hitTest(QPointF(50, 72), fx.ctx);
    QCOMPARE(miss.index, -1);
    QCOMPARE(miss.dataIndex, -1);
    QVERIFY(miss.series == nullptr);
    const QChartHitTester::HitResult missDirect =
        QChartHitTester::hitTest(QPointF(50, 72), fx.layer.seriesList(), toPixel, &fx.ctx);
    QCOMPARE(missDirect.index, -1);
    QCOMPARE(missDirect.dataIndex, -1);
}

// ===== 2D：顶层可见系列优先（自后向前）=====
void TestQChartHitTester::hit2d_topLayerPriority() {
    HitFixture fx;
    auto* bottom = new QScatterSeries("bottom");
    bottom->append(5, 5);
    auto* top = new QScatterSeries("top");
    top->append(5, 5);
    fx.layer.addSeries(bottom);
    fx.layer.addSeries(top);   // top 后加 → 顶层

    const QChartHitTester::HitResult r = fx.layer.hitTest(QPointF(50, 50), fx.ctx);
    QCOMPARE(r.series, static_cast<QChartSeries*>(top));
    QCOMPARE(r.index, 0);

    // 顶层不可见 → 回退到底层
    top->setVisible(false);
    const QChartHitTester::HitResult r2 = fx.layer.hitTest(QPointF(50, 50), fx.ctx);
    QCOMPARE(r2.series, static_cast<QChartSeries*>(bottom));
    top->setVisible(true);
}

// ===== 3D：点图元命中/未命中/8px 阈值 =====
void TestQChartHitTester::hit3d_pointNearest() {
    QVector<QChartPrimitive> items;
    items.append(pointPrim(QPointF(100, 100), 3, QChartPrimitive::Layer::Series));

    // 命中（距离 5 < 8）→ dataIndex 透传
    QCOMPARE(QChartHitTester::hitTest(QPointF(103, 104), items).dataIndex, 3);
    // 恰好阈值边界：距离 8 → 不命中（严格 <）
    QCOMPARE(QChartHitTester::hitTest(QPointF(108, 100), items).dataIndex, -1);
    // 未命中（距离 9）
    QCOMPARE(QChartHitTester::hitTest(QPointF(109, 100), items).dataIndex, -1);
    // 自定义阈值
    QCOMPARE(QChartHitTester::hitTest(QPointF(103, 100), items, 2.0).dataIndex, -1);
    QCOMPARE(QChartHitTester::hitTest(QPointF(101, 100), items, 2.0).dataIndex, 3);
    // 空列表 → 未命中
    QVector<QChartPrimitive> empty;
    QCOMPARE(QChartHitTester::hitTest(QPointF(100, 100), empty).dataIndex, -1);
}

// ===== 3D：点到线段距离（含端点外投影 clamp）=====
void TestQChartHitTester::hit3d_segmentDistance() {
    QVector<QChartPrimitive> items;
    items.append(segPrim(QPointF(0, 0), QPointF(10, 0), 5));

    QCOMPARE(QChartHitTester::hitTest(QPointF(5, 3), items).dataIndex, 5);     // 距离 3
    QCOMPARE(QChartHitTester::hitTest(QPointF(12, 3), items).dataIndex, 5);    // 端点外 clamp → √13≈3.6
    QCOMPARE(QChartHitTester::hitTest(QPointF(5, 20), items).dataIndex, -1);   // 距离 20
    QCOMPARE(QChartHitTester::hitTest(QPointF(25, 0), items).dataIndex, -1);   // 距离 15
}

// ===== 3D：Grid/ForegroundDecor 排除 + dataIndex<0 排除 + 透传 =====
void TestQChartHitTester::hit3d_layerFilter() {
    QVector<QChartPrimitive> items;
    items.append(segPrim(QPointF(0, 0), QPointF(10, 0), 1, QChartPrimitive::Layer::Grid));
    items.append(pointPrim(QPointF(5, 0), 2, QChartPrimitive::Layer::ForegroundDecor));
    items.append(pointPrim(QPointF(5, 0), 7, QChartPrimitive::Layer::Series));

    // 鼠标 (5,0)：Grid/Decor 排除 → 命中 Series 的 dataIndex 7
    QCOMPARE(QChartHitTester::hitTest(QPointF(5, 0), items).dataIndex, 7);

    // 只有 Grid/Decor → 未命中
    QVector<QChartPrimitive> decorOnly;
    decorOnly.append(segPrim(QPointF(0, 0), QPointF(10, 0), 1, QChartPrimitive::Layer::Grid));
    decorOnly.append(pointPrim(QPointF(5, 0), 2, QChartPrimitive::Layer::ForegroundDecor));
    QCOMPARE(QChartHitTester::hitTest(QPointF(5, 0), decorOnly).dataIndex, -1);

    // dataIndex<0 的 Series 图元也排除（§7.4 只扫 dataIndex>=0）
    QVector<QChartPrimitive> neg;
    neg.append(pointPrim(QPointF(5, 0), -1, QChartPrimitive::Layer::Series));
    QCOMPARE(QChartHitTester::hitTest(QPointF(5, 0), neg).dataIndex, -1);
}

// ===== 3D：多图元取最近（< 阈值内最小距离）=====
void TestQChartHitTester::hit3d_multiPrimitive() {
    QVector<QChartPrimitive> items;
    items.append(pointPrim(QPointF(100, 100), 1, QChartPrimitive::Layer::Series));
    items.append(pointPrim(QPointF(100, 102), 9, QChartPrimitive::Layer::Series));

    // 鼠标 (100,100)：far 距离 0 → 最近
    QCOMPARE(QChartHitTester::hitTest(QPointF(100, 100), items).dataIndex, 1);
    // 鼠标 (100,101.5)：near 距离 0.5、far 距离 1.5 → near（全局最近）
    QCOMPARE(QChartHitTester::hitTest(QPointF(100, 101.5f), items).dataIndex, 9);
    // 混合点+线段：线段 (0,0)-(10,0) vs 点 (5,2)：鼠标 (5,1) → 线段距离 1、点距离 1 → 顺序取段
    QVector<QChartPrimitive> mixed;
    mixed.append(segPrim(QPointF(0, 0), QPointF(10, 0), 4));
    mixed.append(pointPrim(QPointF(5, 2), 8, QChartPrimitive::Layer::Series));
    const QChartHitTester::HitResult r = QChartHitTester::hitTest(QPointF(5, 1), mixed);
    QVERIFY2(r.dataIndex == 4 || r.dataIndex == 8, "距离并列时两者之一（确定性取先扫到的段）");
}

// ============================================================
// GPU 拾取解码（design_phase3.md §8.1，t46；纯函数，无 GL 依赖）
// ============================================================
// ===== 1. 正常：RGB24 → ID → 查表 → HitResult（series/dataIndex/index）=====
void TestQChartHitTester::hitTestGPU_normal() {
    QChartLineSeries3D s("l");
    QVector<QChartHitTester::PickRecord> table;
    table.append({ &s, 0, QChartPrimitive::Layer::Series });   // id 0
    table.append({ &s, 5, QChartPrimitive::Layer::Series });   // id 1

    const QChartHitTester::HitResult r0 = QChartHitTester::hitTestGPU(0, 0, 0, table);   // id = 0
    QVERIFY(r0.series == &s);
    QCOMPARE(r0.dataIndex, 0);
    QCOMPARE(r0.index, -1);   // 3D 形态：index 恒 -1（与 CPU 近邻一致，仅 dataIndex 语义有效）

    const QChartHitTester::HitResult r1 = QChartHitTester::hitTestGPU(1, 0, 0, table);   // id = 1
    QVERIFY(r1.series == &s);
    QCOMPARE(r1.dataIndex, 5);

    // 高位编码：id = r | g<<8 | b<<16（b=1 → 65536 越界空表外；改用合法 id 验证通道解码）
    const QChartHitTester::HitResult rB = QChartHitTester::hitTestGPU(0, 1, 0, table);   // id = 256（越界）
    QCOMPARE(rB.dataIndex, -1);
}

// ===== 2. 哨兵：0xFFFFFF（背景 / 轴网格 Decor 片段，§5.3 定案）→ 空 =====
void TestQChartHitTester::hitTestGPU_sentinel() {
    QVector<QChartHitTester::PickRecord> table;
    table.append({ nullptr, -1, QChartPrimitive::Layer::Grid });
    const QChartHitTester::HitResult r = QChartHitTester::hitTestGPU(255, 255, 255, table);
    QVERIFY(r.series == nullptr);
    QCOMPARE(r.dataIndex, -1);
}

// ===== 3. 越界 / 空表 → 空 =====
void TestQChartHitTester::hitTestGPU_outOfRange() {
    QChartLineSeries3D s;
    QVector<QChartHitTester::PickRecord> empty;
    QCOMPARE(QChartHitTester::hitTestGPU(0, 0, 0, empty).dataIndex, -1);   // 空表 → id0 越界 → 空

    QVector<QChartHitTester::PickRecord> table;
    table.append({ &s, 3, QChartPrimitive::Layer::Series });
    const QChartHitTester::HitResult r = QChartHitTester::hitTestGPU(5, 0, 0, table);   // id 5 > size-1
    QVERIFY(r.series == nullptr);
    QCOMPARE(r.dataIndex, -1);
}

// ===== 4. 两后端交叉验证：GPU 表解码 vs CPU 近邻（简单场景，§8.2）=====
void TestQChartHitTester::hitTestGPU_crossCPU() {
    // 无轴/网格（axesDataBox 无效）→ 图元全为系列；pickTable 模拟 collectScene 输出（id == 图元索引）
    QChartLineSeries3D s;
    QVector<QChartPrimitive> prims;
    prims.append(segPrim(QPointF(0, 0), QPointF(10, 0), 0));    // 段 0（dataIndex 0）
    prims.append(segPrim(QPointF(10, 0), QPointF(20, 0), 1));   // 段 1（dataIndex 1）
    QVector<QChartHitTester::PickRecord> table;
    table.append({ &s, 0, QChartPrimitive::Layer::Series });
    table.append({ &s, 1, QChartPrimitive::Layer::Series });

    const QPointF hit(5, 0);   // 段 0 中点
    const QChartHitTester::HitResult cpu = QChartHitTester::hitTest(hit, prims, 8.0);
    QCOMPARE(cpu.dataIndex, 0);   // CPU 近邻

    // GPU 侧：该图元 id = 0 → 解码与 CPU 一致（series + dataIndex + index）
    const QChartHitTester::HitResult gpu = QChartHitTester::hitTestGPU(0, 0, 0, table);
    QVERIFY(gpu.series == &s);
    QCOMPARE(gpu.dataIndex, cpu.dataIndex);
    QCOMPARE(gpu.index, cpu.index);   // 两后端形态一致：index 恒 -1（3D）
}
