// test_axis_matrix.cpp —— S0 轴渲染矩阵集成测试（见头文件注释）
#include "test_axis_matrix.h"

#include <QtTest>
#include <QGuiApplication>
#include <QImage>
#include <QColor>
#include <QOpenGLWidget>
#include <QPainter>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <memory>

#include "QPainterChartRenderer.h"
#include "QOpenGLChartRenderer.h"
#include "QChartGL.h"
#include "QValueAxis.h"
#include "QChartScene.h"
#include "QChartCamera.h"
#include "QCartesianProjection.h"
#include "QPolarProjection.h"

namespace {

enum class ProjectionKind { Cartesian, Polar };

constexpr qreal kViewLo = -10.0;
constexpr qreal kViewHi =  10.0;
constexpr int    kSize  = 400;

/// 与 Test/unit/test_axis_pipeline 同一场景构造语义（双端共用，保证矩阵可比）
struct AxisFixture {
    QChartCamera camera;
    QValueAxis dim0Axis;
    QValueAxis dim1Axis;
    std::unique_ptr<QChartProjection> projection;
    QChartScene scene;

    explicit AxisFixture(ProjectionKind kind)
        : dim0Axis(nullptr, Qt::AlignBottom)
        , dim1Axis(nullptr, Qt::AlignLeft)
    {
        dim0Axis.setTickCount(5);
        dim1Axis.setTickCount(5);
        dim0Axis.setColor(Qt::black);
        dim1Axis.setColor(Qt::black);
        camera.setViewRect(QRectF(kViewLo, kViewLo, kViewHi - kViewLo, kViewHi - kViewLo));
        projection = (kind == ProjectionKind::Cartesian)
            ? std::unique_ptr<QChartProjection>(new QCartesianProjection)
            : std::unique_ptr<QChartProjection>(new QPolarProjection);
        scene.camera = &camera;
        scene.projection = projection.get();
        scene.plotArea = QRectF(0, 0, kSize, kSize);
        scene.backgroundColor = Qt::white;
    }

    void build(bool grid, bool labels)
    {
        // 批次1：夹具 bool 语义映射 true→Tickwise / false→None（Single 由标签契约专项测试覆盖）
        const QChartAxis::LabelMode labelMode = labels ? QChartAxis::LabelMode::Tickwise
                                                       : QChartAxis::LabelMode::None;
        scene.primitives.clear();
        scene.labels.clear();

        if (projection->type() == QChartAbstractProjection::CoordinateSystem::Cartesian) {
            dim0Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 0, scene, 72, labelMode);
            dim1Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 1, scene, 72, labelMode);
            if (grid) {
                const QVector<qreal> ty = dim1Axis.tickValues(kViewLo, kViewHi);
                for (qreal v : ty) { if (qAbs(v) < 1e-9) continue;
                    dim0Axis.drawAtPosition(kViewLo, kViewHi, v, 0.0, 0, scene, 8, QChartAxis::LabelMode::None); }
                const QVector<qreal> tx = dim0Axis.tickValues(kViewLo, kViewHi);
                for (qreal v : tx) { if (qAbs(v) < 1e-9) continue;
                    dim1Axis.drawAtPosition(kViewLo, kViewHi, v, 0.0, 1, scene, 8, QChartAxis::LabelMode::None); }
            }
        } else {
            dim0Axis.drawAtPosition(0.0, 360.0, 10.0, 0.0, 0, scene, 90, labelMode);
            dim1Axis.drawAtPosition(0.0, 10.0, 0.0, 0.0, 1, scene, 8, labelMode);
            if (grid) {
                const QVector<qreal> tr = dim1Axis.tickValues(0.0, 10.0);
                for (qreal r : tr) { if (r < 1e-9 || qAbs(r - 10.0) < 1e-9) continue;
                    dim0Axis.drawAtPosition(0.0, 360.0, r, 0.0, 0, scene, 90, QChartAxis::LabelMode::None); }
                const QVector<qreal> tt = dim0Axis.tickValues(0.0, 360.0);
                for (qreal th : tt) { if (qAbs(th) < 1e-9) continue;
                    dim1Axis.drawAtPosition(0.0, 10.0, th, 0.0, 1, scene, 8, QChartAxis::LabelMode::None); }
            }
        }
    }
};

