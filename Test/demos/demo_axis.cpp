// demo_axis.cpp —— 批次 B4：轴阶段演示（2D 轴 + 3D 轴，CPU/GL 可切）
#include "demos.h"
#include "QChartWidget.h"
#include "QChartWidget3D.h"
#include "QChartLayer.h"
#include "QChartCamera3D.h"
#include "QValueAxis.h"
#include "QCartesianProjection3D.h"
#include <QWidget>

static void applyBackend(QChartAbstractWidget* w, DemoBackend b)
{
    w->setRenderBackend(b == DemoBackend::Gl
        ? QChartAbstractWidget::RenderBackend::OpenGL
        : QChartAbstractWidget::RenderBackend::QPainter);
}

// ===== 2D：Cartesian value×value + 网格 + 边框轴 + 标签 =====
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
    return w;
}

// ===== 3D：三轴 + Lattice（可切 Box）+ 域盒 + 初始姿态 =====
QWidget* buildDemoAxis3D(DemoBackend backend)
{
    // 投影实例须覆盖窗口生命周期（demo 进程单次运行，静态持有即可）
    static QCartesianProjection3D s_proj3;

    auto* w3 = new QChartWidget3D;
    w3->setProjection3D(&s_proj3);
    w3->layer3D()->setGridMode(QChartLayer3D::GridMode::Lattice);   // 视觉丰富；可注释切 Box
    w3->setDomainBox(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));    // → layer3D.setDataBounds + fitWorld
    // 相机初始姿态（fitWorld 内已置 yaw=45/pitch=30；此处再显式设定一次演示意图）
    if (QChartCamera3D* cam = w3->camera3D()) {
        cam->setYaw(45.0);
        cam->setPitch(30.0);
    }

    applyBackend(w3, backend);
    w3->setWindowTitle(QStringLiteral("demo_axis3d [%1]")
                       .arg(backend == DemoBackend::Gl ? "gl" : "cpu"));
    w3->resize(720, 540);
    return w3;
}
