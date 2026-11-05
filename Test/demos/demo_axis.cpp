// demo_axis.cpp —— 批次 B4/B2：轴阶段演示（2D 轴 + 3D 轴三网格模式，CPU/GL 可切）
// 用法（框架见 Test/demos/test.cpp）：
//   QChartDemo                       → 跑全部 demo（axis + axis3d，CPU 后端）
//   QChartDemo axis   [cpu|gl]       → 2D 演示（每条网格脊 1 个数值标签，批次2 A）
//   QChartDemo axis3d [cpu|gl] [box|faceline|lattice]
//                                    → 3D 演示，模式参数缺省 faceline（批次2 B 默认）；
//                                      也可用环境变量 QCHART_DEMO_3D_GRID=box|faceline|lattice
// 出图：QCHART_DEMO_SHOT=1 时框架存 demo_<name>.png（GL 另存 _glhost.png）；
//      3D 演示额外按模式存 demo_axis3d_<mode>.png（模式间出图产物不互相覆盖）。
// 4f 交互（可玩；CPU/GL 两后端一致；交互开关默认开，见 QChartAbstractWidget::setInteractionEnabled）：
//   2D（axis）    ：左键拖动 = 平移视图（像素位移 → 视图坐标位移）；滚轮 = 以光标为中心缩放
//                  （中心处数据坐标不变）。
//   3D（axis3d）  ：左键拖动 = 旋转（orbit，俯仰钳制 ±89°）；滚轮 = 推拉（dolly 改视野盒尺寸，
//                  autoFit 开启时随之重算 distance/近远面；关闭时保留手调参数）；
//                  中键/右键拖动 = 平移视野盒中心（panViewCube）。
//   出图路径不受影响：QCHART_DEMO_SHOT=1 时演示不注入任何鼠标事件，产物与无交互时逐字节一致。
#include "demos.h"
#include "QChartWidget.h"
#include "QChartWidget3D.h"
#include "QChartLayer.h"
#include "QChartCamera3D.h"
#include "QValueAxis.h"
#include "QCartesianProjection.h"
#include "QCartesianProjection3D.h"
#include "QPolarProjection.h"
#include "QScatterSeries.h"
#include "QLineSeries.h"
#include "QPolygonSeries.h"
#include "QBarSeries.h"
#include "QChartScatterSeries3D.h"
#include "QChartLineSeries3D.h"
#include "QChartSurfaceSeries.h"
#include "QSphericalProjection3D.h"
#include "QCylindricalProjection3D.h"
#include "QOpenGLWidget"
#include <QWidget>
#include <QApplication>
#include <QSet>
#include <cmath>
#include <memory>
#include <QTimer>
#include <QPixmap>
#include <QDebug>
#include "QChartDebug.h"   // 脏变量观测（chart.dirty）

static void applyBackend(QChartAbstractWidget* w, DemoBackend b)
{
    w->setRenderBackend(b == DemoBackend::Gl
        ? QChartAbstractWidget::RenderBackend::OpenGL
        : QChartAbstractWidget::RenderBackend::QPainter);
}

// ===== 批次2 B：3D 网格模式参数解析（CLI 额外 token 优先，其次环境变量，默认 FaceLine）=====
namespace {

QChartLayer3D::GridMode gridModeFromParam()
{
    QString key;
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString a = args.at(i).trimmed().toLower();
        if (a == QLatin1String("box") || a == QLatin1String("faceline") || a == QLatin1String("lattice")) {
            key = a;
            break;
        }
    }
    if (key.isEmpty())
        key = qEnvironmentVariable("QCHART_DEMO_3D_GRID").trimmed().toLower();
    if (key == QLatin1String("box")) return QChartLayer3D::GridMode::Box;
    if (key == QLatin1String("lattice")) return QChartLayer3D::GridMode::Lattice;
    return QChartLayer3D::GridMode::FaceLine;   // 默认（批次2 B）
}

QString gridModeKey(QChartLayer3D::GridMode m)
{
    switch (m) {
    case QChartLayer3D::GridMode::Box:      return QStringLiteral("box");
    case QChartLayer3D::GridMode::Lattice:  return QStringLiteral("lattice");
    case QChartLayer3D::GridMode::FaceLine: return QStringLiteral("faceline");
    }
    return QStringLiteral("unknown");
}

