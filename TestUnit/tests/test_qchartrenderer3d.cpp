// test_qchartrenderer3d.cpp —— Renderer 3D 路径单元测试
// 覆盖（design_3d.md §11.1 TestQChartRenderer3D 全部 6 组）。
// 渲染走 QPainterChartRenderer::renderUncached（直绘，不经 QPixmap 缓存）。
#include <QtTest>
#include <QImage>
#include <algorithm>
#include <memory>
#include "../../QChartRenderer.h"
#include "../../QPainterChartRenderer.h"
#include "../../QChartCamera3D.h"
#include "../../QChartLayer3D.h"
#include "../../QChartLineSeries3D.h"
#include "../../QChartScatterSeries3D.h"
#include "../../QChartSurfaceSeries.h"
#include "../../QChartCartesianProjection3D.h"
#include "../../QChartCylindricalProjection3D.h"
#include "../../QChartSphericalProjection3D.h"
#include "../../QChartFunctionalProjection3D.h"
#include "../../QChartWidget3D.h"
#include "../../QValueAxis.h"
#include "test_qchartrenderer3d.h"

namespace {
    // 恒等全链闭包：Data → {screen=(x,y), depth=z}
    QChartProjectedPoint identityProject(const QDataPoint3D& d) {
        return QChartProjectedPoint{ QPointF(d.x().toDouble(), d.y().toDouble()), d.z().toDouble() };
    }
    const ProjectFn3D kIdentityFn = identityProject;

    // 测试夹具：三轴（tickCount 5、range 0..10）+ 盒 (0,0,0)-(10,10,10) + 正交俯视相机
    struct AxesFixture {
        QChartLayer3D layer;
        QValueAxis ax, ay, az;
        std::unique_ptr<QChartProjection3D> proj;
        QChartCamera3D cam;
        QVector<QChartPrimitive> items;
        QVector<QChartTextLabel> labels;

        explicit AxesFixture(std::unique_ptr<QChartProjection3D> p) {
            ax.setRange(0, 10); ax.setTickCount(5);
            ay.setRange(0, 10); ay.setTickCount(5);
            az.setRange(0, 10); az.setTickCount(5);
            layer.setAxisX(&ax);
            layer.setAxisY(&ay);
            layer.setAxisZ(&az);
            layer.setProjection3D(p.get());
            layer.setAxesDataBox(QVector3D(0, 0, 0), QVector3D(10, 10, 10));
            cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
            cam.setViewCube(QChartWorldBox{ QVector3D(-20, -20, -1), QVector3D(20, 20, 1) });
            cam.setYaw(0.0);
            cam.setPitch(0.0);
            proj = std::move(p);
        }
        void collect() {
            items.clear();
            labels.clear();
            layer.collectPrimitives(&cam, QRectF(0, 0, 400, 300), items, &labels);
        }
        int countLayer(QChartPrimitive::Layer L) const {
            int n = 0;
            for (const QChartPrimitive& prim : items)
                if (prim.layer == L) ++n;
            return n;
        }
        QVector<qreal> ticks(int dim) const { return layer.axes3D()->ticks(dim, 0, 10); }
    };
}