QPoint pixOf(qreal x, qreal y)
{
    const qreal s = kSize / (kViewHi - kViewLo);
    return QPoint(qRound((x - kViewLo) * s), qRound((kViewHi - y) * s));
}

bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}

int inkCount(const QImage& img, int cx, int cy, int radius)
{
    int n = 0;
    for (int y = qMax(0, cy - radius); y <= qMin(img.height() - 1, cy + radius); ++y)
        for (int x = qMax(0, cx - radius); x <= qMin(img.width() - 1, cx + radius); ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}

int inkCountAll(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}

int inkInRect(const QImage& img, const QRect& r)
{
    int n = 0;
    const int x0 = qMax(0, r.left()), y0 = qMax(0, r.top());
    const int x1 = qMin(img.width() - 1, r.right()), y1 = qMin(img.height() - 1, r.bottom());
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}

QImage renderCpu(AxisFixture& f)
{
    QImage img(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainterChartRenderer r;
    r.render(f.scene, &img);
    return img;
}

/// 组合行的通用断言（CPU/GL 输出同构图；区域采样避开 Y 翻转 ±1px 差异）
void verifyCombo(const QImage& img, ProjectionKind kind, bool grid, bool labels,
                 const QImage* labelDev, const char* backend)
{
    QVERIFY2(inkCountAll(img) > 300,
             qPrintable(QString("[%1] 轴脊应产生大量非白像素").arg(backend)));

    // 主轴脊恒定可见：笛卡尔中心交点 / 极坐标原点（径向脊）
    const QPoint pc = kind == ProjectionKind::Cartesian ? pixOf(0.0, 0.0) : pixOf(0.0, 0.0);
    QVERIFY2(inkCount(img, pc.x(), pc.y(), 4) > 0,
             qPrintable(QString("[%1] 中心/原点轴脊应落屏 @%2,%3")
                        .arg(backend).arg(pc.x()).arg(pc.y())));
    if (kind == ProjectionKind::Cartesian) {
        // X 轴脊行（采样远离 Y 标签文字区）
        const QPoint px = pixOf(-2.5, 0.0);
        QVERIFY2(inkCount(img, px.x(), px.y(), 4) > 0,
                 qPrintable(QString("[%1] X 轴脊应落屏").arg(backend)));

        // F2 刻度点邻域断言：drawAtPosition 每个主刻度产生 7 点（中心+6 臂）。
        // 臂点离轴脊线 6px（cart ±0.3），2px 点光栅 ±1px 取整 → 半径 2 邻域采样，
        // 不做单像素精确断言。grid=on 时垂直网格线也过该列，但 grid=off 行可独立证明点光栅。
        const QPoint arms[4] = {
            pixOf(-4.0,  0.3),   // X 刻度 x=-4 的 ±y 臂
            pixOf(-4.0, -0.3),
            pixOf( 0.3,  4.0),   // Y 刻度 y=4 的 ±x 臂
            pixOf(-0.3,  4.0),
        };
        for (const QPoint& arm : arms) {
            QVERIFY2(inkCount(img, arm.x(), arm.y(), 2) > 0,
                     qPrintable(QString("[%1] 刻度点臂未出墨 @%2,%3")
                                .arg(backend).arg(arm.x()).arg(arm.y())));
        }
    } else {
        const QPoint pr = pixOf(2.0, 0.0);
        QVERIFY2(inkCount(img, pr.x(), pr.y(), 4) > 0,
                 qPrintable(QString("[%1] 极坐标径向脊应落屏").arg(backend)));
        const QPoint po = pixOf(7.0710678, 7.0710678);
        QVERIFY2(inkCount(img, po.x(), po.y(), 5) > 0,
                 qPrintable(QString("[%1] 极坐标外环应落屏").arg(backend)));
    }

    // 网格开关：采样点离轴脊/标签足够远
    QPoint pg;
    if (kind == ProjectionKind::Cartesian)
        pg = pixOf(4.0, 6.0);          // 垂直网格线 x=4
    else
        pg = pixOf(2.8284271, 2.8284271); // 同心环 r=4（θ=45°）
    const int n = inkCount(img, pg.x(), pg.y(), 3);
    if (grid)
        QVERIFY2(n > 0, qPrintable(QString("[%1] 网格开：%2,%3 应有墨迹")
                                   .arg(backend).arg(pg.x()).arg(pg.y())));
    else
        QVERIFY2(n == 0, qPrintable(QString("[%1] 网格关：%2,%3 应为空白")
                                    .arg(backend).arg(pg.x()).arg(pg.y())));

    // 标签开关（labelDev 只接收标签层；CPU=同图已含标签故经计数差验证）
    if (labelDev) {
        if (labels)
            QVERIFY2(inkCountAll(*labelDev) > 0,
                     qPrintable(QString("[%1] 标签开：标签层应有文字像素").arg(backend)));
        else
            QVERIFY2(inkCountAll(*labelDev) == 0,
                     qPrintable(QString("[%1] 标签关：标签层应空白").arg(backend)));
    } else {
        // CPU：与同组合标签关的图比较（在调用方做，因为需要重建场景）
        Q_UNUSED(labels)
    }
}

QString comboName(ProjectionKind kind, bool grid, bool labels)
{
    return QString("%1|grid=%2|labels=%3")
        .arg(kind == ProjectionKind::Cartesian ? "Cartesian" : "Polar")
        .arg(grid ? "on" : "off").arg(labels ? "on" : "off");
}

} // namespace

// ===== CPU：8 组合全跑（offscreen ctest 常驻）=====
void TestAxisMatrixCpu::cpuMatrix()
{
    for (ProjectionKind kind : {ProjectionKind::Cartesian, ProjectionKind::Polar}) {
        for (bool grid : {false, true}) {
            for (bool labels : {false, true}) {
                AxisFixture f(kind);
                f.build(grid, labels);
                const int expectLabels = (kind == ProjectionKind::Cartesian) ? 10 : 11;
                QCOMPARE(f.scene.labels.size(), labels ? expectLabels : 0);

                const QImage img = renderCpu(f);

                if (labels) {
                    AxisFixture off(kind);
                    off.build(grid, false);
                    const QImage imgOff = renderCpu(off);
                    QVERIFY2(inkCountAll(img) > inkCountAll(imgOff) + 20,
                             qPrintable(QString("[CPU] %1：标签开应比关多出文字像素")
                                        .arg(comboName(kind, grid, labels))));
                }

                verifyCombo(img, kind, grid, labels, nullptr, "CPU");
                qInfo().noquote() << QString("[CPU PASS] %1").arg(comboName(kind, grid, labels));
            }
        }
    }

    // ---- 边框轴维度（批次 B3）：16 组合补齐 ----
    // 边框轴 = plotArea 外 QPainter 直绘（不进 renderer/cull）：外缘出现墨迹带，
    // plotArea 内基线不变（与无边框版本相比仅容许边缘线/刻度微小差）。
    for (ProjectionKind kind : {ProjectionKind::Cartesian, ProjectionKind::Polar}) {
        for (bool grid : {false, true}) {
            for (bool labels : {false, true}) {
                AxisFixture f(kind);
                f.build(grid, labels);
                const QRectF pa(20, 20, 400, 400);
                f.scene.plotArea = pa;
                f.camera.setViewRect(QRectF(kViewLo, kViewLo, kViewHi - kViewLo, kViewHi - kViewLo));

                QImage img(440, 440, QImage::Format_ARGB32_Premultiplied);
                img.fill(Qt::white);
                QPainterChartRenderer r;
                r.render(f.scene, &img);

                // 边框轴（Bottom + Left）画在 plotArea 外侧边距
                DrawContext ctx;
                ctx.plotArea = pa;
                ctx.dataBounds = QRectF(kViewLo, kViewLo, kViewHi - kViewLo, kViewHi - kViewLo);
                ctx.viewRect = ctx.dataBounds;
                ctx.projection = f.projection.get();
                {
                    QPainter p(&img);
                    f.dim0Axis.drawAtEdge(&p, ctx, true, true, true);
                    f.dim1Axis.drawAtEdge(&p, ctx, true, true, true);
                    p.end();
                }
                const int leftBand = inkInRect(img, QRect(0, 20, 20, 400));
                const int bottomBand = inkInRect(img, QRect(20, 420, 400, 20));
                QVERIFY2(leftBand > 0, "边框轴开：plotArea 左外带应有墨迹");
                QVERIFY2(bottomBand > 0, "边框轴开：plotArea 下外带应有墨迹");

                // 基线不变：无边框渲染同场景 interior 计数近似（容差边缘线/刻度小差）
                AxisFixture base(kind);
                base.build(grid, labels);
                base.scene.plotArea = pa;
                QImage imgBase(440, 440, QImage::Format_ARGB32_Premultiplied);
                imgBase.fill(Qt::white);
                QPainterChartRenderer rb;
                rb.render(base.scene, &imgBase);
                auto interiorInk = [](const QImage& im) {
                    int n = 0;
                    for (int y = 21; y < 420; ++y)
                        for (int x = 21; x < 420; ++x)
                            if (isInk(im.pixelColor(x, y))) ++n;
                    return n;
                };
                const int diff = qAbs(interiorInk(img) - interiorInk(imgBase));
                QVERIFY2(diff < 120,
                         qPrintable(QString("边框轴不应改变 plotArea 内基线（diff=%1）")
                                    .arg(diff)));
                qInfo().noquote() << QString("[CPU PASS] %1|border=on")
                    .arg(comboName(kind, grid, labels));
            }
        }
    }
}

// ===== GL：真实环境 8 组合 =====
void TestAxisMatrixGl::initTestCase()
{
    // §验收：offscreen 平台无真实 GL → 整类 QSKIP（记录环境缺失；转 wayland/xcb 或 Windows 侧）
    if (QGuiApplication::platformName() == "offscreen")
        QSKIP("offscreen 平台无真实 GL：GL 组合跳过（转 wayland/xcb 手动实跑或用户 Windows 侧）");
}

void TestAxisMatrixGl::glMatrix()
{
    QOpenGLWidget host;
    host.setFormat(QChartGL::surfaceFormat());
    host.resize(kSize, kSize);
    host.show();
    if (!QTest::qWaitForWindowExposed(&host))
        QSKIP("无可用显示/GL 窗口未暴露：GL 组合跳过（转用户 Windows 侧）");

    host.makeCurrent();
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(host.context());
    QVERIFY2(f, "QOpenGLFunctions_3_3_Core 不可用");
    static bool s_envLogged = false;
    if (!s_envLogged) {
        const auto v = reinterpret_cast<const char*>(f->glGetString(GL_VENDOR));
        const auto r = reinterpret_cast<const char*>(f->glGetString(GL_RENDERER));
        const auto ver = reinterpret_cast<const char*>(f->glGetString(GL_VERSION));
        qInfo().noquote() << QString("GL 环境: platform=%1 vendor=%2 renderer=%3 version=%4")
            .arg(QGuiApplication::platformName())
            .arg(v ? v : "?").arg(r ? r : "?").arg(ver ? ver : "?");
        s_envLogged = true;
    }

    for (ProjectionKind kind : {ProjectionKind::Cartesian, ProjectionKind::Polar}) {
        for (bool grid : {false, true}) {
            for (bool labels : {false, true}) {
                AxisFixture fx(kind);
                fx.build(grid, labels);

                f->glBindFramebuffer(GL_FRAMEBUFFER, host.defaultFramebufferObject());
                f->glViewport(0, 0, kSize, kSize);
                f->glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
                f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                QImage labelDev(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
                labelDev.fill(Qt::white);

                QOpenGLChartRenderer renderer;
                renderer.render(fx.scene, &labelDev);
                f->glFinish();

                QVector<uchar> buf(kSize * kSize * 4);
                f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
                f->glReadPixels(0, 0, kSize, kSize, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

                // GL 行序底→顶翻转，得到与 QPainter 同构的 QImage（顶行序）
                QImage glImg(kSize, kSize, QImage::Format_RGBA8888);
                for (int y = 0; y < kSize; ++y)
                    std::memcpy(glImg.scanLine(y), buf.constData() + (kSize - 1 - y) * kSize * 4,
                                kSize * 4);

                const int expectLabels = (kind == ProjectionKind::Cartesian) ? 10 : 11;
                QCOMPARE(fx.scene.labels.size(), labels ? expectLabels : 0);
                verifyCombo(glImg, kind, grid, labels, &labelDev, "GL");
                renderer.clearBatches();   // 上下文仍 current：释放 VBO/VAO，避免析构泄漏告警
                qInfo().noquote() << QString("[GL PASS] %1").arg(comboName(kind, grid, labels));
            }
        }
    }

    host.doneCurrent();
    host.hide();
}
