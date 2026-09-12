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
#include "QCartesianProjection3D.h"
#include "QOpenGLWidget"
#include <QWidget>
#include <QApplication>
#include <QSet>
#include <cmath>
#include <memory>
#include <QTimer>
#include <QPixmap>
#include <QDebug>

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

/// 出图取证摘要（批次2 A/B + t31）：二维网格脊数 vs 自由标签数；按后端标注渲染预期
///   CPU：自由标签可渲染 → 每线一个数值标签（脊数==标签数）
///   GL ：既定契约 = 纯 GPU 后端不渲染自由标签 → 网格线标签在本后端不显示（已知差异）
void logSceneSummary2D(QChartLayer* layer, DemoBackend backend)
{
    if (!layer) return;
    const QChartScene& sc = layer->scene();
    QSet<int> groups;
    int freeLabels = 0;
    for (const QChartPrimitive& p : sc.primitives)
        if (p.sourceId >= 0) groups.insert(p.sourceId);
    for (const QChartTextLabel& l : sc.labels)
        if (l.refPrimitiveId == -1 && std::isnan(l.numericAnchor.x())) ++freeLabels;
    const bool gl = (backend == DemoBackend::Gl);
    qInfo().noquote() << QString("shot-summary axis[%1]: gridSpines=%2 labels=%3 freeLabels=%4 (%5)")
                             .arg(gl ? "gl" : "cpu")
                             .arg(groups.size()).arg(sc.labels.size()).arg(freeLabels)
                             .arg(gl ? QStringLiteral("GL 不渲染自由标签：网格线标签在本后端不显示（既定契约）")
                                     : (groups.size() == sc.labels.size()
                                            ? QStringLiteral("每线一标签 OK")
                                            : QStringLiteral("MISMATCH")));
}

void logSceneSummary3D(QChartLayer3D* layer, QChartLayer3D::GridMode mode)
{
    if (!layer) return;
    const QChartScene& sc = layer->scene3D();
    qInfo().noquote() << QString("shot-summary axis3d[%1]: prims=%2 labels=%3")
                             .arg(gridModeKey(mode)).arg(sc.primitives.size()).arg(sc.labels.size());
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
        const bool ready = !layer->scene().primitives.isEmpty();
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
        const bool ready = w3->layer3D() && !w3->layer3D()->scene3D().primitives.isEmpty();
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

// ===== 3D：三轴 + 三网格模式（box/faceline/lattice，批次2 B）+ 域盒 + 初始姿态 =====
QWidget* buildDemoAxis3D(DemoBackend backend)
{
    // 投影实例须覆盖窗口生命周期（demo 进程单次运行，静态持有即可）
    static QCartesianProjection3D s_proj3;

    const QChartLayer3D::GridMode mode = gridModeFromParam();

    auto* w3 = new QChartWidget3D;
    w3->setProjection3D(&s_proj3);
    w3->setGridMode3D(mode);                                        // 转发 layer3D（Box 仅直角投影，否则回退）
    w3->setDomainBox(QCube(QVector3D(-3, -3, -3), QVector3D(3, 3, 3)));   // 4c：QCube 域盒 → layer3D + fitWorld
    // 相机初始姿态（fitWorld 内已置 yaw=45/pitch=30；此处再显式设定一次演示意图）
    if (QChartCamera3D* cam = w3->camera3D()) {
        cam->setYaw(45.0);
        cam->setPitch(30.0);
    }
    scheduleModeShot(w3, mode, backend);

    applyBackend(w3, backend);
    w3->setWindowTitle(QStringLiteral("demo_axis3d [%1|%2]")
                       .arg(backend == DemoBackend::Gl ? "gl" : "cpu")
                       .arg(gridModeKey(mode)));
    w3->resize(720, 540);
    return w3;
}