// ===== 7. 盒模式图元计数（§10.2 #9）：盒 12 边 + 地板 2×(T+1) 线 + tick 点 3×(T+1)，Layer 归属正确 =====
void TestQChartRenderer3D::boxMode_primitiveCounts() {
    AxesFixture f(std::make_unique<QChartCartesianProjection3D>());   // identity → 段数 2
    const int t0 = f.ticks(0).size(), t1 = f.ticks(1).size(), t2 = f.ticks(2).size();
    QVERIFY(t0 >= 2 && t1 >= 2 && t2 >= 2);   // 刻度数由 Axis 决定（0..10/tickCount5 → 6 个）
    const int S = 2;   // Cartesian3D samplingSegmentsHint（§5.4）

    f.collect();

    // Grid 层：盒模式地板 = w=wMin 平面 tick 网格（u 方向 t1 条 + v 方向 t0 条，每条 S 段）
    QCOMPARE(f.countLayer(QChartPrimitive::Layer::Grid), (t0 + t1) * S);

    // ForegroundDecor：盒 12 边 ×S + 刻度点（3 轴 × t 个，Point 不采样）
    QCOMPARE(f.countLayer(QChartPrimitive::Layer::ForegroundDecor), 12 * S + (t0 + t1 + t2));

    // Series 层：无系列 → 0
    QCOMPARE(f.countLayer(QChartPrimitive::Layer::Series), 0);

    // Layer 归属：Grid 全为 LineSegment、decor 含 Point（刻度）
    bool gridAllLines = true, decorHasPoints = false;
    for (const QChartPrimitive& prim : f.items) {
        if (prim.layer == QChartPrimitive::Layer::Grid && prim.type != QChartPrimitive::Type::LineSegment)
            gridAllLines = false;
        if (prim.layer == QChartPrimitive::Layer::ForegroundDecor && prim.type == QChartPrimitive::Type::Point)
            decorHasPoints = true;
    }
    QVERIFY(gridAllLines);
    QVERIFY(decorHasPoints);

    // labels：tick 标签 (t0+t1+t2) + 轴标题 3（dimensionName x/y/z）
    QCOMPARE(f.labels.size(), t0 + t1 + t2 + 3);
    int titleCount = 0;
    for (const QChartTextLabel& lbl : f.labels)
        if (lbl.isTitle) ++titleCount;
    QCOMPARE(titleCount, 3);

    // 默认（axesDataBox 无效）→ 零轴零网格（零回归）
    QChartLayer3D bare;
    QVector<QChartPrimitive> none;
    bare.collectPrimitives(&f.cam, QRectF(0, 0, 400, 300), none);
    QCOMPARE(none.size(), 0);
}

// ===== 8. 晶格模式行数（§10.2 #10）：三族 = (nu+1)(nw+1)+(nv+1)(nw+1)+(nu+1)(nv+1) =====
void TestQChartRenderer3D::lattice_rowCounts() {
    AxesFixture f(std::make_unique<QChartCartesianProjection3D>());
    f.layer.setGridMode(QChartLayer3D::GridMode::Lattice);
    const int t0 = f.ticks(0).size(), t1 = f.ticks(1).size(), t2 = f.ticks(2).size();
    const int S = 2;

    f.collect();

    // 三族行数 ×S 段
    const int lines = t0 * t2 + t1 * t2 + t0 * t1;
    QCOMPARE(f.countLayer(QChartPrimitive::Layer::Grid), lines * S);
    QCOMPARE(f.countLayer(QChartPrimitive::Layer::ForegroundDecor), 12 * S + (t0 + t1 + t2));
}

