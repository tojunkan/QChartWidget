// test_axis_pipeline.cpp —— S0 轴渲染管线单元测试（见头文件注释）
#include "test_axis_pipeline.h"

#include <QtTest>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <memory>

#include "QPainterChartRenderer.h"
#include "QValueAxis.h"
#include "QChartScene.h"
#include "QChartCamera.h"
#include "QCartesianProjection.h"
#include "QPolarProjection.h"

namespace {

enum class ProjectionKind { Cartesian, Polar };

constexpr qreal kViewLo = -10.0;
constexpr qreal kViewHi =  10.0;   // 视图窗口 [-10,10]²
constexpr int    kSize  = 400;     // 方形画布/plotArea

/// 轴管线场景夹具：QValueAxis×2 + 投影 + 2D 相机 + 场景
struct AxisFixture {
    QChartCamera camera;
    QValueAxis dim0Axis;        // Cartesian: X（dim0）；Polar: 角度（环）
    QValueAxis dim1Axis;        // Cartesian: Y（dim1）；Polar: 半径（径向）
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

        if (kind == ProjectionKind::Cartesian)
            projection = std::make_unique<QCartesianProjection>();
        else
            projection = std::make_unique<QPolarProjection>();

        scene.camera = &camera;
        scene.projection = projection.get();
        scene.plotArea = QRectF(0, 0, kSize, kSize);
        scene.backgroundColor = Qt::white;
    }

    /// 组装场景：轴脊（+网格线）+ 刻度标签
    /// 语义（与 QChartLayer::drawGrid 的"tick 作 offset 反复 drawAtPosition"一致）：
    ///   grid=false → 只画两主轴脊（X@y=0 与 Y@x=0 / 外环 r=10 与 θ=0 径向脊）
    ///   grid=true  → 在另一轴每个 tick 的 offset 处再画主脊（笛卡尔：纵横网格线；
    ///                极坐标：θ tick 的径向辐条 + r tick 的同心环）；网格脊不带标签
    void build(bool grid, bool labels)
    {
        scene.primitives.clear();
        scene.labels.clear();

        if (projection->type() == QChartAbstractProjection::CoordinateSystem::Cartesian) {
            // 轴脊：X 沿 dim0 扫 y=0；Y 沿 dim1 扫 x=0
            dim0Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 0, scene, 72, labels);
            dim1Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 1, scene, 72, labels);

            if (grid) {
                const QVector<qreal> ticksY = dim1Axis.tickValues(kViewLo, kViewHi);
                for (qreal yt : ticksY) {
                    if (qAbs(yt) < 1e-9) continue;      // y=0 即 X 轴脊，跳过
                    dim0Axis.drawAtPosition(kViewLo, kViewHi, yt, 0.0, 0, scene, 8, false);
                }
                const QVector<qreal> ticksX = dim0Axis.tickValues(kViewLo, kViewHi);
                for (qreal xt : ticksX) {
                    if (qAbs(xt) < 1e-9) continue;      // x=0 即 Y 轴脊，跳过
                    dim1Axis.drawAtPosition(kViewLo, kViewHi, xt, 0.0, 1, scene, 8, false);
                }
            }
        } else {
            // 轴脊：外环 r=10（dim0=θ 0..360）；径向脊 θ=0（dim1=r 0..10）
            dim0Axis.drawAtPosition(0.0, 360.0, 10.0, 0.0, 0, scene, 90, labels);
            dim1Axis.drawAtPosition(0.0, 10.0, 0.0, 0.0, 1, scene, 8, labels);

            if (grid) {
                const QVector<qreal> ticksR = dim1Axis.tickValues(0.0, 10.0);
                for (qreal r : ticksR) {
                    if (r < 1e-9 || qAbs(r - 10.0) < 1e-9) continue;  // r=10 即外环，跳过
                    dim0Axis.drawAtPosition(0.0, 360.0, r, 0.0, 0, scene, 90, false);
                }
                const QVector<qreal> ticksTheta = dim0Axis.tickValues(0.0, 360.0);
                for (qreal th : ticksTheta) {
                    if (qAbs(th) < 1e-9) continue;      // θ=0 即径向脊，跳过
                    dim1Axis.drawAtPosition(0.0, 10.0, th, 0.0, 1, scene, 8, false);
                }
            }
        }
    }
};

/// 图元 Numeric → 像素（CPU 约定：row 顶→底；仅用于选取采样点）
QPoint pixOf(qreal x, qreal y)
{
    const qreal scale = kSize / (kViewHi - kViewLo);   // 20px/单位
    return QPoint(qRound((x - kViewLo) * scale), qRound((kViewHi - y) * scale));
}

