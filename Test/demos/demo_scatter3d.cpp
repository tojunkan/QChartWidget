// demo_scatter3d.cpp —— 3D 散点：球面随机采样 + 柱面/球面投影切换（静态）
// 数据 = (r=1, θ, φ) 三元组：球面投影 → 单位球上的点；柱面投影 → 单位圆柱上的点
// （同一数据两种投影演示）。交互：左键 orbit / 滚轮 dolly（R6 无平移手势）。
// 按键：'S' 球面投影、'C' 柱面投影、'A' 轴/网格开关（§12 建议同步，统一观感）。
#include "demos.h"
#include "QChartWidget3D.h"
#include "QChartLayer3D.h"
#include "QChartScatterSeries3D.h"
#include "QChartSphericalProjection3D.h"
#include "QChartCylindricalProjection3D.h"
#include "QValueAxis.h"
#include <QRandomGenerator>
#include <QKeyEvent>
#include <QtMath>
#include <QDebug>

namespace {

// 按键过滤：'S'/'C' 切换投影、'A' 轴/网格开关（无 Q_OBJECT，纯虚函数重写）
class ScatterKeyFilter : public QObject {
public:
    QChartWidget3D* w = nullptr;
    QChartLayer3D* layer = nullptr;
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() == Qt::Key_C) {
                w->setProjection3D(std::make_unique<QChartCylindricalProjection3D>());
                qDebug() << "投影切换：柱面";
                return true;
            }
            if (ke->key() == Qt::Key_S) {
                w->setProjection3D(std::make_unique<QChartSphericalProjection3D>());
                qDebug() << "投影切换：球面";
                return true;
            }
            if (ke->key() == Qt::Key_A) {
                if (layer) {
                    layer->axes3D()->setVisible(!layer->axes3D()->visible());
                    qDebug() << "轴/网格开关:" << (layer->axes3D()->visible() ? "开" : "关");
                    w->invalidateForeground();
                    w->update();
                }
                return true;
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};

} // namespace

QWidget* buildDemoScatter3D() {
    qDebug() << "\n========== 3D 散点（球面采样 / 投影切换）==========";

    auto* w = new QChartWidget3D();
    w->setRenderBackend(QChartWidget3D::RenderBackend::OpenGL);   // §12：默认 GL（QCHART_GL=0 由 Widget 兜底回退 QPainter）
    w->setWindowTitle("3D 散点 - 按键 S/C 切换球面/柱面投影，A 轴开关");
    w->setFocusPolicy(Qt::StrongFocus);
    w->resize(560, 520);
    w->setObjectName("widget3d");

    auto* filter = new ScatterKeyFilter();
    filter->setParent(w);
    filter->w = w;
    w->installEventFilter(filter);

    // 数据：球面随机采样点 (r=1, θ∈[0,360), φ∈[-90,90])
    auto* layer = new QChartLayer3D(w);
    // §12 建议同步：轴绑定（tickCount 3；球/柱均有反向 → 视图驱动路径）
    auto* axisR = new QValueAxis(w, Qt::AlignBottom);
    auto* axisTh = new QValueAxis(w, Qt::AlignLeft);
    auto* axisPh = new QValueAxis(w, Qt::AlignBottom);
    axisR->setTickCount(3);
    axisTh->setTickCount(3);
    axisPh->setTickCount(3);
    axisR->setColor(QColor("#E53935"));
    axisTh->setColor(QColor("#43A047"));
    axisPh->setColor(QColor("#FF9800"));
    layer->setAxisX(axisR);
    layer->setAxisY(axisTh);
    layer->setAxisZ(axisPh);
    filter->layer = layer;

    auto* scatter = new QChartScatterSeries3D("球面采样点", layer);
    scatter->setColor(QColor("#FF9800"));
    scatter->setMarkerSize(4.0);
    auto* rng = QRandomGenerator::global();
    const int n = 400;
    for (int i = 0; i < n; ++i) {
        const qreal u = rng->generateDouble() * 360.0;                     // θ
        const qreal v = qRadiansToDegrees(qAsin(2.0 * rng->generateDouble() - 1.0)); // φ（球面均匀）
        scatter->append(QDataPoint3D(1.0, u, v));
    }
    layer->addSeries3D(scatter);
    w->addLayer3D(layer);

    w->setProjection3D(std::make_unique<QChartSphericalProjection3D>());   // 默认球面 + 自动 fit
    return w;
}