// ===== 坐标系参数解析（CLI token，缺省 Cartesian）=====
bool isPolarParam()
{
    return QCoreApplication::arguments().contains(QLatin1String("polar"), Qt::CaseInsensitive);
}

enum class Proj3D { Cartesian, Spherical, Cylindrical };

Proj3D projection3DFromParam()
{
    const QStringList args = QCoreApplication::arguments();
    if (args.contains(QLatin1String("spherical"), Qt::CaseInsensitive)) return Proj3D::Spherical;
    if (args.contains(QLatin1String("cylindrical"), Qt::CaseInsensitive)) return Proj3D::Cylindrical;
    return Proj3D::Cartesian;
}

QString projection3DKey(Proj3D p)
{
    switch (p) {
    case Proj3D::Spherical:   return QStringLiteral("spherical");
    case Proj3D::Cylindrical: return QStringLiteral("cylindrical");
    case Proj3D::Cartesian:   break;
    }
    return QStringLiteral("cartesian");
}

/// 出图取证摘要（批次2 A/B + t31）：二维网格脊数 vs 自由标签数；按后端标注渲染预期
///   CPU：自由标签可渲染 → 每线一个数值标签（脊数==标签数）
///   GL ：既定契约 = 纯 GPU 后端不渲染自由标签 → 网格线标签在本后端不显示（已知差异）
void logSceneSummary2D(QChartLayer* layer, DemoBackend backend)
{
    if (!layer) return;
    const QChartScene& sc = layer->scene();
    const int spineCount = sc.axisSegments.size();
    const int seriesCount = sc.seriesSegments.size();
    int labels = 0, freeLabels = 0, spinesWithLabel = 0;
    for (const QChartSegment& seg : sc.axisSegments) {
        if (!seg.labels.isEmpty()) ++spinesWithLabel;
        for (const QChartTextLabel& l : seg.labels) {
            ++labels;
            if (l.segPrim == -1 && std::isnan(l.numericAnchor.x())) ++freeLabels;
        }
    }
    for (const QChartSegment& seg : sc.seriesSegments) labels += seg.labels.size();

    const bool gl = (backend == DemoBackend::Gl);
    qInfo().noquote() << QString("shot-summary axis[%1]: spines=%2 series=%3 labels=%4 freeLabels=%5 (%6)")
                             .arg(gl ? "gl" : "cpu")
                             .arg(spineCount).arg(seriesCount).arg(labels).arg(freeLabels)
                             .arg(gl ? QStringLiteral("GL 不渲染自由标签：网格线标签在本后端不显示（既定契约）")
                                     : (spinesWithLabel == spineCount
                                            ? QStringLiteral("每线一标签 OK")
                                            : QStringLiteral("MISMATCH")));
}

void logSceneSummary3D(QChartLayer3D* layer, QChartLayer3D::GridMode mode)
{
    if (!layer) return;
    const QChartScene& sc = layer->scene3D();
    int primCount = 0, labelCount = 0;
    for (const QChartSegment& s : sc.axisSegments) { primCount += s.prims.size(); labelCount += s.labels.size(); }
    for (const QChartSegment& s : sc.seriesSegments) { primCount += s.prims.size(); labelCount += s.labels.size(); }
    qInfo().noquote() << QString("shot-summary axis3d[%1]: prims=%2 labels=%3")
                             .arg(gridModeKey(mode)).arg(primCount).arg(labelCount);

    // 几何自检（诊断用）：环 = 固定 r/φ、θ 从 0 扫到 360 的 Path。
    // 打印环数量、World 空间首尾距离（>1e-6 即未闭合）、以及每环的 (r, φ)。
    if (mode != QChartLayer3D::GridMode::Lattice) return;
    const QChartProjection3D* proj = layer->projection3D();
    int rings = 0, openRings = 0;
    auto scanSegment = [&](const QChartSegment& seg) {
        for (const auto& prim : seg.prims) {
            if (prim.type != QChartPrimitive::Type::Path || prim.numVerts.size() < 2) continue;
            const QVector3D& a = prim.numVerts.first();
            const QVector3D& b = prim.numVerts.last();
            if (qAbs(a.x() - b.x()) > 1e-9 || qAbs(a.z() - b.z()) > 1e-9) continue;      // 只认 y(θ) 扫动的
            if (qAbs(qAbs(b.y() - a.y()) - 360.0) > 1e-6) continue;                      // 跨度须为 360
            ++rings;
            const QVector3D wa = proj ? proj->toCartesian(a) : a;
            const QVector3D wb = proj ? proj->toCartesian(b) : b;
            const qreal d = (wa - wb).length();
            if (d > 1e-6) {
                ++openRings;
                if (openRings <= 6)
                    qInfo().noquote() << QString("  open-ring r=%1 phi=%2 dWorld=%3")
                                             .arg(a.x()).arg(a.z()).arg(d);
            }
        }
    };
    for (const QChartSegment& s : sc.axisSegments) scanSegment(s);
    for (const QChartSegment& s : sc.seriesSegments) scanSegment(s);
    qInfo().noquote() << QString("  lattice rings=%1 openRings=%2").arg(rings).arg(openRings);
}

