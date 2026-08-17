// test_qchartrenderer3d.cpp —— Renderer 3D 路径单元测试
// 覆盖（design_3d.md §11.1 TestQChartRenderer3D 全部 6 组）。
// 渲染走 QPainterChartRenderer::renderUncached（直绘，不经 QPixmap 缓存）。
#include <QtTest>
#include <QImage>
#include <algorithm>
#include "../../QChartRenderer.h"
#include "../../QPainterChartRenderer.h"
#include "../../QChartCamera3D.h"
#include "../../QChartLayer3D.h"
#include "../../QChartLineSeries3D.h"
#include "../../QChartScatterSeries3D.h"
#include "../../QChartSurfaceSeries.h"
#include "test_qchartrenderer3d.h"

namespace {
    // 恒等全链闭包：Data → {screen=(x,y), depth=z}
    QChartProjectedPoint identityProject(const QDataPoint3D& d) {
        return QChartProjectedPoint{ QPointF(d.x().toDouble(), d.y().toDouble()), d.z().toDouble() };
    }
    const ProjectFn3D kIdentityFn = identityProject;
}

// ===== 1. line：n 点 → n-1 线段；NaN 断段 =====
void TestQChartRenderer3D::line_collectPrimitives_count() {
    QChartLineSeries3D s;
    s.append(0, 0, 0);
    s.append(1, 0, 2);
    s.append(2, 0, 4);
    s.append(3, 0, 6);
    QVector<QChartPrimitive> out;
    s.collectPrimitives(kIdentityFn, out);
    QCOMPARE(out.size(), 3);               // n-1
    QVERIFY(qAbs(out[0].depth - 1.0) < 1e-6);   // (0+2)/2
    QVERIFY(qAbs(out[2].depth - 5.0) < 1e-6);   // (4+6)/2
    QCOMPARE(out[1].dataIndex, 1);              // 线段起点索引

    // NaN 断段：中间 NaN → 仅两段有效
    QChartLineSeries3D s2;
    s2.append(0, 0, 0);
    s2.append(1, 0, 0);
    s2.append(qQNaN(), 0, 0);
    s2.append(2, 0, 0);
    s2.append(3, 0, 0);
    QVector<QChartPrimitive> out2;
    s2.collectPrimitives(kIdentityFn, out2);
    QCOMPARE(out2.size(), 2);
}

// ===== 2. surface：线框计数 rows·(cols-1) + cols·(rows-1) =====
void TestQChartRenderer3D::surface_wireframeCount() {
    QChartSurfaceSeries surf;
    QVector<QDataPoint3D> pts;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            pts.append(QDataPoint3D(r, c, 0));
    surf.setGrid(3, 4, pts);
    QVector<QChartPrimitive> out;
    surf.collectPrimitives(kIdentityFn, out);
    QCOMPARE(out.size(), 3 * 3 + 4 * 2);   // 17
    for (const QChartPrimitive& prim : out) {
        QVERIFY(prim.type == QChartPrimitive::Type::LineSegment);
        QVERIFY(prim.dataIndex >= 0);
    }

    // 2×2 网格：4 条；NaN 点 → 触及段跳过
    QChartSurfaceSeries s2;
    QVector<QDataPoint3D> p2;
    p2.append(QDataPoint3D(0, 0, 0));
    p2.append(QDataPoint3D(1, 0, 0));
    p2.append(QDataPoint3D(0, 1, 0));
    p2.append(QDataPoint3D(qQNaN(), 1, 0));   // (1,1) 为 NaN
    s2.setGrid(2, 2, p2);
    QVector<QChartPrimitive> out2;
    s2.collectPrimitives(kIdentityFn, out2);
    QCOMPARE(out2.size(), 2);   // 触及 NaN 点的两条段被跳过
}

// ===== 3. depthSort：depth 降序（远→近），近者在后 =====
void TestQChartRenderer3D::depthSort_farToNear() {
    QVector<QChartPrimitive> items;
    const qreal depths[] = { 5.0, 1.0, 10.0, 3.0 };
    for (qreal d : depths) {
        QChartPrimitive prim;
        prim.depth = d;
        items.append(prim);
    }
    // 与 Renderer drawForeground3D 相同的比较器：降序（大=depth 远 → 先画）
    std::sort(items.begin(), items.end(),
              [](const QChartPrimitive& a, const QChartPrimitive& b) { return a.depth > b.depth; });
    QCOMPARE(items[0].depth, 10.0);
    QCOMPARE(items[1].depth, 5.0);
    QCOMPARE(items[2].depth, 3.0);
    QCOMPARE(items[3].depth, 1.0);   // 近者位于序列末尾（后画，覆盖远者）
}

