// test_qopenglrenderer.cpp —— GL 渲染器（Shader 主 pass，design_phase3.md §4/§5）单元测试
// 真实 GL 环境（xcb/wayland，llvmpipe 可跑）；offscreen 平台 initTestCase QSKIP（§13.2）。
// 场景构造复刻 test_qchartrenderer3d::PixelFixture（同相机/轴/盒 → 屏幕映射 px=200+40x, py=150−30y），
// 使 GL 与 QPainter 两后端的深度语义断言可直接对照。
#include <QtTest>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <cstring>
#include "QOpenGLChartRenderer.h"
#include "QChartGL.h"
#include "QChartLayer3D.h"
#include "QChartCamera3D.h"
#include "QChartSeries3D.h"
#include "QChartLineSeries3D.h"
#include "QChartScatterSeries3D.h"
#include "QChartCartesianProjection3D.h"
#include "QChartRenderer.h"
#include "QPainterChartRenderer.h"
#include "QValueAxis.h"
#include "test_qopenglrenderer.h"

namespace {
    struct GLFixture {
        QChartLayer3D layer;
        QValueAxis ax, ay, az;
        std::unique_ptr<QChartProjection3D> proj;
        QChartCamera3D cam;
        QOpenGLWidget w;                 // 宿主（须先于 r 声明：r(&w)）
        QOpenGLChartRenderer r;
        QChartScene scene;

        GLFixture(const QVector3D& boxMin, const QVector3D& boxMax, qreal seriesY)
            : proj(std::make_unique<QChartCartesianProjection3D>()), w(), r(&w) {
            ax.setRange(-3, 3); ax.setTickCount(5); ax.setColor(Qt::magenta);
            ay.setRange(-3, 3); ay.setTickCount(5); ay.setColor(Qt::cyan);
            az.setRange(-3, 3); az.setTickCount(5); az.setColor(Qt::yellow);
            layer.setAxisX(&ax);
            layer.setAxisY(&ay);
            layer.setAxisZ(&az);
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

            w.setFormat(QChartGL::surfaceFormat());
            w.resize(400, 300);
            w.show();
            exposed = QTest::qWaitForWindowExposed(&w);   // GL 上下文初始化（show 后）

            scene.plotArea = QRectF(0, 0, 400, 300);
            scene.backgroundColor = QColor(Qt::white);   // 与 QPainter 侧 img.fill(white) 一致
            scene.camera3D = &cam;
            scene.layers3D.append(&layer);
        }
        ~GLFixture() { w.hide(); }
        /// 窗口已显示（GL 上下文初始化）——测试内断言（QVERIFY 可用于测试调用的辅助函数）
        void requireExposed() const { QVERIFY(exposed); }
        bool exposed = false;
    };

