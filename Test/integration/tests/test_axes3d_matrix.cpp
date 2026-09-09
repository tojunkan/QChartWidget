// test_axes3d_matrix.cpp —— 批次 B3：3D 轴矩阵（CPU/GL）+ spherical 冒烟
#include "test_axes3d_matrix.h"

#include <QtTest>
#include <QGuiApplication>
#include <QImage>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

#include "QChartLayer3D.h"
#include "QPainterChartRenderer.h"
#include "QOpenGLChartRenderer.h"
#include "QChartGL.h"
#include "QValueAxis.h"
#include "QChartCamera3D.h"
#include "QCartesianProjection3D.h"
#include "QSphericalProjection3D.h"

namespace {
bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}
int inkAll(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}

/// 墨迹掩膜差异像素计数（两图 isInk 状态不同的像素数）
int maskDiff(const QImage& a, const QImage& b)
{
    int n = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (isInk(a.pixelColor(x, y)) != isInk(b.pixelColor(x, y))) ++n;
    return n;
}

struct Combo { bool lattice; qreal yaw, pitch; };

/// 组装 Cartesian3D 层场景（Box/Lattice × 姿态）；grid 用深色便于区域计数判别
void buildScene3D(QChartLayer3D& layer, QChartScene& scene,
                  const QChartProjection3D* proj, bool lattice, qreal yaw, qreal pitch,
                  const QVector3D& mn, const QVector3D& mx)
{
    Q_UNUSED(proj)
    QValueAxis ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft), az(nullptr, Qt::AlignBottom);
    ax.setTickCount(5); ay.setTickCount(5); az.setTickCount(5);
    ax.setColor(Qt::black); ay.setColor(Qt::black); az.setColor(Qt::black);
    layer.setAxisX(&ax);
    layer.setAxisY(&ay);
    layer.setAxisZ(&az);
    layer.setProjection3D(proj);
    layer.setDataBounds(mn, mx);
    layer.setGridMode(lattice ? QChartLayer3D::GridMode::Lattice : QChartLayer3D::GridMode::Box);
    layer.setGridColor(QColor(0, 0, 0));   // 深色网格（判别计数）
    QChartCamera3D* cam = layer.camera3D();
    const QVector3D pad = (mx - mn) * 0.25f;
    cam->setViewCube(QCube(mn - pad, mx + pad));
    cam->setYaw(yaw);
    cam->setPitch(pitch);
    layer.setScene3DProjection(proj);
    layer.setScene3DPlotArea(QRectF(0, 0, 400, 400));
    layer.collectPrimitives();
    scene = layer.scene3D();
    for (QChartPrimitive& p : scene.primitives)
        p.depth = 2.0f;   // decor 批次（depthTest 关 = 全边可见，与 CPU 全绘制一致）
}

QImage renderCpu(QChartScene& scene)
{
    QImage img(400, 400, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainterChartRenderer r;
    r.render(scene, &img);
    return img;
}

QString comboTag(const Combo& c)
{
    return QString("%1|yaw=%2|pitch=%3").arg(c.lattice ? "Lattice" : "Box")
        .arg(c.yaw, 0, 'g', 3).arg(c.pitch, 0, 'g', 3);
}
} // namespace

// ===== CPU：4 组合（Box/Lattice × 姿态 A/B）=====
void TestAxes3dMatrixCpu::cpuMatrix8()
{
    const Combo combos[4] = {
        {false, 0, 0}, {false, 45, 30}, {true, 0, 0}, {true, 45, 30}
    };
    QCartesianProjection3D proj;
    int latticeSum = 0, boxSum = 0;
    QImage imgs[4];
    for (int i = 0; i < 4; ++i) {
        QChartLayer3D layer;   // 生命周期覆盖渲染（scene 含指向其相机的指针）
        QChartScene scene;
        buildScene3D(layer, scene, &proj, combos[i].lattice, combos[i].yaw, combos[i].pitch,
                     QVector3D(-3, -3, -3), QVector3D(3, 3, 3));
        imgs[i] = renderCpu(scene);
        const int n = inkAll(imgs[i]);
        qInfo().noquote() << QString("[3D-CPU] %1 ink=%2").arg(comboTag(combos[i])).arg(n);
        QVERIFY2(n > 150, qPrintable(QString("%1 应出墨 ink=%2").arg(comboTag(combos[i])).arg(n)));
        if (combos[i].lattice) latticeSum += n; else boxSum += n;
    }
    QVERIFY2(latticeSum > boxSum + 200, "Lattice 模式墨迹应显著多于 Box（判别计数非自证）");
    // 姿态判别（B3/t21）：同模式下姿态 A(0,0) 与 B(45,30) 必须产生不同投影几何——
    // 墨迹掩膜差异像素计数（旋转后盒/网格位置变化，差异应为万级；阈值 300 保守）
    const int diffBox = maskDiff(imgs[0], imgs[1]);
    const int diffLattice = maskDiff(imgs[2], imgs[3]);
    qInfo().noquote() << QString("[3D-CPU] pose-diff: Box(A/B)=%1 Lattice(A/B)=%2")
                         .arg(diffBox).arg(diffLattice);
    QVERIFY2(diffBox > 300, qPrintable(QString("Box 姿态 A/B 投影几何应不同（diff=%1）").arg(diffBox)));
    QVERIFY2(diffLattice > 300,
             qPrintable(QString("Lattice 姿态 A/B 投影几何应不同（diff=%1）").arg(diffLattice)));
}