// ===== 9. 快速通道（§5.4）：Cartesian3D 每线 2 段 vs 球面 32 段 =====
void TestQChartRenderer3D::identityFastPath_layer3d() {
    // Cartesian3D：isIdentityMapping → 免 toWorld、段数 2
    {
        AxesFixture f(std::make_unique<QChartCartesianProjection3D>());
        const int t0 = f.ticks(0).size(), t1 = f.ticks(1).size();
        f.collect();
        QCOMPARE(f.countLayer(QChartPrimitive::Layer::Grid), (t0 + t1) * 2);
    }
    // 球面：非恒等 → toWorld 弯曲、段数 32
    {
        AxesFixture f(std::make_unique<QChartSphericalProjection3D>());
        const int t0 = f.ticks(0).size(), t1 = f.ticks(1).size();
        f.collect();
        QCOMPARE(f.countLayer(QChartPrimitive::Layer::Grid), (t0 + t1) * 32);
    }
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
    // R5 viewCube 相机：正交俯视（viewCube x/y=±10、z 覆盖数据平面、yaw0/pitch0）
    QChartCamera3D cam;
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
    cam.setViewCube(QChartWorldBox{ QVector3D(-10, -10, -1), QVector3D(10, 10, 1) });
    cam.setYaw(0.0);
    cam.setPitch(0.0);
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
    // R5 viewCube 相机：正交俯视；z=0（近）与 z=-5（远）两重叠水平线段
    QChartCamera3D cam;
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
    cam.setViewCube(QChartWorldBox{ QVector3D(-10, -10, -1), QVector3D(10, 10, 1) });
    cam.setYaw(0.0);
    cam.setPitch(0.0);

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

// ============================================================
// 分层像素断言（design_3d_axes.md §10.2 #11-15）
// 构造说明：用「线系列（Series，红）」替代文档中的球体（本质同：统一深度排序 + 偏置的
// 像素证明）；盒模式地板网格（Grid，绿）置于系列前方/后方/同深度；spine（Decor，轴色品红）。
// 相机：正交前视（viewCube ±5×±5×z∈[-1,1]、yaw0/pitch0）→ 世界 x/y 线性映射屏幕。
// 屏幕映射：px = 200 + 40x；py = 150 − 30y（y=0→150、y=-3→240）。
// ============================================================
namespace {
    struct PixelFixture {
        QChartLayer3D layer;
        QValueAxis ax, ay, az;
        std::unique_ptr<QChartProjection3D> proj;
        QChartCamera3D cam;
        QImage img;

        PixelFixture(const QVector3D& boxMin, const QVector3D& boxMax, qreal seriesY) {
            ax.setRange(-3, 3); ax.setTickCount(5); ax.setColor(Qt::magenta);
            ay.setRange(-3, 3); ay.setTickCount(5); ay.setColor(Qt::cyan);
            az.setRange(-3, 3); az.setTickCount(5); az.setColor(Qt::yellow);
            layer.setAxisX(&ax);
            layer.setAxisY(&ay);
            layer.setAxisZ(&az);
            proj = std::make_unique<QChartCartesianProjection3D>();
            layer.setProjection3D(proj.get());
            layer.setAxesDataBox(boxMin, boxMax);
            layer.setGridColor(QColor(0, 200, 0));   // 网格绿

            auto* series = new QChartLineSeries3D("s", &layer);
            series->append(-3, seriesY, 0);
            series->append(3, seriesY, 0);
            series->setColor(Qt::red);
            layer.addSeries3D(series);

            cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
            cam.setViewCube(QChartWorldBox{ QVector3D(-5, -5, -1), QVector3D(5, 5, 1) });
            cam.setYaw(0.0);
            cam.setPitch(0.0);
        }
        void render() {
            QChartScene scene;
            scene.plotArea = QRectF(0, 0, 400, 300);
            scene.camera3D = &cam;
            scene.layers3D.append(&layer);
            img = QImage(400, 300, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::white);
            QPainterChartRenderer renderer;
            renderer.renderUncached(scene, &img);
        }
    };
    bool nearColor(const QColor& c, int r, int g, int b, int tol = 40) {
        return qAbs(c.red() - r) < tol && qAbs(c.green() - g) < tol && qAbs(c.blue() - b) < tol;
    }
}

// ===== #11 球后网格被盖：地板 w=-5（系列 z=0 后方）→ 像素 = 系列色 =====
void TestQChartRenderer3D::gridBehindSeries_pixel() {
    PixelFixture f(QVector3D(-3, -3, -5), QVector3D(3, 3, -1), 0.0);
    f.render();
    // (200,150)：地板 y=0 网格线（远）+ 系列线（近）重叠 → 系列盖网格 → 红
    const QColor c = f.img.pixelColor(200, 150);
    QVERIFY2(nearColor(c, 255, 0, 0),
             qPrintable(QString("后方网格应被系列盖住（红），实际 %1").arg(c.name())));
}

// ===== #12 球前网格遮挡：地板 w=1（系列前方）→ 像素 = 网格色 =====
void TestQChartRenderer3D::gridInFrontOfSeries_pixel() {
    PixelFixture f(QVector3D(-3, -3, 1), QVector3D(3, 3, 5), 0.0);
    f.render();
    const QColor c = f.img.pixelColor(200, 150);
    QVERIFY2(nearColor(c, 0, 200, 0),
             qPrintable(QString("前方网格应遮挡系列（绿），实际 %1").arg(c.name())));
}

// ===== #13 同深度系列优先：地板 w=0 == 系列 z=0 → kGridDepthBias 生效 → 系列色 =====
void TestQChartRenderer3D::gridTie_seriesWins() {
    PixelFixture f(QVector3D(-3, -3, 0), QVector3D(3, 3, 5), 0.0);
    f.render();
    const QColor c = f.img.pixelColor(200, 150);
    QVERIFY2(nearColor(c, 255, 0, 0),
             qPrintable(QString("同深度处系列应赢（红，偏置生效），实际 %1").arg(c.name())));
}

// ===== #14 decor 恒可见：u-spine（y=-3,z=-1，比系列更远）仍画在系列之上 =====
void TestQChartRenderer3D::decorAlwaysOnTop() {
    PixelFixture f(QVector3D(-3, -3, -1), QVector3D(3, 3, 1), -3.0);   // 系列 y=-3 与 u-spine 屏幕重合
    f.render();
    // (200,240)：系列线（z=0）+ 网格（z=-1）+ u-spine（z=-1，Decor）重叠 → spine 恒后画 → 品红
    const QColor c = f.img.pixelColor(200, 240);
    QVERIFY2(nearColor(c, 255, 0, 255),
             qPrintable(QString("decor(spine) 应恒在系列/网格之上（品红），实际 %1").arg(c.name())));
}

// ===== #15 axes3D 关闭（等价 'A' 键）→ 无 grid/decor/labels 图元与轴色像素 =====
void TestQChartRenderer3D::axesToggle_zero() {
    PixelFixture f(QVector3D(-3, -3, -1), QVector3D(3, 3, 1), -3.0);

    // 开：有 grid/decor 图元 + labels；像素 = spine 品红（decor 在系列之上）
    {
        QVector<QChartPrimitive> items;
        QVector<QChartTextLabel> labels;
        f.layer.collectPrimitives(&f.cam, QRectF(0, 0, 400, 300), items, &labels);
        int gridCount = 0, decorCount = 0;
        for (const QChartPrimitive& prim : items) {
            if (prim.layer == QChartPrimitive::Layer::Grid) ++gridCount;
            if (prim.layer == QChartPrimitive::Layer::ForegroundDecor) ++decorCount;
        }
        QVERIFY(gridCount > 0);
        QVERIFY(decorCount > 0);
        QVERIFY(!labels.isEmpty());

        f.render();
        QVERIFY(nearColor(f.img.pixelColor(200, 240), 255, 0, 255));
    }

    // 关（'A' 键等价）：零 grid/decor/labels；轴色像素消失（只剩系列红）
    f.layer.axes3D()->setVisible(false);
    {
        QVector<QChartPrimitive> items;
        QVector<QChartTextLabel> labels;
        f.layer.collectPrimitives(&f.cam, QRectF(0, 0, 400, 300), items, &labels);
        int gridCount = 0, decorCount = 0;
        for (const QChartPrimitive& prim : items) {
            if (prim.layer == QChartPrimitive::Layer::Grid) ++gridCount;
            if (prim.layer == QChartPrimitive::Layer::ForegroundDecor) ++decorCount;
        }
        QCOMPARE(gridCount, 0);
        QCOMPARE(decorCount, 0);
        QVERIFY(labels.isEmpty());

        f.render();
        QVERIFY(nearColor(f.img.pixelColor(200, 240), 255, 0, 0));   // 系列红，无轴色像素
    }
}

// ============================================================
// Widget3D 控制器（design_3d_axes.md §10.2 #16-18；§2.2/§3）
// ============================================================

// ===== #16 Cartesian 快速通道：反算 dataBounds == viewCube 本身（免采样）=====
void TestQChartRenderer3D::dataBounds3D_viewCubeReverse() {
    QChartWidget3D w;
    w.setProjection3D(std::make_unique<QChartCartesianProjection3D>());   // fitWorld（A3 链）
    QVERIFY(w.dataBounds3DValid());   // Cartesian 恒等 → 反算恒有效

    // 反算 == viewCube（视图变化钩子：setViewCube → viewChanged → recompute）
    w.camera3D()->setViewCube(QChartWorldBox{ QVector3D(-3, -2, -1), QVector3D(4, 5, 6) });
    QCOMPARE(w.dataBounds3DMin(), QVector3D(-3, -2, -1));
    QCOMPARE(w.dataBounds3DMax(), QVector3D(4, 5, 6));

    // 正交俯视退化（§2.2 论证）：viewCube = {viewRect 范围, z 覆盖数据平面}
    w.camera3D()->setViewCube(QChartWorldBox{ QVector3D(0, 0, -1), QVector3D(10, 10, 1) });
    QCOMPARE(w.dataBounds3DMin(), QVector3D(0, 0, -1));
    QCOMPARE(w.dataBounds3DMax(), QVector3D(10, 10, 1));

    // orbit 后仍 == viewCube（viewCube 不动，R6）
    w.camera3D()->orbit(30, 20);
    QCOMPARE(w.dataBounds3DMin(), QVector3D(0, 0, -1));
    QCOMPARE(w.dataBounds3DMax(), QVector3D(10, 10, 1));
}

// ===== #17 柱坐标 5³ 采样：捕获棱中点极值（8 角会漏）=====
void TestQChartRenderer3D::dataBounds3D_gridSampling_curved() {
    QChartWidget3D w;
    w.setProjection3D(std::make_unique<QChartCylindricalProjection3D>());

    // viewCube：x∈[0.5,1.5]、y∈[-0.5,0.5]、z∈[-1,1]；r=√(x²+y²) 最小值在 (x=0.5, y=0)
    // （y=0 采样档，盒棱中点）→ r_min=0.5；8 角方案只会得到角点 r=√(0.5²+0.5²)≈0.707
    w.camera3D()->setViewCube(QChartWorldBox{ QVector3D(0.5f, -0.5f, -1.0f), QVector3D(1.5f, 0.5f, 1.0f) });
    QVERIFY(w.dataBounds3DValid());

    QVERIFY2(w.dataBounds3DMin().x() <= 0.5 + 1e-6,
             qPrintable(QString("5³ 采样应捕获 r_min=0.5（棱中点极值），实际 %1")
                        .arg(w.dataBounds3DMin().x())));
    QVERIFY2(w.dataBounds3DMin().x() < 0.7,
             "r_min 应显著小于 8 角方案的角点值 0.707（证明采样捕获棱中点极值）");
    // r_max = √(1.5²+0.5²) ≈ 1.581（角点，采样含）
    QVERIFY(qAbs(w.dataBounds3DMax().x() - 1.58114) < 1e-3);
}

// ===== #18 Functional 无反向：Valid=false → 轴/网格用锚定域盒（静态，相机变化后不变）=====
void TestQChartRenderer3D::dataBounds3D_noBackward_fallback() {
    QChartWidget3D w;
    w.setProjection3D(std::make_unique<QChartFunctionalProjection3D>(
        [](qreal u, qreal v, qreal /*n2*/) -> QVector3D {
            const qreal uRad = qDegreesToRadians(u), vRad = qDegreesToRadians(v);
            return QVector3D(qCos(vRad) * qCos(uRad), qCos(vRad) * qSin(uRad), qSin(vRad));
        },
        nullptr,   // 无反向 → fromWorld 全 NaN
        QVector3D(0, -90, 0), QVector3D(360, 90, 0), nullptr, "u", "v", "w"));

    // fromWorld 全 NaN → dataBounds3DValid == false（A9 兜底）
    QVERIFY(!w.dataBounds3DValid());

    // 带轴图层：轴盒 = 锚定域盒（defaultDataBounds，静态）
    QValueAxis ax, ay, az;
    ax.setRange(0, 360);
    ay.setRange(-90, 90);
    az.setRange(0, 0);
    auto* layer = new QChartLayer3D(&w);
    layer->setAxisX(&ax);
    layer->setAxisY(&ay);
    layer->setAxisZ(&az);
    w.addLayer3D(layer);

    auto collectCounts = [&](int& gridOut, int& decorOut) {
        QVector<QChartPrimitive> items;
        layer->collectPrimitives(w.camera3D(), QRectF(0, 0, 400, 300), items);
        gridOut = 0;
        decorOut = 0;
        for (const QChartPrimitive& prim : items) {
            if (prim.layer == QChartPrimitive::Layer::Grid) ++gridOut;
            if (prim.layer == QChartPrimitive::Layer::ForegroundDecor) ++decorOut;
        }
    };

    int g1 = 0, d1 = 0, g2 = 0, d2 = 0;
    collectCounts(g1, d1);
    QVERIFY(g1 > 0);   // 锚定域盒有效 → 网格/装饰生成
    QVERIFY(d1 > 0);

    // 相机 orbit 后：仍 invalid；锚定域盒静态 → 图元数量不随相机变化
    w.camera3D()->orbit(30, 20);
    QVERIFY(!w.dataBounds3DValid());
    collectCounts(g2, d2);
    QCOMPARE(g1, g2);
    QCOMPARE(d1, d2);
}
