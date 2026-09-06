// test_widget_gl.cpp —— 批次 A widget GL 宿主冒烟（实窗验证）
// 覆盖：QChartWidget 切 OpenGL 后端 → plotArea 对齐 QOpenGLWidget 子控件（GL 只画 plotArea 内）→
//       host FBO readback 取证网格/轴出墨 + 宿主几何==plotArea（外部边框轴由外层画，区域互不重叠）。
#include "test_widget_gl.h"

#include <QtTest>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

#include "QChartWidget.h"
#include "QChartLayer.h"
#include "QValueAxis.h"

namespace {
bool isInk(const QColor& c)
{
    // 网格默认色 (220,220,220)：离白 35 → 阈值 25 使浅灰网格也算墨
    return qAbs(c.red() - 255) > 25 || qAbs(c.green() - 255) > 25 || qAbs(c.blue() - 255) > 25;
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

void TestWidgetGl::initTestCase()
{
    if (QGuiApplication::platformName() == "offscreen")
        QSKIP("offscreen 平台无真实 GL：widget GL 冒烟跳过（wayland/xcb 实跑或 Windows 侧）");
}

void TestWidgetGl::glWidgetRenders()
{
    QChartWidget w;
    QValueAxis ax(nullptr, Qt::AlignBottom);
    QValueAxis ay(nullptr, Qt::AlignLeft);
    ax.setRange(-10, 10); ax.setTickCount(5); ax.setColor(Qt::black);
    ay.setRange(-10, 10); ay.setTickCount(5); ay.setColor(Qt::black);

    QChartLayer layer;
    w.addAxis(&ax);
    w.addAxis(&ay);
    w.addLayer(&layer);
    w.resize(420, 340);

    w.setRenderBackend(QChartAbstractWidget::RenderBackend::OpenGL);
    w.show();
    if (!QTest::qWaitForWindowExposed(&w))
        QSKIP("无可用显示/窗口未暴露：GL widget 冒烟跳过（转用户 Windows 侧）");

    // 等首帧 + 布局（plotArea 计算并摆放宿主）
    for (int i = 0; i < 20; ++i) {
        QTest::qWait(20);
        if (w.plotArea().width() > 0) break;
    }
    QVERIFY2(w.plotArea().width() > 100 && w.plotArea().height() > 100,
             "布局后 plotArea 应有效");

    auto* host = qobject_cast<QOpenGLWidget*>(w.glHostWidget());
    QVERIFY2(host, "GL 后端应创建 plotArea 对齐的 QOpenGLWidget 宿主");
    QVERIFY2(host->isVisible(), "GL 宿主应可见");

    // 宿主几何 == plotArea（GL 只画 plotArea 内 → 与外层边框轴区域互不重叠/不遮挡）
    QCOMPARE(host->geometry(), w.plotArea().toRect());
    QVERIFY2(host->geometry().right() < w.width() && host->geometry().bottom() < w.height(),
             "宿主应位于 plotArea（外层留出边距画边框轴/标题）");

    // 环境记录（vendor/renderer）
    host->makeCurrent();
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(host->context());
    QVERIFY2(f, "GL 3.3 Core 函数不可用");
    const auto* v = reinterpret_cast<const char*>(f->glGetString(GL_VENDOR));
    const auto* r = reinterpret_cast<const char*>(f->glGetString(GL_RENDERER));
    const auto* ver = reinterpret_cast<const char*>(f->glGetString(GL_VERSION));
    qInfo().noquote() << QString("widget GL 环境: platform=%1 vendor=%2 renderer=%3 version=%4")
        .arg(QGuiApplication::platformName())
        .arg(v ? v : "?").arg(r ? r : "?").arg(ver ? ver : "?");
    host->doneCurrent();

    // 等若干帧让 paintGL 真正执行
    QTest::qWait(200);

    // host FBO 取证：网格/轴脊落屏（FBO 行序底→顶，翻转后与顶层行序一致）
    const QImage fbo = host->grabFramebuffer();
    QVERIFY2(!fbo.isNull(), "host grabFramebuffer 应成功");
    const QPoint center(fbo.width() / 2, fbo.height() / 2);
    QVERIFY2(inkIn(fbo, QRect(center.x() - 3, center.y() - 3, 7, 7)) > 0,
             "host FBO 中心应有网格墨迹（轴域 0,0 交叉）");
    QVERIFY2(inkIn(fbo, QRect(0, 0, fbo.width(), fbo.height())) > 400,
             "host FBO 应产生大量墨迹（网格+刻度）");

    // F1(t8) 标签 child-local 探针：垂直网格脊 x=-8 上 y=0 刻度标签（文本向右排）。
    // 实测字形墨迹 rows≈135..143（水平网格线行 row≈139 除外）：
    //   正确带 rows≈140..144 cols≈36..66（translate(-plotArea.topLeft) 生效时字形在此）
    //   偏移带 rows≈160..164 cols≈104..134（未平移时字形整体 +plotArea.topLeft 落此）
    const int nLocalBand = inkIn(fbo, QRect(36, 140, 30, 5));
    const int nShiftBand = inkIn(fbo, QRect(104, 160, 30, 5));
    qInfo().noquote() << QString("widget GL 标签带采样: 正确带(local)=%1 偏移带(父系)=%2")
                         .arg(nLocalBand).arg(nShiftBand);
    QVERIFY2(nLocalBand > 0,
             "标签局部坐标带应有字形墨迹（translate(-plotArea.topLeft) 生效）");
    QVERIFY2(nShiftBand == 0,
             "父系偏移带应为空白（标签未按局部坐标绘制时字形会落此）");
}