/// 二维摘要调度：QCHART_DEMO_SHOT=1 时等首帧收集完成再记录（GL 宿主首帧可能晚于 280ms；
/// 最迟 360ms 记录一次，早于框架 400ms 截图与退出）
void scheduleSummary2D(QChartLayer* layer, DemoBackend backend)
{
    if (!qEnvironmentVariableIsSet("QCHART_DEMO_SHOT")) return;
    auto* tick = new QTimer(layer);
    tick->setInterval(60);
    auto waited = std::make_shared<int>(0);
    QObject::connect(tick, &QTimer::timeout, layer, [tick, layer, backend, waited]() {
        *waited += 60;
        const bool ready = !layer->scene().axisSegments.isEmpty()
                           || !layer->scene().seriesSegments.isEmpty();
        if (!ready && *waited < 360) return;
        tick->stop();
        logSceneSummary2D(layer, backend);
    });
    tick->start();
}

/// QCHART_DEMO_SHOT=1：按模式名额外出图（GL 后端另存 GL 宿主 FBO）
void scheduleModeShot(QChartWidget3D* w3, QChartLayer3D::GridMode mode, DemoBackend backend)
{
    if (!qEnvironmentVariableIsSet("QCHART_DEMO_SHOT")) return;
    // 等首帧收集完成再出图/记录（GL 宿主首帧可能晚于 300ms；最迟 360ms，早于框架 400ms 截图）
    auto* tick = new QTimer(w3);
    tick->setInterval(60);
    auto waited = std::make_shared<int>(0);
    QObject::connect(tick, &QTimer::timeout, w3, [tick, w3, mode, backend, waited]() {
        *waited += 60;
        const bool ready = w3->layer3D()
                           && (!w3->layer3D()->scene3D().axisSegments.isEmpty()
                               || !w3->layer3D()->scene3D().seriesSegments.isEmpty());
        if (!ready && *waited < 360) return;
        tick->stop();
        logSceneSummary3D(w3->layer3D(), mode);
        const QString base = QStringLiteral("demo_axis3d_%1").arg(gridModeKey(mode));
        const bool ok = w3->grab().save(base + QStringLiteral(".png"));
        qInfo().noquote() << QString("shot-mode %1 saved=%2").arg(base + ".png").arg(ok);
        if (backend == DemoBackend::Gl) {
            if (auto* host = qobject_cast<QOpenGLWidget*>(w3->glHostWidget())) {
                const QString fboFile = base + QStringLiteral("_glhost.png");
                qInfo().noquote() << QString("shot-mode %1 saved=%2")
                                         .arg(fboFile).arg(host->grabFramebuffer().save(fboFile));
            }
        }
    });
    tick->start();
}

} // namespace

