// test_axes3d_gl.cpp —— 批次 B1：3D 轴渲染 GL 冒烟
// 场景与 CPU 冒烟同构（QChartLayer3D collect → Camera3D+3D 投影 → QOpenGLChartRenderer）。
#include "test_axes3d_gl.h"

#include <QtTest>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

#include "QChartLayer3D.h"
#include "QOpenGLChartRenderer.h"
#include "QChartGL.h"
#include "QValueAxis.h"
#include "QCartesianProjection3D.h"

namespace {
bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}
} // namespace

void TestAxes3DGl::initTestCase()
{
    if (QGuiApplication::platformName() == "offscreen")
        QSKIP("offscreen 平台无真实 GL：3D GL 冒烟跳过（wayland/xcb 实跑或 Windows 侧）");
}

void TestAxes3DGl::gl3dWireframeRenders()
{
    int totals[2] = {0, 0};   // [0]=Box, [1]=Lattice（模式判别收集）
    for (bool lattice : {false, true}) {
        // 组装 3D 场景（layer/轴/投影/相机生命周期覆盖整个渲染过程）
        QChartLayer3D layer;
        QValueAxis ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft), az(nullptr, Qt::AlignBottom);
        QCartesianProjection3D proj;
        ax.setTickCount(5); ay.setTickCount(5); az.setTickCount(5);
        ax.setColor(Qt::black); ay.setColor(Qt::black); az.setColor(Qt::black);
        // 网格色加深（默认 220 浅灰被 isInk>40 阈值排除 → 网格/Box-Lattice 差异无法断言）
        layer.setGridColor(QColor(120, 120, 120));
        layer.setAxisX(&ax);
        layer.setAxisY(&ay);
        layer.setAxisZ(&az);
        layer.setProjection3D(&proj);
        layer.setDataBounds(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));
        layer.setGridMode(lattice ? QChartLayer3D::GridMode::Lattice : QChartLayer3D::GridMode::Box);
        QChartCamera3D* cam = layer.camera3D();
        cam->setViewCube(QCube(QVector3D(-4, -4, -4), QVector3D(4, 4, 4)));
        cam->setYaw(45.0);
        cam->setPitch(30.0);
        layer.setScene3DProjection(&proj);
        layer.setScene3DPlotArea(QRectF(0, 0, 400, 400));
        layer.collectPrimitives();
        QChartScene scene = layer.scene3D();   // camera 指针指向 layer 成员（layer 存活期渲染）
        for (QChartPrimitive& p : scene.primitives)
            p.depth = 2.0f;   // B1 线框冒烟：depth>1 → GL decor 批次（depthTest 关=全边可见）

        QOpenGLWidget host;
        host.setFormat(QChartGL::surfaceFormat());
        host.resize(400, 400);
        host.show();
        if (!QTest::qWaitForWindowExposed(&host))
            QSKIP("无可用显示/窗口未暴露：3D GL 冒烟跳过（转用户 Windows 侧）");

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
        renderer.render(scene, &labelDev);   // 图元→FBO（u_viewProj 经 Camera3D viewProjectionMatrix）
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

        int total = 0, tiles = 0;
        for (int y = 0; y < 400; ++y)
            for (int x = 0; x < 400; ++x)
                if (isInk(img.pixelColor(x, y))) ++total;
        for (int ty = 0; ty < 3; ++ty)
            for (int tx = 0; tx < 3; ++tx) {
                bool hit = false;
                for (int y = ty * 133; y < (ty + 1) * 133 && !hit; ++y)
                    for (int x = tx * 133; x < (tx + 1) * 133 && !hit; ++x)
                        if (isInk(img.pixelColor(x, y))) hit = true;
                if (hit) ++tiles;
            }
        QVERIFY2(scene.primitives.size() > 40, "3D 场景应有盒边/脊/网格图元");
        QVERIFY2(total > 150,
                 qPrintable(QString("3D GL 盒/网格应出墨（mode=%1 total=%2）")
                            .arg(lattice ? "Lattice" : "Box").arg(total)));
        QVERIFY2(tiles >= 3,
                 qPrintable(QString("3D GL 内容应铺开（mode=%1 tiles=%2/9）")
                            .arg(lattice ? "Lattice" : "Box").arg(tiles)));
        qInfo().noquote() << QString("3D GL ink: mode=%1 total=%2 tiles=%3/9")
                             .arg(lattice ? "Lattice" : "Box").arg(total).arg(tiles);
        totals[lattice ? 1 : 0] = total;
    }
    // 模式判别：Lattice（3 族全网格）墨迹应显著多于 Box（仅底面 2 族）
    QVERIFY2(totals[1] > totals[0] + 100,
             qPrintable(QString("Lattice 墨迹应显著多于 Box（Box=%1 Lattice=%2）")
                        .arg(totals[0]).arg(totals[1])));
}