    /// GL 渲染 → 读回 400×300 图像（QPainter 顶行序；GL 行序底→顶翻转）
    QImage renderGL(GLFixture& fx) {
        fx.requireExposed();
        fx.w.makeCurrent();
        auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(fx.w.context());
        Q_ASSERT(f);
        f->glBindFramebuffer(GL_FRAMEBUFFER, fx.w.defaultFramebufferObject());
        fx.r.initializeGL();
        fx.r.paintGL(fx.scene);
        QVector<uchar> buf(400 * 300 * 4);
        f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        f->glReadPixels(0, 0, 400, 300, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
        fx.w.doneCurrent();
        QImage img(400, 300, QImage::Format_RGBA8888);
        for (int y = 0; y < 300; ++y)
            std::memcpy(img.scanLine(y), buf.constData() + (299 - y) * 400 * 4, 400 * 4);
        return img;
    }

    /// QPainter 路径同场景渲染（对照基线）
    QImage renderPainter(const QChartScene& scene) {
        QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        QPainterChartRenderer renderer;
        renderer.renderUncached(scene, &img);
        return img;
    }

    bool nearColor(const QColor& c, int r, int g, int b, int tol = 60) {
        return qAbs(c.red() - r) < tol && qAbs(c.green() - g) < tol && qAbs(c.blue() - b) < tol;
    }
}

void TestQOpenGLRenderer::initTestCase() {
    // §13.2 运行环境：offscreen 平台无真实 GL → 整类 QSKIP（GL 用例不判失败；实跑转 t50/Windows）
    if (QGuiApplication::platformName() == "offscreen")
        QSKIP("offscreen 平台无真实 GL：GL 渲染用例跳过（§13.2；实跑见 wayland/xcb 手动运行或 t50）");
    // 环境预检：共享根可建（llvmpipe 软渲染即可）——不可用则整类跳过（A8 冒烟不判失败）
    QChartGL::registerHost();
    const bool glOk = (QChartGL::sharedContext() != nullptr);
    QChartGL::unregisterHost();
    if (!glOk)
        QSKIP("本环境 GL 不可用（EGL/DRI2 故障？）：GL 用例跳过，实跑转 t50/Windows 侧（A8）");
}

// ===== 1. 批次结构：图元→批次（合并/顶点数/baseId 连续/pointSize/深度标志）=====
void TestQOpenGLRenderer::batch_structure() {
    // gridTie 场景 + 额外散点（markerSize 6.5）→ 点批次 pointSize 校验
    GLFixture fx(QVector3D(-3, -3, 0), QVector3D(3, 3, 5), 0.0);
    auto* scat = new QChartScatterSeries3D("p", &fx.layer);
    scat->append(0, 0, 0);
    scat->setMarkerSize(6.5);
    scat->setColor(Qt::blue);
    fx.layer.addSeries3D(scat);

    fx.requireExposed();
    fx.w.makeCurrent();
    fx.r.initializeGL();
    QVERIFY(fx.r.isReady());
    fx.r.paintGL(fx.scene);   // 触发 buildBatches + 主 pass
    const BatchPool& pool = fx.r.m_batches;   // friend 直查（§13.2 批次结构）

    QVERIFY(!pool.batches.isEmpty());
    int totalPrims = 0;
    bool foundSeriesLine = false, foundSeriesPoint = false, foundGrid = false;
    for (const GLBatch& b : pool.batches) {
        const int vertsPer = (b.primitive == GL_POINTS) ? 1 : 2;
        QVERIFY(b.vertexCount % vertsPer == 0);
        QCOMPARE(b.baseId, totalPrims);   // baseId 连续 = 图元 ID 基址（§5.3 零内存）
        totalPrims += b.vertexCount / vertsPer;
        if (b.layer == QChartPrimitive::Layer::Grid) {
            foundGrid = true;
            QVERIFY(b.depthTest);
            QVERIFY(b.depthBias > 0.0);   // Grid 偏置生效（§5.2）
        }
        if (b.layer == QChartPrimitive::Layer::ForegroundDecor) {
            QVERIFY(!b.depthTest);        // decor 关深度后画（A4 恒可见）
            QCOMPARE(b.depthBias, 0.0);
        }
        if (b.layer == QChartPrimitive::Layer::Series && b.primitive == GL_LINES) {
            foundSeriesLine = true;
            QCOMPARE(b.vertexCount, 2);   // 1 线图元 → 2 顶点
            QCOMPARE(b.depthBias, 0.0);
        }
        if (b.layer == QChartPrimitive::Layer::Series && b.primitive == GL_POINTS) {
            foundSeriesPoint = true;
            QCOMPARE(b.vertexCount, 1);   // 1 散点图元 → 1 顶点
            QCOMPARE(b.pointSize, 6.5f);  // markerSize 按批次 uniform（§3.1）
        }
    }
    QVERIFY(foundGrid);
    QVERIFY(foundSeriesLine);
    QVERIFY(foundSeriesPoint);
    // pickTable 与图元一一对应（§8.1；含轴/网格装饰 dataIndex=-1 记录）
    QCOMPARE(pool.pickTable.size(), totalPrims);
    // 系列图元归属正确（dataIndex ≥ 0 且 series 非空）
    bool seriesRecord = false;
    for (const QChartHitTester::PickRecord& rec : pool.pickTable) {
        if (rec.layer == QChartPrimitive::Layer::Series) {
            seriesRecord = true;
            QVERIFY(rec.series != nullptr);
        }
    }
    QVERIFY(seriesRecord);
    fx.w.doneCurrent();
}

// ===== 2. 深度语义：Grid 偏置（同深度系列优先）+ decor 关深度后画 + 不透明清屏 =====
void TestQOpenGLRenderer::depth_layering_pixels() {
    // (a) 同深度 tie：地板 w=0 == 系列 z=0 → 网格偏置生效 → 系列红（§5.2，A4）
    {
        GLFixture fx(QVector3D(-3, -3, 0), QVector3D(3, 3, 5), 0.0);
        const QImage img = renderGL(fx);
        const QColor c = img.pixelColor(200, 150);
        QVERIFY2(nearColor(c, 255, 0, 0),
                 qPrintable(QString("同深度处系列应赢（红，Grid 偏置生效），实际 %1").arg(c.name())));
    }
    // (b) 网格在前（地板 w=1 > 系列 z=0）→ 网格绿遮挡系列
    {
        GLFixture fx(QVector3D(-3, -3, 1), QVector3D(3, 3, 5), 0.0);
        const QImage img = renderGL(fx);
        const QColor c = img.pixelColor(200, 150);
        QVERIFY2(nearColor(c, 0, 200, 0),
                 qPrintable(QString("前方网格应遮挡系列（绿，z-buffer），实际 %1").arg(c.name())));
    }
    // (c) decor 恒可见：u-spine（y=-3,z=-1，比系列更远）关深度后画 → 品红
    {
        GLFixture fx(QVector3D(-3, -3, -1), QVector3D(3, 3, 1), -3.0);
        const QImage img = renderGL(fx);
        const QColor c = img.pixelColor(200, 240);
        QVERIFY2(nearColor(c, 255, 0, 255),
                 qPrintable(QString("decor(spine) 应恒在系列/网格之上（品红，关深度），实际 %1").arg(c.name())));
    }
    // (d) 不透明清屏（§5.1 ⚠ 透明语义教训）：背景像素 = 场景背景色白（非透明/非黑）
    {
        GLFixture fx(QVector3D(-3, -3, 0), QVector3D(3, 3, 5), 0.0);
        const QImage img = renderGL(fx);
        const QColor c = img.pixelColor(20, 20);
        QVERIFY2(nearColor(c, 255, 255, 255),
                 qPrintable(QString("背景应不透明白（清屏=场景背景色 alpha 1.0），实际 %1").arg(c.name())));
    }
}

// ===== 3. 深度偏置等价性：同场景 GL 与 QPainter 输出一致（A4 硬指标）=====
void TestQOpenGLRenderer::gl_qpainter_equivalence() {
    // gridTie：同深度系列优先——两后端都必须输出系列红
    GLFixture fx(QVector3D(-3, -3, 0), QVector3D(3, 3, 5), 0.0);
    const QImage glImg = renderGL(fx);
    const QImage qpImg = renderPainter(fx.scene);

    // 关键点：同深度重叠 (200,150) ——两后端同为系列红（±容差内互相一致）
    const QColor glC = glImg.pixelColor(200, 150);
    const QColor qpC = qpImg.pixelColor(200, 150);
    QVERIFY2(nearColor(glC, 255, 0, 0), qPrintable(QString("GL 同深度系列应红，实际 %1").arg(glC.name())));
    QVERIFY2(nearColor(qpC, 255, 0, 0), qPrintable(QString("QPainter 同深度系列应红，实际 %1").arg(qpC.name())));
    QVERIFY2(qAbs(glC.red() - qpC.red()) < 60 && qAbs(glC.green() - qpC.green()) < 60,
             qPrintable(QString("两后端同深度输出应一致（红），GL=%1 QPainter=%2")
                        .arg(glC.name()).arg(qpC.name())));

    // 背景：两后端均为白
    QVERIFY(nearColor(glImg.pixelColor(20, 20), 255, 255, 255));
    QVERIFY(nearColor(qpImg.pixelColor(20, 20), 255, 255, 255));
}

// ============================================================
// ID 帧拾取（design_phase3.md §5.3/§8.1，t46）
// ============================================================
namespace {
    QChartHitTester::HitResult decodePick(QRgb id, GLFixture& fx) {
        return QChartHitTester::hitTestGPU(qRed(id), qGreen(id), qBlue(id), fx.r.pickTable());
    }
}

// ===== 1. 可见图元命中：系列线 dataIndex；背景哨兵 =====
void TestQOpenGLRenderer::pick_hitVisible() {
    // gridTie（地板 z=0 == 系列 z=0）：同深度偏置 → 系列赢 → 系列线 id
    GLFixture fx(QVector3D(-3, -3, 0), QVector3D(3, 3, 5), 0.0);
    renderGL(fx);   // 主 pass（m_glReady + 批次就绪）
    const QRgb id = fx.r.pickIdAt(QPoint(200, 150), fx.scene);
    QVERIFY2(id != 0xFFFFFF, "系列线上拾取应命中（非哨兵）");
    const QChartHitTester::HitResult r = decodePick(id, fx);
    QVERIFY(r.series != nullptr);
    QCOMPARE(r.dataIndex, 0);   // 线系列首段起点索引

    // 背景（无图元）→ 哨兵（§5.3：清屏白 0xFFFFFF）→ 空
    QTest::qWait(30);   // 过 16ms 限流闸
    const QRgb bg = fx.r.pickIdAt(QPoint(20, 20), fx.scene);
    QCOMPARE(bg, qRgb(255, 255, 255));
    QCOMPARE(decodePick(bg, fx).dataIndex, -1);
}

// ===== 2. 遮挡与哨兵：网格遮挡系列 → 哨兵；decor 位置 → 哨兵 =====
void TestQOpenGLRenderer::pick_occlusionSentinel() {
    // 网格在前（地板 w=1 > 系列 z=0）：(200,150) 网格遮挡系列 → ID 帧深度 → 网格片段
    // → 轴/网格批次 0xFFFFFF 哨兵（§5.3 定案：不参与拾取，但仍渲染维持深度）
    GLFixture fx(QVector3D(-3, -3, 1), QVector3D(3, 3, 5), 0.0);
    renderGL(fx);
    QCOMPARE(fx.r.pickIdAt(QPoint(200, 150), fx.scene), qRgb(255, 255, 255));

    // decor（spine，y=-3 与系列重合）：Decor 批次 → 哨兵
    QTest::qWait(30);
    GLFixture fx2(QVector3D(-3, -3, -1), QVector3D(3, 3, 1), -3.0);
    renderGL(fx2);
    QCOMPARE(fx2.r.pickIdAt(QPoint(200, 240), fx2.scene), qRgb(255, 255, 255));
}

// ===== 3. ID 帧自包含：无主 pass（首帧前）拾取深度正确（§5.3 ★）=====
void TestQOpenGLRenderer::pick_selfContained() {
    // 仅初始化（无 paintGL 主 pass）→ 直接拾取：ID 帧自包含（清 depth + 写 depth +
    // 同主 pass 批次顺序/深度语义）——不依赖主 pass 深度状态/时序
    GLFixture fx(QVector3D(-3, -3, 0), QVector3D(3, 3, 5), 0.0);
    fx.w.makeCurrent();
    fx.r.initializeGL();
    fx.w.doneCurrent();
    QVERIFY(fx.r.isReady());

    // 同深度 tie：ID 帧内 Grid 偏置生效 → 系列赢 → 系列 id（批次 dirty → pickIdAt 内构建）
    const QRgb id = fx.r.pickIdAt(QPoint(200, 150), fx.scene);
    qInfo().noquote() << "ID 帧自包含拾取: id=" << id
                      << "table=" << fx.r.pickTable().size()
                      << "decode=" << decodePick(id, fx).dataIndex;
    QVERIFY2(id != 0xFFFFFF, "ID 帧自包含：无主 pass 也应命中可见系列（偏置生效）");
    QCOMPARE(decodePick(id, fx).dataIndex, 0);

    // 网格在前：ID 帧深度正确 → 网格（哨兵）盖系列
    GLFixture fx2(QVector3D(-3, -3, 1), QVector3D(3, 3, 5), 0.0);
    fx2.w.makeCurrent();
    fx2.r.initializeGL();
    fx2.w.doneCurrent();
    QTest::qWait(30);   // 限流闸（上一条目仍有效）
    QCOMPARE(fx2.r.pickIdAt(QPoint(200, 150), fx2.scene), qRgb(255, 255, 255));
}

// ===== 4. 三闸限流（§5.3）：③m_glReady 守卫 / ①位移<1px / ②<16ms =====
void TestQOpenGLRenderer::pick_throttle() {
    GLFixture fx(QVector3D(-3, -3, 0), QVector3D(3, 3, 5), 0.0);
    // ③ m_glReady==false（未初始化）→ 哨兵跳过（不渲染，§5.3 第三道闸）
    QCOMPARE(fx.r.pickIdAt(QPoint(200, 150), fx.scene), qRgb(255, 255, 255));
    renderGL(fx);   // 初始化 + 主 pass
    QVERIFY(fx.r.isReady());

    // 首次拾取（真实渲染）→ 命中系列线
    const QRgb r1 = fx.r.pickIdAt(QPoint(200, 150), fx.scene);
    QVERIFY2(r1 != 0xFFFFFF, "首次拾取应命中（非哨兵）");
    QCOMPARE(decodePick(r1, fx).dataIndex, 0);

    // ① 位移 <1px（同位置）→ 保持上次结果（不重渲染）
    QCOMPARE(fx.r.pickIdAt(QPoint(200, 150), fx.scene), r1);

    // ② 距上次 <16ms（第二道闸）：llvmpipe 软渲染单次拾取实测 ~59ms，无法用真实墙钟命中 <16ms 窗；
    //    直接置最近拾取时刻为 5ms 前（friend 直改，等价模拟快速 GPU 上的连续事件）→ 新位置限流保持上次结果
    fx.r.m_lastPickMs = fx.r.m_pickClock.elapsed() - 5;
    QCOMPARE(fx.r.pickIdAt(QPoint(20, 20), fx.scene), r1);

    // 等待 ≥16ms 后 → 真实渲染新位置（背景 → 哨兵；证明限流窗过后恢复渲染）
    QTest::qWait(30);
    QCOMPARE(fx.r.pickIdAt(QPoint(20, 20), fx.scene), qRgb(255, 255, 255));
}