// ===== 2D：Cartesian value×value + 网格（批次2 A：每条网格脊 1 个数值标签）+ 边框轴 =====
QWidget* buildDemoAxis(DemoBackend backend)
{
    if (isPolarParam()) {
        // 2D 极坐标：角度轴 HCenter + 半径轴 VCenter（数据主脊；polar 下边框轴不适用）
        auto* w = new QChartWidget;
        w->setProjection(std::make_unique<QPolarProjection>());
        auto* angle = new QValueAxis(w, Qt::AlignHCenter);
        angle->setRange(0.0, 360.0);
        angle->setTickInterval(45.0);
        angle->setLabelFormat(QStringLiteral("%g°"));
        angle->setColor(Qt::black);
        auto* radial = new QValueAxis(w, Qt::AlignVCenter);
        radial->setRange(0.0, 5.0);
        radial->setTickCount(6);
        radial->setColor(Qt::black);
        auto* layer = new QChartLayer(w);
        layer->setGridVisible(true);
        w->addAxis(angle);
        w->addAxis(radial);
        w->addLayer(layer);
        applyBackend(w, backend);
        w->setWindowTitle(QStringLiteral("demo_axis polar [%1]")
                          .arg(backend == DemoBackend::Gl ? "gl" : "cpu"));
        w->resize(720, 540);
        scheduleSummary2D(layer, backend);
        return w;
    }

    auto* w = new QChartWidget;
    auto* x = new QValueAxis(w, Qt::AlignBottom);
    auto* y = new QValueAxis(w, Qt::AlignLeft);
    x->setRange(-10.0, 10.0);
    x->setTickCount(9);
    x->setColor(Qt::black);
    y->setRange(-10.0, 10.0);
    y->setTickCount(9);
    y->setColor(Qt::black);

    auto* layer = new QChartLayer(w);     // QObject 子，基类容器非持有
    layer->setGridVisible(true);
    w->addAxis(x);
    w->addAxis(y);
    w->addLayer(layer);                   // 便捷轴挂到首层（边框轴由外层 drawAtEdge 绘制）

    applyBackend(w, backend);
    w->setWindowTitle(QStringLiteral("demo_axis [%1]")
                      .arg(backend == DemoBackend::Gl ? "gl" : "cpu"));
    w->resize(720, 540);
    scheduleSummary2D(layer, backend);
    return w;
}

// ===== 散点：同一份代码只换投影指针（缺省 cartesian；参数 polar 切极坐标）=====
// 两个变体的窗口构建代码完全相同，唯一差异是 setProjection 那一行。
QWidget* buildDemoScatter(DemoBackend backend)
{
    const bool polar = QCoreApplication::arguments().contains(QLatin1String("polar"), Qt::CaseInsensitive);

    auto* w = new QChartWidget;
    if (polar)
        w->setProjection(std::make_unique<QPolarProjection>());        // ← 唯一差异
    else
        w->setProjection(std::make_unique<QCartesianProjection>());

    auto* x = new QValueAxis(w, Qt::AlignBottom);
    auto* y = new QValueAxis(w, Qt::AlignLeft);
    x->setRange(0.0, 360.0);
    x->setTickCount(9);
    x->setColor(Qt::black);
    y->setRange(0.0, 360.0);   // 与 x 同尺度（避免极端宽高比触发 Preserve 面积拟合的压缩）
    y->setTickCount(5);
    y->setColor(Qt::black);

    auto* layer = new QChartLayer(w);
    layer->setGridVisible(true);
    w->addAxis(x);
    w->addAxis(y);
    w->addLayer(layer);

    // 数据：θ∈[0,360) + r 螺旋（同一份数据；Cartesian 下是斜带，polar 下是螺旋）
    auto* series = new QScatterSeries(QStringLiteral("scatter"), w);
    series->setMarkerSize(3);
    series->setColor(QColor("#1E88E5"));
    const int N = 5000;
    for (int i = 0; i < N; ++i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(N);
        series->append(t * 360.0, 20.0 + 340.0 * t + 12.0 * std::sin(t * 60.0));
    }
    layer->addSeries(series);

    applyBackend(w, backend);
    w->setWindowTitle(QStringLiteral("demo_scatter [%1|%2]")
                      .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                      .arg(polar ? "polar" : "cartesian"));
    w->resize(720, 540);
    scheduleSummary2D(layer, backend);
    return w;
}