void TestAxes3dMatrixCpu::cpuSpherical()
{
    QSphericalProjection3D proj;
    QChartLayer3D layer;
    QChartScene scene;
    // 轴数值盒 = 球坐标默认数据域形状（r/θ/φ 量纲）
    const QVector3D numMin(1, 0, -30), numMax(3, 90, 30);
    buildScene3D(layer, scene, &proj, /*lattice=*/true, 45.0, 30.0, numMin, numMax);
    // 非恒等投影：viewCube 必须取世界（Cartesian）包围盒而非数值盒
    {
        QChartCamera3D* cam = layer.camera3D();
        QCube wc = proj.computeViewCube(numMin, numMax);
        const QVector3D pad = (wc.max - wc.min) * 0.35f;
        wc = QCube(wc.min - pad, wc.max + pad);
        cam->setViewCube(wc);
        const qreal hd = (wc.max - wc.min).length() * 0.5;
        cam->setDistance(hd / qMax(qreal(0.05), qSin(qDegreesToRadians(cam->fov()) * 0.5)));
        cam->resetNearFar();
    }
    scene = layer.scene3D();
    const QImage img = renderCpu(scene);
    const int n = inkAll(img);
    qInfo().noquote() << QString("[3D-CPU] spherical ink=%1").arg(n);
    QVERIFY2(n > 80, qPrintable(QString("spherical CPU 应出图 ink=%1").arg(n)));
    // 非恒等：图元在数值域(1..3 r)下映射到世界非立方盒（区域铺开 ≥2 tiles）
    int tiles = 0;
    for (int ty = 0; ty < 3; ++ty)
        for (int tx = 0; tx < 3; ++tx) {
            bool hit = false;
            for (int y = ty * 133; y < (ty + 1) * 133 && !hit; ++y)
                for (int x = tx * 133; x < (tx + 1) * 133 && !hit; ++x)
                    if (isInk(img.pixelColor(x, y))) hit = true;
            if (hit) ++tiles;
        }
    QVERIFY2(tiles >= 2, qPrintable(QString("spherical 内容应铺开 tiles=%1/9").arg(tiles)));
}

// ===== GL：wayland 实跑 =====
void TestAxes3dMatrixGl::initTestCase()
{
    if (QGuiApplication::platformName() == "offscreen")
        QSKIP("offscreen 平台无真实 GL：3D 矩阵 GL 跳过（wayland/xcb 实跑或 Windows 侧）");
}