QImage renderCpu(AxisFixture& f)
{
    QImage img(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainterChartRenderer renderer;
    renderer.render(f.scene, &img);
    return img;
}

bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}

int inkCount(const QImage& img, int cx, int cy, int radius)
{
    int n = 0;
    for (int y = qMax(0, cy - radius); y <= qMin(img.height() - 1, cy + radius); ++y) {
        for (int x = qMax(0, cx - radius); x <= qMin(img.width() - 1, cx + radius); ++x) {
            if (isInk(img.pixelColor(x, y))) ++n;
        }
    }
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

} // namespace

// ===== Cartesian：轴脊与刻度点真实落屏 =====
void TestAxisPipeline::cartesianSpineTicksRender()
{
    AxisFixture f(ProjectionKind::Cartesian);
    f.build(/*grid=*/false, /*labels=*/false);

    QVERIFY2(f.scene.primitives.size() > 10, "drawAtPosition 应提交轴脊与刻度图元");
    QCOMPARE(f.scene.labels.size(), 0);

    const QImage img = renderCpu(f);
    QVERIFY2(inkCountAll(img) > 300, "轴脊（X+Y 贯穿线）应产生大量非白像素（非空图冒烟）");

    // X 轴脊：cart(-2.5, 0) → 像素 (150, 200)
    const QPoint px = pixOf(-2.5, 0.0);
    QVERIFY2(inkCount(img, px.x(), px.y(), 3) > 0,
             qPrintable(QString("X 轴脊未落屏 @%1,%2").arg(px.x()).arg(px.y())));
    // Y 轴脊：cart(0, 5) → 像素 (200, 100)
    const QPoint py = pixOf(0.0, 5.0);
    QVERIFY2(inkCount(img, py.x(), py.y(), 3) > 0,
             qPrintable(QString("Y 轴脊未落屏 @%1,%2").arg(py.x()).arg(py.y())));
    // 主刻度中心点：tick x=4（QValueAxis -10..10 → -8,-4,0,4,8）→ 像素 (280,200)
    const QPoint pt = pixOf(4.0, 0.0);
    QVERIFY2(inkCount(img, pt.x(), pt.y(), 3) > 0,
             qPrintable(QString("X 轴主刻度点未落屏 @%1,%2").arg(pt.x()).arg(pt.y())));
    // 刻度点必须“超出”轴脊单线才算真验到 7 点刻度图元（轴脊墨迹会覆盖刻度中心，
    // 仅查中心点无法区分）→ 抽查垂直臂 ±6px（0.3 数据单位 = TICK_LENGTH×轴长 20）
    const QPoint ptArmA(pt.x(), pt.y() - 6);
    const QPoint ptArmB(pt.x(), pt.y() + 6);
    QVERIFY2(inkCount(img, ptArmA.x(), ptArmA.y(), 2) > 0,
             qPrintable(QString("X 轴刻度垂直臂未落屏（疑似刻度点图元缺失）@%1,%2")
                        .arg(ptArmA.x()).arg(ptArmA.y())));
    QVERIFY2(inkCount(img, ptArmB.x(), ptArmB.y(), 2) > 0,
             qPrintable(QString("X 轴刻度垂直臂未落屏（疑似刻度点图元缺失）@%1,%2")
                        .arg(ptArmB.x()).arg(ptArmB.y())));
}

// ===== Cartesian：网格 × 标签 组合 =====
void TestAxisPipeline::cartesianGridLabelCombos()
{
    for (bool grid : {false, true}) {
        for (bool labels : {false, true}) {
            AxisFixture f(ProjectionKind::Cartesian);
            f.build(grid, labels);

            QCOMPARE(f.scene.labels.size(), labels ? 10 : 0);
            const QImage img = renderCpu(f);
            QVERIFY2(inkCountAll(img) > 300, "任一组合下轴脊应可见");

            // 轴脊始终可见：中心交点 cart(0,0) → (200,200)
            const QPoint pc = pixOf(0.0, 0.0);
            QVERIFY2(inkCount(img, pc.x(), pc.y(), 3) > 0, "轴脊中心交点应落屏");

            // 网格采样点：垂直网格线 x=4 与水平网格线 y=6 上的点 cart(4,6) → (280,80)
            // （离轴脊与标签排版区都足够远，网格关→白，网格开→有墨）
            const QPoint pg = pixOf(4.0, 6.0);
            const int n = inkCount(img, pg.x(), pg.y(), 2);
            if (grid)
                QVERIFY2(n > 0, qPrintable(QString("网格开：%1,%2 应有网格线墨迹").arg(pg.x()).arg(pg.y())));
            else
                QVERIFY2(n == 0, qPrintable(QString("网格关：%1,%2 应为空白").arg(pg.x()).arg(pg.y())));

            if (labels) {
                AxisFixture off(ProjectionKind::Cartesian);
                off.build(grid, false);
                const QImage imgOff = renderCpu(off);
                QVERIFY2(inkCountAll(img) > inkCountAll(imgOff) + 20,
                         "标签开时应比标签关多出文字像素");
            }
        }
    }
}