// ===== 3D 散点：只收集 Numeric 图元，投影/相机全由 renderer 完成 =====
QWidget* buildDemoScatter3D(DemoBackend backend)
{
    // 投影实例须覆盖窗口生命周期（demo 进程单次运行，静态持有即可）
    static QCartesianProjection3D s_cart;

    auto* w3 = new QChartWidget3D;
    w3->setProjection3D(&s_cart);
    w3->setGridMode3D(gridModeFromParam());   // box|faceline|lattice（CLI token / QCHART_DEMO_3D_GRID）
    w3->setDataBounds3D(QCube(QVector3D(-3, -3, -3), QVector3D(3, 3, 3)));

    // 数据：Fibonacci 球面点云（半径 2.2 ± 0.25 扰动）
    auto* series = new QChartScatterSeries3D(QStringLiteral("scatter3d"), w3);
    series->setMarkerSize(4.0);
    series->setColor(QColor("#1E88E5"));
    const int N = 3000;
    for (int i = 0; i < N; ++i) {
        const qreal u = 1.0 - 2.0 * (i + 0.5) / N;              // [-1,1]
        const qreal rho = std::sqrt(qMax(0.0, 1.0 - u * u));
        const qreal theta = M_PI * (1.0 + std::sqrt(5.0)) * i;
        const qreal rad = 2.2 + 0.25 * std::sin(theta * 3.0);
        series->append(rad * rho * std::cos(theta),
                       rad * rho * std::sin(theta),
                       rad * u);
    }
    w3->layer3D()->addSeries3D(series);

    applyBackend(w3, backend);
    w3->setWindowTitle(QStringLiteral("demo_scatter3d [%1|%2]")
                       .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                       .arg(gridModeKey(w3->gridMode3D())));
    w3->resize(720, 540);
    return w3;
}

// ===== 折线：同一份代码只换投影指针（缺省 cartesian；参数 polar 切极坐标）=====
QWidget* buildDemoLine(DemoBackend backend)
{
    const bool polar = QCoreApplication::arguments().contains(QLatin1String("polar"), Qt::CaseInsensitive);

    auto* w = new QChartWidget;
    if (polar)
        w->setProjection(std::make_unique<QPolarProjection>());        // ← 唯一差异
    else
        w->setProjection(std::make_unique<QCartesianProjection>());

    auto* x = new QValueAxis(w, Qt::AlignBottom);
    auto* y = new QValueAxis(w, Qt::AlignLeft);
    x->setRange(-6.0, 6.0);
    x->setTickCount(7);
    x->setColor(Qt::black);
    y->setRange(-6.0, 6.0);
    y->setTickCount(7);
    y->setColor(Qt::black);

    auto* layer = new QChartLayer(w);
    layer->setGridVisible(true);
    w->addAxis(x);
    w->addAxis(y);
    w->addLayer(layer);

    // ① y = 1/x 双分支 + 中间 NaN：演示"同一系列、NaN 断线"
    auto* hyperbola = new QLineSeries(QStringLiteral("1/x"), w);
    hyperbola->setLineWidth(6.0);
    hyperbola->setColor(QColor("#E53935"));
    for (int i = -120; i <= -10; ++i) {
        const qreal t = i / 20.0;
        hyperbola->append(t, 1.0 / t);
    }
    hyperbola->append(qQNaN(), qQNaN());            // 断线
    for (int i = 10; i <= 120; ++i) {
        const qreal t = i / 20.0;
        hyperbola->append(t, 1.0 / t);
    }
    layer->addSeries(hyperbola);

    // ② 正弦折线
    auto* wave = new QLineSeries(QStringLiteral("sin"), w);
    wave->setLineWidth(3.0);
    wave->setColor(QColor("#1E88E5"));
    for (int i = 0; i <= 200; ++i) {
        const qreal t = -6.0 + 12.0 * static_cast<qreal>(i) / 200.0;
        wave->append(t, 3.0 * std::sin(t * 1.5));
    }
    layer->addSeries(wave);

    // ===== 脏变量验证钩子（QCHART_DEMO_DIRTY=1 时启用）=====
    // 程序跑到第 60 秒改一次 series 数据（y 整体 +1.5），用来观察：
    //   dataDirty 是否真的驱动了「收集 + 变换 + 裁剪 + 绘制」，以及图像是否随之更新。
    if (qEnvironmentVariableIsSet("QCHART_DEMO_DIRTY")) {
        auto* timer = new QTimer(wave);
        timer->setSingleShot(true);
        timer->setInterval(60000);
        QObject::connect(timer, &QTimer::timeout, wave, [wave]() {
            QVector<QDataPoint> pts;
            pts.reserve(201);
            for (int i = 0; i <= 200; ++i) {
                const qreal t = -6.0 + 12.0 * static_cast<qreal>(i) / 200.0;
                pts.append(QDataPoint(t, 3.0 * std::sin(t * 1.5) + 1.5));
            }
            wave->setPoints(pts);
            QCHART_DIRTY_MSG(QStringLiteral("demo: series<'sin'> 数据已改动（200 点 y 整体 +1.5）"));
        });
        timer->start();
    }

    applyBackend(w, backend);
    w->setWindowTitle(QStringLiteral("demo_line [%1|%2]")
                      .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                      .arg(polar ? "polar" : "cartesian"));
    w->resize(720, 540);
    scheduleSummary2D(layer, backend);
    return w;
}