// ===== 4. render3d offscreen：不崩、非空白 =====
void TestQChartRenderer3D::render3d_offscreen_ok() {
    QChartCamera3D cam;   // 默认透视：pos(0,0,10) 看向原点
    QChartLayer3D layer;
    auto* line = new QChartLineSeries3D("l");   // 堆分配：所有权归 layer（dtor qDeleteAll）
    line->append(-5, 0, 0);
    line->append(0, 0, 0);
    line->append(5, 0, 0);
    line->setColor(Qt::red);
    layer.addSeries3D(line);

    QChartScene scene;
    scene.plotArea = QRectF(0, 0, 400, 300);
    scene.camera3D = &cam;
    scene.layers3D.append(&layer);

    QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainterChartRenderer renderer;
    renderer.renderUncached(scene, &img);

    // 非空白：存在非背景像素（全图扫描，命中即停）
    bool found = false;
    for (int y = 0; y < img.height() && !found; ++y)
        for (int x = 0; x < img.width() && !found; ++x)
            if (img.pixelColor(x, y) != QColor(Qt::white)) found = true;
    QVERIFY2(found, "3D 渲染应产生非背景像素");

    // 线段中心像素应为系列色（红）：y=0 世界线 → 屏幕 y≈150
    const QColor c = img.pixelColor(200, 150);
    QVERIFY(qAbs(c.red() - 255) < 40 && qAbs(c.green()) < 40 && qAbs(c.blue()) < 40);
}

// ===== 5. nearCoversFar：重叠线段远蓝近红 → 重叠像素为近者（红）=====
void TestQChartRenderer3D::render3d_nearCoversFar() {
    // 正交俯视：z=0（depth 10，近）与 z=-5（depth 15，远）两重叠水平线段
    QChartCamera3D cam;
    cam.setPosition(QVector3D(0, 0, 10));
    cam.setLookAt(QVector3D(0, 0, 0));
    cam.setUp(QVector3D(0, 1, 0));
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
    cam.setOrthographicBox(QRectF(-10, -10, 20, 20));
    cam.setNearPlane(0.1);
    cam.setFarPlane(100.0);

    QChartLayer3D layer;
    auto* far = new QChartLineSeries3D("far");   // 堆分配：所有权归 layer
    far->append(-2, 0, -5);
    far->append(2, 0, -5);
    far->setColor(Qt::blue);
    auto* near = new QChartLineSeries3D("near");
    near->append(-2, 0, 0);
    near->append(2, 0, 0);
    near->setColor(Qt::red);
    layer.addSeries3D(far);
    layer.addSeries3D(near);

    QChartScene scene;
    scene.plotArea = QRectF(0, 0, 400, 300);
    scene.camera3D = &cam;
    scene.layers3D.append(&layer);

    QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainterChartRenderer renderer;
    renderer.renderUncached(scene, &img);

    // 重叠中心像素 (200,150)：远(蓝)先画、近(红)后画覆盖 → 应为近者红
    const QColor c = img.pixelColor(200, 150);
    QVERIFY2(qAbs(c.red() - 255) < 40 && qAbs(c.green()) < 40 && qAbs(c.blue()) < 40,
             qPrintable(QString("重叠像素应为近者红色，实际 %1 (r=%2 g=%3 b=%4)")
                        .arg(c.name()).arg(c.red()).arg(c.green()).arg(c.blue())));
}

// ===== 6. scene.is3D：camera3D 非空 ↔ is3D()；2D 默认值零变化 =====
void TestQChartRenderer3D::scene_is3D_detection() {
    QChartScene scene;
    QVERIFY(!scene.is3D());
    QVERIFY(scene.camera3D == nullptr);
    QVERIFY(scene.layers3D.isEmpty());
    QVERIFY(scene.worldBounds.min == QVector3D(0, 0, 0));   // 默认盒零值

    QChartCamera3D cam;
    scene.camera3D = &cam;
    QVERIFY(scene.is3D());
    scene.camera3D = nullptr;
    QVERIFY(!scene.is3D());
}