void TestAxes3dMatrixGl::glMatrix8()
{
    const Combo combos[4] = {
        {false, 0, 0}, {false, 45, 30}, {true, 0, 0}, {true, 45, 30}
    };
    QCartesianProjection3D proj;
    int latticeSum = 0, boxSum = 0;
    QImage imgs[4];
    for (int i = 0; i < 4; ++i) {
        QChartLayer3D layer;
        QChartScene scene;
        buildScene3D(layer, scene, &proj, combos[i].lattice, combos[i].yaw, combos[i].pitch,
                     QVector3D(-3, -3, -3), QVector3D(3, 3, 3));

        QOpenGLWidget host;
        host.setFormat(QChartGL::surfaceFormat());
        host.resize(400, 400);
        host.show();
        if (!QTest::qWaitForWindowExposed(&host))
            QSKIP("无可用显示/窗口未暴露：3D 矩阵 GL 跳过（转用户 Windows 侧）");
        host.makeCurrent();
        auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(host.context());
        QVERIFY2(f, "GL 3.3 Core 函数不可用");
        f->glBindFramebuffer(GL_FRAMEBUFFER, host.defaultFramebufferObject());
        f->glViewport(0, 0, 400, 400);
        f->glClearColor(1, 1, 1, 1);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        QImage labelDev(400, 400, QImage::Format_ARGB32_Premultiplied);
        labelDev.fill(Qt::transparent);
        QOpenGLChartRenderer renderer;
        renderer.invalidateView();
        renderer.render(scene, &labelDev);
        f->glFinish();
        QVector<uchar> buf(400 * 400 * 4);
        f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        f->glReadPixels(0, 0, 400, 400, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
        QImage img(400, 400, QImage::Format_RGBA8888);
        for (int y = 0; y < 400; ++y)
            std::memcpy(img.scanLine(y), buf.constData() + (399 - y) * 400 * 4, 400 * 4);
        renderer.clearBatches();
        host.doneCurrent();
        host.hide();
        imgs[i] = img;

        const int n = inkAll(imgs[i]);
        qInfo().noquote() << QString("[3D-GL] %1 ink=%2").arg(comboTag(combos[i])).arg(n);
        QVERIFY2(n > 120, qPrintable(QString("GL %1 应出墨 ink=%2").arg(comboTag(combos[i])).arg(n)));
        if (combos[i].lattice) latticeSum += n; else boxSum += n;
    }
    QVERIFY2(latticeSum > boxSum + 150, "GL Lattice 墨迹应显著多于 Box（判别计数非自证）");
    // 姿态判别（B3/t21）：GL 同模式下姿态 A/B 投影几何必须不同（墨迹掩膜差异）
    const int diffBox = maskDiff(imgs[0], imgs[1]);
    const int diffLattice = maskDiff(imgs[2], imgs[3]);
    qInfo().noquote() << QString("[3D-GL] pose-diff: Box(A/B)=%1 Lattice(A/B)=%2")
                         .arg(diffBox).arg(diffLattice);
    QVERIFY2(diffBox > 300, qPrintable(QString("GL Box 姿态 A/B 投影几何应不同（diff=%1）").arg(diffBox)));
    QVERIFY2(diffLattice > 300,
             qPrintable(QString("GL Lattice 姿态 A/B 投影几何应不同（diff=%1）").arg(diffLattice)));
}

void TestAxes3dMatrixGl::glSpherical()
{
    QSphericalProjection3D proj;
    QChartLayer3D layer;
    QChartScene scene;
    const QVector3D numMin(1, 0, -30), numMax(3, 90, 30);
    buildScene3D(layer, scene, &proj, /*lattice=*/true, 45.0, 30.0, numMin, numMax);
    {
        QChartCamera3D* cam = layer.camera3D();
        QCube wc = proj.computeViewCube(numMin, numMax);
        const QVector3D pad = (wc.max - wc.min) * 0.35f;
        wc = QCube(wc.min - pad, wc.max + pad);
        cam->setViewCube(wc);
        const qreal hd = (wc.max - wc.min).length() * 0.5;
        cam->setDistance(hd / qMax(qreal(0.05), qSin(qDegreesToRadians(cam->fov()) * 0.5)));
        cam->resetNearFar();
    }
    scene = layer.scene3D();

    QOpenGLWidget host;
    host.setFormat(QChartGL::surfaceFormat());
    host.resize(400, 400);
    host.show();
    if (!QTest::qWaitForWindowExposed(&host))
        QSKIP("无可用显示/窗口未暴露：spherical GL 跳过（转用户 Windows 侧）");
    host.makeCurrent();
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(host.context());
    QVERIFY2(f, "GL 3.3 Core 函数不可用");
    f->glBindFramebuffer(GL_FRAMEBUFFER, host.defaultFramebufferObject());
    f->glViewport(0, 0, 400, 400);
    f->glClearColor(1, 1, 1, 1);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    QImage labelDev(400, 400, QImage::Format_ARGB32_Premultiplied);
    labelDev.fill(Qt::transparent);
    QOpenGLChartRenderer renderer;
    renderer.invalidateView();
    renderer.render(scene, &labelDev);   // GLSL 注入 spherical 表达式
    f->glFinish();
    QVector<uchar> buf(400 * 400 * 4);
    f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    f->glReadPixels(0, 0, 400, 400, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
    QImage img(400, 400, QImage::Format_RGBA8888);
    for (int y = 0; y < 400; ++y)
        std::memcpy(img.scanLine(y), buf.constData() + (399 - y) * 400 * 4, 400 * 4);
    renderer.clearBatches();
    host.doneCurrent();
    host.hide();

    const int n = inkAll(img);
    qInfo().noquote() << QString("[3D-GL] spherical ink=%1").arg(n);
    QVERIFY2(n > 60, qPrintable(QString("spherical GL 应出图 ink=%1").arg(n)));
}