// ===== 3D 折线：同一份代码只换投影指针（cartesian | cylindrical | spherical）=====
QWidget* buildDemoLine3D(DemoBackend backend)
{
    const Proj3D projKind = projection3DFromParam();

    // 投影实例须覆盖窗口生命周期（demo 进程单次运行，静态持有即可）
    static QCartesianProjection3D s_cart;
    static QSphericalProjection3D s_sph;
    static QCylindricalProjection3D s_cyl;
    const QChartProjection3D* proj =
        (projKind == Proj3D::Spherical)     ? static_cast<const QChartProjection3D*>(&s_sph)
      : (projKind == Proj3D::Cylindrical)   ? static_cast<const QChartProjection3D*>(&s_cyl)
      :                                       static_cast<const QChartProjection3D*>(&s_cart);

    auto* w3 = new QChartWidget3D;
    w3->setProjection3D(proj);
    w3->setGridMode3D(gridModeFromParam());

    auto* series = new QChartLineSeries3D(QStringLiteral("spiral"), w3);
    series->setLineWidth(2.0);
    series->setColor(QColor("#1E88E5"));

    const int N = 600;
    if (projKind == Proj3D::Cylindrical) {
        // (r, θ, z)：r=2.2 的螺旋
        w3->setDataBounds3D(QCube(QVector3D(0, 0, -3), QVector3D(3, 360, 3)));
        for (int i = 0; i <= N; ++i) {
            const qreal t = static_cast<qreal>(i) / N;
            series->append(2.2, 720.0 * t, 2.5 * std::sin(6.0 * M_PI * t));
        }
    } else if (projKind == Proj3D::Spherical) {
        // (r, θ, φ)：r=2.2 的球面螺旋
        w3->setDataBounds3D(QCube(QVector3D(0, 0, -90), QVector3D(3, 360, 90)));
        for (int i = 0; i <= N; ++i) {
            const qreal t = static_cast<qreal>(i) / N;
            series->append(2.2, 720.0 * t, 60.0 * std::sin(2.0 * M_PI * t));
        }
    } else {
        // (x, y, z)：直角螺旋
        w3->setDataBounds3D(QCube(QVector3D(-3, -3, -3), QVector3D(3, 3, 3)));
        for (int i = 0; i <= N; ++i) {
            const qreal t = 6.0 * M_PI * static_cast<qreal>(i) / N;
            series->append(2.2 * std::cos(t), 2.2 * std::sin(t), 2.5 * std::sin(t * 0.5));
        }
    }
    w3->layer3D()->addSeries3D(series);

    // ===== 脏变量验证钩子（QCHART_DEMO_DIRTY=1 时启用）=====
    // 第 60 秒续写 60 个螺旋点（延伸线段，仍落在 dataBounds 内），观察 dataDirty 是否驱动全链更新。
    if (qEnvironmentVariableIsSet("QCHART_DEMO_DIRTY")) {
        auto* timer = new QTimer(series);
        timer->setSingleShot(true);
        timer->setInterval(60000);
        QObject::connect(timer, &QTimer::timeout, series, [series, projKind]() {
            const int extra = 60;
            for (int i = 1; i <= extra; ++i) {
                const qreal u = static_cast<qreal>(i) / 600.0;
                if (projKind == Proj3D::Cylindrical)
                    series->append(2.2, 720.0 * u + 720.0, 2.5 * std::sin(6.0 * M_PI * u));
                else if (projKind == Proj3D::Spherical)
                    series->append(2.2, 720.0 * u + 720.0, 60.0 * std::sin(2.0 * M_PI * u));
                else {
                    const qreal t = 6.0 * M_PI + 6.0 * M_PI * u;
                    series->append(2.2 * std::cos(t), 2.2 * std::sin(t), 2.5 * std::sin(t * 0.5));
                }
            }
            QCHART_DIRTY_MSG(QStringLiteral("demo: series<'spiral'> 数据已改动（续写 %1 点）").arg(extra));
        });
        timer->start();
    }

    applyBackend(w3, backend);
    w3->setWindowTitle(QStringLiteral("demo_line3d [%1|%2|%3]")
                       .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                       .arg(gridModeKey(w3->gridMode3D()))
                       .arg(projection3DKey(projKind)));
    w3->resize(720, 540);
    return w3;
}

