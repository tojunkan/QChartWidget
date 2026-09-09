// test_widget3d_gl.cpp —— 批次 B2：QChartWidget3D GL 实跑冒烟
#include "test_widget3d_gl.h"

#include <QtTest>
#include <QGuiApplication>
#include <QOpenGLWidget>

#include "QChartWidget3D.h"
#include "QCartesianProjection3D.h"

namespace {
bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}
int inkIn(const QImage& img, const QRect& r)
{
    int n = 0;
    const int x0 = qMax(0, r.left()), y0 = qMax(0, r.top());
    const int x1 = qMin(img.width() - 1, r.right()), y1 = qMin(img.height() - 1, r.bottom());
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}
} // namespace

void TestWidget3DGl::initTestCase()
{
    if (QGuiApplication::platformName() == "offscreen")
        QSKIP("offscreen 平台无真实 GL：widget3D GL 冒烟跳过（wayland/xcb 实跑或 Windows 侧）");
}

void TestWidget3DGl::glWidget3DRenders()
{
    QChartWidget3D w;
    QCartesianProjection3D proj;
    w.setProjection3D(&proj);
    w.setDomainBox(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));
    w.resize(420, 360);

    w.setRenderBackend(QChartAbstractWidget::RenderBackend::OpenGL);
    w.show();
    if (!QTest::qWaitForWindowExposed(&w))
        QSKIP("无可用显示/窗口未暴露：widget3D GL 冒烟跳过（转用户 Windows 侧）");
    for (int i = 0; i < 20; ++i) {
        QTest::qWait(20);
        if (w.plotArea().width() > 0) break;
    }
    QTest::qWait(250);

    auto* host = qobject_cast<QOpenGLWidget*>(w.glHostWidget());
    QVERIFY2(host && host->isVisible(), "3D 容器应创建 plotArea 对齐 GL 宿主");
    QCOMPARE(host->geometry(), w.plotArea().toRect());

    const QImage fbo = host->grabFramebuffer();
    QVERIFY2(!fbo.isNull(), "host grabFramebuffer 应成功");
    QVERIFY2(inkIn(fbo, QRect(0, 0, fbo.width(), fbo.height())) > 150,
             "3D GL 盒/网格/刻度应出墨（u_viewProj Camera3D 通路）");
    // 中心区域应有内容（相机 fit 后盒在画布中部附近；多像素邻域）
    QVERIFY2(inkIn(fbo, QRect(fbo.width() / 2 - 60, fbo.height() / 2 - 60, 120, 120)) > 10,
             "3D 盒应落在画布中部区域");
}
