// test_widget3d_smoke.cpp —— 批次 B2：QChartWidget3D 轻量容器 CPU 冒烟（offscreen）
#include "test_widget3d_smoke.h"

#include <QtTest>
#include <QImage>
#include <QColor>
#include <QRegularExpression>

#include "QChartWidget3D.h"
#include "QChartCamera3D.h"
#include "QCartesianProjection3D.h"
#include "QSphericalProjection3D.h"

namespace {
bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}
int inkTotal(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}
int inkTiles(const QImage& img)
{
    const int tw = 3, th = 3;
    int tiles = 0;
    for (int ty = 0; ty < th; ++ty)
        for (int tx = 0; tx < tw; ++tx) {
            bool hit = false;
            for (int y = ty * (img.height() / th); y < (ty + 1) * (img.height() / th) && !hit; ++y)
                for (int x = tx * (img.width() / tw); x < (tx + 1) * (img.width() / tw) && !hit; ++x)
                    if (isInk(img.pixelColor(x, y))) hit = true;
            if (hit) ++tiles;
        }
    return tiles;
}
} // namespace

void TestWidget3DSmoke::cpuWidget3DRenders()
{
    QChartWidget3D w;
    QCartesianProjection3D proj;          // 生命周期覆盖渲染（非持有约定）
    w.setProjection3D(&proj);
    w.setDomainBox(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));   // → layer3D.setDataBounds + fitWorld
    // 批次2 B：FaceLine 为默认模式——容器冒烟取证（盒/网格/刻度墨迹）显式使用几何最丰富的 Box
    w.setGridMode3D(QChartLayer3D::GridMode::Box);
    QCOMPARE(static_cast<int>(w.gridMode3D()),
             static_cast<int>(QChartLayer3D::GridMode::Box));
    QVERIFY2(w.hasDomainBox(), "setDomainBox 应记录域盒");
    QVERIFY2(w.layer3D() != nullptr, "构造应托管默认 layer3D");
    QVERIFY2(w.camera3D() == w.layer3D()->camera3D(), "相机归 layer3D（widget 无独立相机成员）");
    QVERIFY2(w.projection3D() == &proj, "投影应转发 layer3D");
    QVERIFY2(w.viewCube().isValid(), "fitWorld 后相机应有有效 viewCube");

    // worldToPixel 便捷（经层相机；plotArea 就绪前返回 NaN 亦可接受，此处仅调用不崩）
    (void)w.worldToPixel(QVector3D(0, 0, 0));

    w.resize(420, 360);
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "offscreen 下窗口应暴露");
    QTest::qWait(40);

    const QImage img = w.grab().toImage();
    // ★ HiDPI：grab 为设备像素图（逻辑 × dpr），断言按 dpr 缩放（DPR=1 时 s=1 与历史一致）
    const qreal s = qreal(img.width()) / w.width();
    QVERIFY2(qAbs(s - w.devicePixelRatioF()) < 0.01,
             "grab 尺寸应等于 widget（设备像素 = 逻辑 × dpr）");
    const int total = inkTotal(img);
    const int tiles = inkTiles(img);
    QVERIFY2(total > 150, qPrintable(QString("3D 盒/网格/刻度应出墨 total=%1").arg(total)));
    QVERIFY2(tiles >= 4, qPrintable(QString("3D 内容应铺开多区 tiles=%1/9").arg(tiles)));
}

// ===== 批次2 B：QChartWidget3D 三网格模式入口（转发图层）+ 默认值 + 标签契约 =====
void TestWidget3DSmoke::cpuWidget3DGridModes()
{
    QChartWidget3D w;
    QCartesianProjection3D proj;
    w.setProjection3D(&proj);
    w.setDomainBox(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));

    // 默认模式 = FaceLine（批次2 B）
    QCOMPARE(static_cast<int>(w.gridMode3D()),
             static_cast<int>(QChartLayer3D::GridMode::FaceLine));

    QChartLayer3D* layer = w.layer3D();
    QVERIFY(layer != nullptr);

    const auto collect = [layer](QChartLayer3D::GridMode m) {
        layer->setGridMode(m);
        layer->collectPrimitives();
        return qMakePair(layer->scene3D().primitives.size(), layer->scene3D().labels.size());
    };

    // FaceLine：只画一条安全轴线 + 刻度标签；无盒边/网格
    w.setGridMode3D(QChartLayer3D::GridMode::FaceLine);
    QCOMPARE(static_cast<int>(w.gridMode3D()),
             static_cast<int>(QChartLayer3D::GridMode::FaceLine));
    const auto face = collect(QChartLayer3D::GridMode::FaceLine);
    QVERIFY2(face.first > 0, "面线模式应有轴线图元");
    QVERIFY2(face.second > 0, "面线模式：轴按刻度逐个标注");

    // Box：盒 12 边 + 底面网格 + 三主轴标签（图元显著多于面线）
    w.setGridMode3D(QChartLayer3D::GridMode::Box);
    const auto box = collect(QChartLayer3D::GridMode::Box);
    QVERIFY2(box.first > face.first, "盒模式图元应多于面线模式");
    QVERIFY2(box.second > 0, "盒模式：三条主轴应按刻度生成标签");

    // Lattice：只画线，无任何标签
    w.setGridMode3D(QChartLayer3D::GridMode::Lattice);
    const auto lat = collect(QChartLayer3D::GridMode::Lattice);
    QCOMPARE(lat.second, 0);
    QVERIFY2(lat.first > 0, "晶格模式应有线图元");

    // Box + 非直角投影：图层 qWarning 并回退 FaceLine（模式下不变）
    QSphericalProjection3D sph;
    w.setProjection3D(&sph);
    w.setGridMode3D(QChartLayer3D::GridMode::Box);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("GridMode::Box"));
    layer->collectPrimitives();
    QCOMPARE(layer->scene3D().primitives.size(), face.first);
    QCOMPARE(static_cast<int>(w.gridMode3D()),
             static_cast<int>(QChartLayer3D::GridMode::Box));
}