// ===== 多边形：填充（凹形走耳切）+ 可选描边 + NaN 断段；同一份代码只换投影指针 =====
QWidget* buildDemoPolygon(DemoBackend backend)
{
    const bool polar = QCoreApplication::arguments().contains(QLatin1String("polar"), Qt::CaseInsensitive);

    auto* w = new QChartWidget;
    if (polar)
        w->setProjection(std::make_unique<QPolarProjection>());        // ← 唯一差异
    else
        w->setProjection(std::make_unique<QCartesianProjection>());

    auto* x = new QValueAxis(w, Qt::AlignBottom);
    auto* y = new QValueAxis(w, Qt::AlignLeft);
    x->setRange(-6.0, 6.0);
    x->setTickCount(7);
    x->setColor(Qt::black);
    y->setRange(-6.0, 6.0);
    y->setTickCount(7);
    y->setColor(Qt::black);

    auto* layer = new QChartLayer(w);
    layer->setGridVisible(true);
    w->addAxis(x);
    w->addAxis(y);
    w->addLayer(layer);

    // ① 五角星（凹多边形 → 耳切）+ 闭合描边
    auto* star = new QPolygonSeries(QStringLiteral("star"), w);
    star->setColor(QColor("#1E88E5"));
    star->setFillColor(QColor(30, 136, 229));
    star->setStrokeVisible(true);
    for (int i = 0; i < 10; ++i) {
        const qreal a = -M_PI / 2.0 + i * 2.0 * M_PI / 10.0;
        const qreal r = (i % 2 == 0) ? 5.0 : 2.0;
        star->append(r * std::cos(a), r * std::sin(a));
    }
    layer->addSeries(star);

    // ② 两个三角形用 NaN 隔开 → 两个独立多边形（纯填充）
    auto* two = new QPolygonSeries(QStringLiteral("two"), w);
    two->setColor(QColor("#E53935"));
    two->setFillColor(QColor(229, 57, 53));
    two->append(-5.0, -5.0);
    two->append(-2.0, -5.0);
    two->append(-3.5, -1.5);
    two->append(qQNaN(), qQNaN());                 // 断段
    two->append(2.0, -5.0);
    two->append(5.0, -5.0);
    two->append(3.5, -1.5);
    layer->addSeries(two);

    applyBackend(w, backend);
    w->setWindowTitle(QStringLiteral("demo_polygon [%1|%2]")
                      .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                      .arg(polar ? "polar" : "cartesian"));
    w->resize(720, 540);
    scheduleSummary2D(layer, backend);
    return w;
}

// ===== 柱状：每个柱一个 Data 空间矩形；同一份代码只换投影指针 =====
QWidget* buildDemoBar(DemoBackend backend)
{
    const bool polar = QCoreApplication::arguments().contains(QLatin1String("polar"), Qt::CaseInsensitive);

    auto* w = new QChartWidget;
    if (polar)
        w->setProjection(std::make_unique<QPolarProjection>());        // ← 唯一差异
    else
        w->setProjection(std::make_unique<QCartesianProjection>());

    auto* x = new QValueAxis(w, Qt::AlignBottom);
    auto* y = new QValueAxis(w, Qt::AlignLeft);
    x->setRange(-1.0, 11.0);
    x->setTickCount(7);
    x->setColor(Qt::black);
    y->setRange(-6.0, 6.0);
    y->setTickCount(7);
    y->setColor(Qt::black);

    auto* layer = new QChartLayer(w);
    layer->setGridVisible(true);
    w->addAxis(x);
    w->addAxis(y);
    w->addLayer(layer);

    // 10 根柱，值有正有负（基线 y=0）
    auto* bars = new QBarSeries(QStringLiteral("bars"), w);
    bars->setFillColor(QColor(33, 150, 243));
    for (int i = 0; i < 10; ++i) {
        const qreal v = 4.0 * std::sin(i * 1.1) + 1.0;
        bars->append(i + 0.15,   // left
                     v,          // top
                     i + 0.85,   // right
                     0.0);       // bottom
    }
    layer->addSeries(bars);

    applyBackend(w, backend);
    w->setWindowTitle(QStringLiteral("demo_bar [%1|%2]")
                      .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                      .arg(polar ? "polar" : "cartesian"));
    w->resize(720, 540);
    scheduleSummary2D(layer, backend);
    return w;
}