// ===== Polar：轴脊（外环 + 径向）与刻度点真实落屏 =====
void TestAxisPipeline::polarSpineTicksRender()
{
    AxisFixture f(ProjectionKind::Polar);
    f.build(/*grid=*/false, /*labels=*/false);

    QVERIFY2(f.scene.primitives.size() > 40, "drawAtPosition 应提交外环/径向脊图元");
    QCOMPARE(f.scene.labels.size(), 0);

    const QImage img = renderCpu(f);
    QVERIFY2(inkCountAll(img) > 400, "极坐标轴脊（环+径向线）应产生大量非白像素");

    // 径向脊：cart(2, 0) → (240, 200)（θ=0 半轴，r=2 刻度点亦在此）
    const QPoint pr = pixOf(2.0, 0.0);
    QVERIFY2(inkCount(img, pr.x(), pr.y(), 3) > 0,
             qPrintable(QString("径向脊/刻度未落屏 @%1,%2").arg(pr.x()).arg(pr.y())));
    // 外环：r=10、θ=45° → cart(7.071, 7.071) → (341,59)
    const QPoint po = pixOf(7.0710678, 7.0710678);
    QVERIFY2(inkCount(img, po.x(), po.y(), 4) > 0,
             qPrintable(QString("外环未落屏 @%1,%2").arg(po.x()).arg(po.y())));
    // 环轴 θ 刻度点图元（θ=80 主刻度，7 点形态的径向内臂落在 r=10−5.4=4.6、
    // θ=80° → cart(0.799, 4.530) → (216,109)）：远离外环/径向脊，能区分刻度点与脊线
    const QPoint ptArm = pixOf(0.7989385, 4.5295340);
    QVERIFY2(inkCount(img, ptArm.x(), ptArm.y(), 2) > 0,
             qPrintable(QString("极坐标环轴刻度点未落屏（疑似刻度点图元缺失）@%1,%2")
                        .arg(ptArm.x()).arg(ptArm.y())));
}

// ===== Polar：网格 × 标签 组合 =====
void TestAxisPipeline::polarGridLabelCombos()
{
    for (bool grid : {false, true}) {
        for (bool labels : {false, true}) {
            AxisFixture f(ProjectionKind::Polar);
            f.build(grid, labels);

            // 环轴 5 个 θ 刻度 + 径向轴 6 个 r 刻度（含原点/外端）→ 11 个标签
            QCOMPARE(f.scene.labels.size(), labels ? 11 : 0);
            const QImage img = renderCpu(f);
            QVERIFY2(inkCountAll(img) > 400, "任一组合下极轴脊应可见");

            // 径向脊 + 原点：cart(2,0) → (240,200)
            const QPoint pr = pixOf(2.0, 0.0);
            QVERIFY2(inkCount(img, pr.x(), pr.y(), 3) > 0, "径向脊应始终落屏");

            // 网格采样点 A：同心环 r=4、θ=45° → cart(2.828,2.828) → (257,143)
            const QPoint pgA = pixOf(2.8284271, 2.8284271);
            const int na = inkCount(img, pgA.x(), pgA.y(), 2);
            if (grid)
                QVERIFY2(na > 0, qPrintable(QString("网格开：环采样 %1,%2 应有墨迹").arg(pgA.x()).arg(pgA.y())));
            else
                QVERIFY2(na == 0, qPrintable(QString("网格关：环采样 %1,%2 应为空白").arg(pgA.x()).arg(pgA.y())));

            // 网格采样点 B：辐条 θ=80°、r=4 → cart(0.695,3.939) → (214,121)
            const QPoint pgB = pixOf(0.6945927, 3.9392310);
            const int nb = inkCount(img, pgB.x(), pgB.y(), 2);
            if (grid)
                QVERIFY2(nb > 0, qPrintable(QString("网格开：辐条采样 %1,%2 应有墨迹").arg(pgB.x()).arg(pgB.y())));
            else
                QVERIFY2(nb == 0, qPrintable(QString("网格关：辐条采样 %1,%2 应为空白").arg(pgB.x()).arg(pgB.y())));

            if (labels) {
                AxisFixture off(ProjectionKind::Polar);
                off.build(grid, false);
                const QImage imgOff = renderCpu(off);
                QVERIFY2(inkCountAll(img) > inkCountAll(imgOff) + 20,
                         "标签开时应比标签关多出文字像素");
            }
        }
    }
}
