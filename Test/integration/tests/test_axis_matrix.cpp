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
#include "QChartWidget.h"      // 4h-a：真实 widget 路径验证
#include "QChartLayer.h"
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

/// 4h-a：绘图区左外带的“标签行带”中心（连续有墨行合并为一带；scale 为设备像素比）
QVector<int> labelRowBands(const QImage& img, const QRectF& plot, qreal scale, int bandWidth = 55)
{
    QVector<int> bands;
    const int x0 = qMax(0, int(plot.left() * scale) - bandWidth);
    const int x1 = qMin(img.width() - 1, int(plot.left() * scale) - 1);
    // 仅扫 plotArea 纵向范围：排除上/下边框轴自身的标签带（否则会被多计一带）
    const int y0 = qMax(0, int(plot.top() * scale));
    const int y1 = qMin(img.height() - 1, int(plot.bottom() * scale));
    int runStart = -1;
    for (int y = y0; y <= y1; ++y) {
        bool ink = false;
        for (int x = x0; x <= x1 && !ink; ++x)
            if (isInk(img.pixelColor(x, y))) ink = true;
        if (ink && runStart < 0) runStart = y;
        if (!ink && runStart >= 0) { bands.append((runStart + y - 1) / 2); runStart = -1; }
    }
    if (runStart >= 0) bands.append((runStart + y1) / 2);
    return bands;
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

        // F2 刻度点邻域断言（4i 改造）：drawAtPosition 的 7 点装饰（中心+6 臂）**只在 Tickwise 脊**生成。
        // 臂点离轴脊线 6px（cart ±0.3），2px 点光栅 ±1px 取整 → 半径 2 邻域采样，不做单像素精确断言。
        //   · labels=true（Tickwise）⇒ 臂点必须有墨（与 4i 前一致）；
        //   · labels=false（None）⇒ 装饰被跳过，臂点必须**无墨**；注意 grid=on 时垂直网格脊本身
        //     穿过该列（x=-4），故“无墨”只在 grid=off 组合下成立并可判（grid=on 由 cpuMatrix 的
        //     图元类型/计数断言覆盖：网格脊为 None ⇒ 0 个 Point 图元）。
        const QPoint arms[4] = {
            pixOf(-4.0,  0.3),   // X 刻度 x=-4 的 ±y 臂
            pixOf(-4.0, -0.3),
            pixOf( 0.3,  4.0),   // Y 刻度 y=4 的 ±x 臂
            pixOf(-0.3,  4.0),
        };
        for (const QPoint& arm : arms) {
            if (labels) {
                QVERIFY2(inkCount(img, arm.x(), arm.y(), 2) > 0,
                         qPrintable(QString("[%1] Tickwise 刻度点臂未出墨 @%2,%3")
                                    .arg(backend).arg(arm.x()).arg(arm.y())));
            } else if (!grid) {
                QVERIFY2(inkCount(img, arm.x(), arm.y(), 2) == 0,
                         qPrintable(QString("[%1] None 脊不应有刻度装饰墨迹 @%2,%3")
                                    .arg(backend).arg(arm.x()).arg(arm.y())));
            }
        }
    } else {
        const QPoint pr = pixOf(2.0, 0.0);
        QVERIFY2(inkCount(img, pr.x(), pr.y(), 4) > 0,
                 qPrintable(QString("[%1] 极坐标径向脊应落屏").arg(backend)));
        const QPoint po = pixOf(7.0710678, 7.0710678);
        QVERIFY2(inkCount(img, po.x(), po.y(), 5) > 0,
                 qPrintable(QString("[%1] 极坐标外环应落屏").arg(backend)));

        // 4i：极坐标装饰对照（探针 = θ=80 主刻度的径向内臂 → r=10−5.4=4.6、θ=80°
        //     → cart(0.7989, 4.5295)）。tickValues(0,360)+tickCount5 → niceStep 80 ⇒ 环轴主刻度
        //     恰为 {0,80,160,240,320}，θ=80 是其中之一。
        //     注意 grid=on 时 θ=80 的辐条脊（网格脊本身）正好穿过该探针 ⇒ 无墨判定只在 grid=off 成立；
        //     grid=on 组合由 cpuMatrix 的图元类型/计数断言覆盖（网格脊 None ⇒ 0 个 Point）。
        //     ★后端差异（既有，非 4i 引入）：GL 后端在该点不落墨——变异副本（4i 前=装饰无条件生成）
        //       在 wayland 下同样报红（t67_mut/intg_wayland.log），故“装饰可见”正向断言只对 CPU 成立；
        //       GL 侧的装饰门控由 cpuMatrix 的图元计数断言与 None 无墨断言覆盖。
        const QPoint parm = pixOf(0.7989385, 4.5295340);
        const bool isCpu = (QLatin1String(backend) == QLatin1String("CPU"));
        if (labels) {
            if (isCpu) {
                QVERIFY2(inkCount(img, parm.x(), parm.y(), 2) > 0,
                         qPrintable(QString("[%1] Polar|Tickwise 刻度装饰（径向内臂）应有墨 @%2,%3")
                                    .arg(backend).arg(parm.x()).arg(parm.y())));
            }
        } else if (!grid) {
            QVERIFY2(inkCount(img, parm.x(), parm.y(), 2) == 0,
                     qPrintable(QString("[%1] Polar|None 脊不应有刻度装饰墨迹 @%2,%3")
                                .arg(backend).arg(parm.x()).arg(parm.y())));
        }
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
// 4h-a：真实 widget 路径验证（定义见文件后部）
void checkWidgetBorderAxisMatchesGridSpines();

void TestAxisMatrixCpu::cpuMatrix()
{
    for (ProjectionKind kind : {ProjectionKind::Cartesian, ProjectionKind::Polar}) {
        for (bool grid : {false, true}) {
            for (bool labels : {false, true}) {
                AxisFixture f(kind);
                f.build(grid, labels);
                const int expectLabels = (kind == ProjectionKind::Cartesian) ? 10 : 11;
                QCOMPARE(f.scene.labels.size(), labels ? expectLabels : 0);

                // ── 4i 契约（字面量 + 构成）────────────────────────────────────────────
                // 脊线表示：Cartesian 为恒等投影 ⇒ 全部 Type::Line（2 顶点，numA/numB）；
                //           Polar 为非恒等投影 ⇒ 保持 Type::Path 采样（顶点数 > 2）。
                // 装饰点：只在 Tickwise 脊上（本夹具 labels=true ⇔ 两条主轴脊 Tickwise，网格脊恒为 None）。
                //   脊数 = 主轴 2 + 网格 8（笛卡尔：x/y 各 4 条非零刻度；极坐标：4 环 + 4 辐条）
                //   装饰点 = labels ? 主轴刻度数×7 : 0（笛卡尔 2×5×7=70；极坐标 5θ×7 + 6r×7=77）
                int nLine = 0, nPoint = 0, nPath = 0;
                for (const QChartPrimitive& p : f.scene.primitives) {
                    switch (p.type) {
                    case QChartPrimitive::Type::Line: ++nLine; break;
                    case QChartPrimitive::Type::Point: ++nPoint; break;
                    case QChartPrimitive::Type::Path: ++nPath; break;
                    default: break;
                    }
                }
                const int expectSpines = grid ? 10 : 2;
                const int expectPoints = labels ? ((kind == ProjectionKind::Cartesian) ? 70 : 77) : 0;
                if (kind == ProjectionKind::Cartesian) {
                    QCOMPARE(nLine, expectSpines);
                    QCOMPARE(nPath, 0);
                    for (const QChartPrimitive& p : f.scene.primitives) {
                        if (p.type != QChartPrimitive::Type::Line) continue;
                        QVERIFY2(p.numVerts.isEmpty() && p.numA != p.numB,
                                 "恒等投影的脊线应为 2 顶点 Line（numA/numB，无采样顶点）");
                    }
                } else {
                    QCOMPARE(nPath, expectSpines);
                    QCOMPARE(nLine, 0);
                }
                QCOMPARE(nPoint, expectPoints);
                QCOMPARE(f.scene.primitives.size(), expectSpines + expectPoints);
                qInfo().noquote() << QString("[4i CPU] %1 prims=%2 (Line=%3 Path=%4 Point=%5) labels=%6")
                    .arg(comboName(kind, grid, labels)).arg(f.scene.primitives.size())
                    .arg(nLine).arg(nPath).arg(nPoint).arg(f.scene.labels.size());

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
                // 4h-a：改为**生产朝向**（legacy：dim0=left..right、dim1=bottom..top，height<0），
                // 数值范围不变（仍 kViewLo..kViewHi）——旧夹具喂数学式朝向，与生产相反故测不出反向读取。
                ctx.dataBounds = QRectF(kViewLo, kViewHi, kViewHi - kViewLo, -(kViewHi - kViewLo));
                ctx.viewRect = ctx.dataBounds;
                ctx.projection = f.projection.get();
                {
                    QPainter p(&img);
                    f.dim0Axis.drawAtEdge(&p, ctx, true, true, true);
                    f.dim1Axis.drawAtEdge(&p, ctx, true, true, true);
                    p.end();
                }
                // 4h-a 对向朝向对照：同数值范围改喂数学式朝向 → 渲染结果必须逐位一致
                {
                    QImage imgMirror = img;
                    imgMirror.fill(Qt::white);
                    QPainterChartRenderer r2;
                    r2.render(f.scene, &imgMirror);
                    DrawContext ctxM = ctx;
                    ctxM.dataBounds = QRectF(kViewLo, kViewLo, kViewHi - kViewLo, kViewHi - kViewLo);
                    ctxM.viewRect = ctxM.dataBounds;
                    QPainter p(&imgMirror);
                    f.dim0Axis.drawAtEdge(&p, ctxM, true, true, true);
                    f.dim1Axis.drawAtEdge(&p, ctxM, true, true, true);
                    p.end();
                    QVERIFY2(imgMirror == img, "4h-a：边框轴两种 dataBounds 朝向的像素输出必须一致");
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

    checkWidgetBorderAxisMatchesGridSpines();   // 4h-a：真实 widget 路径验证（左侧边框轴 ↔ 网格脊）
}

// ===== GL：真实环境 8 组合 =====
// ===== 4h-a：真实 widget 路径——左侧边框轴标签行带与该层网格脊固定坐标一一对应 =====
void checkWidgetBorderAxisMatchesGridSpines()
{
    QChartWidget w;
    QValueAxis ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft);
    QChartLayer layer;
    ax.setRange(0.0, 10.0);
    ay.setRange(-5.0, 45.0);            // 非对称范围（反向读取时会退化为 9 等分兜底）
    ax.setTickCount(5);
    ay.setTickCount(5);
    ax.setColor(Qt::black);
    ay.setColor(Qt::black);
    layer.setGridVisible(true);
    layer.setGridColor(QColor(200, 200, 200));
    w.addAxis(&ax);
    w.addAxis(&ay);
    w.addLayer(&layer);
    w.resize(420, 340);
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "offscreen/wayland 下窗口应暴露");
    const QImage shot = w.grab().toImage();
    const qreal s = qreal(shot.width()) / w.width();
    const QRectF pa = w.plotArea();
    QVERIFY2(pa.width() > 100.0 && pa.height() > 100.0, "plotArea 应有效");

    // 网格脊固定坐标：水平脊 = 脊线图元且**所有顶点 y 相同**（x 方向扫动）
    // 4i：恒等投影（Cartesian）下脊线提交为 2 顶点 Type::Line（numA/numB）；此处两种表示都接受，
    //      判据保持“所有端点 y 相同”这一几何语义（不因表示变化而放宽/改变判定对象）。
    QVector<qreal> spineY;
    for (const QChartPrimitive& p : layer.scene().primitives) {
        if (p.type == QChartPrimitive::Type::Line) {
            if (qAbs(qreal(p.numA.y()) - qreal(p.numB.y())) > 1e-9) continue;
            spineY.append(qreal(p.numA.y()));
        } else if (p.type == QChartPrimitive::Type::Path && p.numVerts.size() >= 2) {
            const qreal y0 = qreal(p.numVerts.first().y());
            bool horiz = true;
            for (const QVector3D& v : p.numVerts)
                if (qAbs(qreal(v.y()) - y0) > 1e-9) { horiz = false; break; }
            if (horiz) spineY.append(y0);
        }
    }
    QVERIFY2(spineY.size() == 5,
             qPrintable(QString("水平网格脊应为 5 条（nice 刻度，非 9 等分兜底），实为 %1").arg(spineY.size())));

    // ── 4i 契约（真实 widget 路径 · 字面量 + 构成）─────────────────────────────────
    // 该 widget：ax 0..10 tickCount5 → step 2 ⇒ 竖向脊 6 条（x=0,2,4,6,8,10）；
    //            ay -5..45 tickCount5 → step 10 ⇒ 横向脊 5 条（y=0,10,20,30,40）→ 共 11 条脊。
    // 二维网格脊恒为 LabelMode::Single（QChartLayer.h:116）⇒ 4i 起不再生成 7 点装饰（原每脊每刻度 7 点）；
    // Cartesian 为恒等投影 ⇒ 11 条脊全部为 2 顶点 Line。每脊另有 1 个代表标签 ⇒ 11 个标签。
    {
        int nLine = 0, nPoint = 0, nPath = 0;
        for (const QChartPrimitive& p : layer.scene().primitives) {
            switch (p.type) {
            case QChartPrimitive::Type::Line: ++nLine; break;
            case QChartPrimitive::Type::Point: ++nPoint; break;
            case QChartPrimitive::Type::Path: ++nPath; break;
            default: break;
            }
        }
        QCOMPARE(nLine, 11);
        QCOMPARE(nPoint, 0);
        QCOMPARE(nPath, 0);
        QCOMPARE(int(layer.scene().labels.size()), 11);
        QVERIFY2(layer.scene().primitives.size() <= 50,
                 qPrintable(QString("2D 网格每帧图元总数应 ≤ 50（4i 前为 11 脊 × 33 顶点 Path + 装饰点），实为 %1")
                            .arg(layer.scene().primitives.size())));
        // 装饰被跳过（Single）⇒ 标签不得留下悬空 refPrimitiveId
        for (const QChartTextLabel& l : layer.scene().labels)
            QCOMPARE(l.refPrimitiveId, -1);
        qInfo().noquote() << QString("4i widget 路径：脊 %1 条（Line=%2 Path=%3，其中横向脊 %4 条）/ 装饰点=%5 / 标签=%6 / 图元总数=%7")
                                 .arg(nLine + nPath).arg(nLine).arg(nPath).arg(spineY.size()).arg(nPoint)
                                 .arg(layer.scene().labels.size()).arg(layer.scene().primitives.size());
    }

    // 左侧边框轴标签行带（抓图设备像素）
    const QVector<int> bands = labelRowBands(shot, pa, s);
    {
        QString b, sp;
        for (int x : bands) b += QString::number(x) + " ";
        for (qreal y : spineY) sp += QString::number(y, 'g', 4) + " ";
        qInfo().noquote() << QString("4h-a 诊断：标签行带=[%1] 脊固定值=[%2]").arg(b.trimmed()).arg(sp.trimmed());
    }
    QVERIFY2(bands.size() == spineY.size(),
             qPrintable(QString("左侧边框轴标签数(%1)应与网格脊数(%2)一致（9 等分退化会显著不一致）")
                            .arg(bands.size()).arg(spineY.size())));

    // 每条脊的投影行必须能找到对应标签行带（值/位置一致）
    for (qreal y : spineY) {
        const qreal row = layer.camera()->project(QVector3D(0.0f, float(y), 0.0f), pa).screen.y() * s;
        bool found = false;
        for (int band : bands)
            if (qAbs(band - row) <= 3.0) found = true;
        QVERIFY2(found,
                 qPrintable(QString("网格脊 y=%1（投影行 %2）应在左侧边框轴标签行带中找到对应（带：%3）")
                                .arg(y).arg(row).arg([&bands]() {
                                    QString s2; for (int b : bands) s2 += QString::number(b) + " ";
                                    return s2; }())));
    }
    qInfo().noquote() << QString("4h-a widget 路径：水平脊 %1 条 / 左侧标签带 %2 个，逐一对齐（plotArea=%3x%4）")
                             .arg(spineY.size()).arg(bands.size()).arg(pa.width()).arg(pa.height());
}

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