// ===== 3D 曲面：z = 1.5·sin(x)·cos(y) 的网格（纯色剪影，方案 A）=====
QWidget* buildDemoSurface3D(DemoBackend backend)
{
    static QCartesianProjection3D s_cart;

    auto* w3 = new QChartWidget3D;
    w3->setProjection3D(&s_cart);
    w3->setGridMode3D(gridModeFromParam());
    w3->setDataBounds3D(QCube(QVector3D(-3, -3, -2), QVector3D(3, 3, 2)));

    auto* surface = new QChartSurfaceSeries(QStringLiteral("surface"), w3);
    surface->setColor(QColor("#1E88E5"));

    const int R = 41, C = 41;
    QVector<QDataPoint3D> pts;
    pts.reserve(R * C);
    for (int r = 0; r < R; ++r) {
        const qreal y = -3.0 + 6.0 * r / (R - 1);
        for (int c = 0; c < C; ++c) {
            const qreal x = -3.0 + 6.0 * c / (C - 1);
            pts.append(QDataPoint3D(QVariant(x), QVariant(y),
                                    QVariant(1.5 * std::sin(x) * std::cos(y))));
        }
    }
    surface->setGrid(R, C, pts);
    w3->layer3D()->addSeries3D(surface);

    applyBackend(w3, backend);
    w3->setWindowTitle(QStringLiteral("demo_surface3d [%1|%2]")
                       .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                       .arg(gridModeKey(w3->gridMode3D())));
    w3->resize(720, 540);
    return w3;
}

// ===== 3D：三轴 + 三网格模式（box/faceline/lattice，批次2 B）+ 域盒 + 初始姿态 =====
QWidget* buildDemoAxis3D(DemoBackend backend)
{
    const Proj3D projKind = projection3DFromParam();

    // 投影实例须覆盖窗口生命周期（demo 进程单次运行，静态持有即可）
    static QCartesianProjection3D s_cart;
    static QSphericalProjection3D s_sph;
    static QCylindricalProjection3D s_cyl;
    const QChartProjection3D* proj =
        (projKind == Proj3D::Spherical)     ? static_cast<const QChartProjection3D*>(&s_sph)
      : (projKind == Proj3D::Cylindrical)   ? static_cast<const QChartProjection3D*>(&s_cyl)
      :                                       static_cast<const QChartProjection3D*>(&s_cart);

    const QChartLayer3D::GridMode mode = gridModeFromParam();

    auto* w3 = new QChartWidget3D;
    w3->setProjection3D(proj);
    w3->setGridMode3D(mode);                                        // 转发 layer3D（Box 仅直角投影，否则回退）

    // 各坐标系按 Numeric 语义设域盒（球：(r,θ,φ)；柱：(r,θ,z)）
    QCube domain;
    switch (projKind) {
    case Proj3D::Spherical:   domain = QCube(QVector3D(0, 0, -90), QVector3D(2, 360, 90)); break;
    case Proj3D::Cylindrical: domain = QCube(QVector3D(0, 0, -2),  QVector3D(2, 360, 2));  break;
    case Proj3D::Cartesian:   domain = QCube(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));  break;
    }
    w3->setDataBounds3D(domain);

    // 球/柱坐标：θ 轴显式 45° 间隔（niceStep 不整除 360 → 环不闭合、接缝错位；角度轴需要能整除的步长）
    if (projKind != Proj3D::Cartesian) {
        if (auto* a = dynamic_cast<QValueAxis*>(w3->axisY3D()))
            a->setTickInterval(45.0);
    }

    // 相机初始姿态（fitWorld 内已置 yaw=45/pitch=30；此处再显式设定一次演示意图）
    if (QChartCamera3D* cam = w3->camera3D()) {
        cam->setYaw(45.0);
        cam->setPitch(30.0);
    }
    scheduleModeShot(w3, mode, backend);

    applyBackend(w3, backend);
    w3->setWindowTitle(QStringLiteral("demo_axis3d [%1|%2|%3]")
                       .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                       .arg(gridModeKey(mode))
                       .arg(projection3DKey(projKind)));
    w3->resize(720, 540);
    return w3;
}
