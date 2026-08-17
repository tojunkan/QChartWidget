// demo_surface3d.cpp —— 参数曲面（球面/莫比乌斯按键切换）+ 双 Widget 联动（§9）
// 左：QChartWidget3D 参数曲面（QChartSurfaceSeries 64×64 网格，FunctionalProjection3D 球面/莫比乌斯）
//     + 辅助网格地板 + 3D 高亮标记（QChartScatterSeries3D 单点，只收不发）；
// 右：QChartWidget 平面 (u,v) 域（同一 axisU/axisV 范围 + 参考格点 + 2D 高亮标记，只收不发）。
// 联动：左悬停/点选 → uvHovered/uvSelected/uvHoveredEnd → 右 2D 标记；右悬停/点选 → 左 3D 标记
// （2D 侧经 pixelToCartesian 直连，实现细节按 §9.2「留给 demo」）。
// 按键：'S' 球面、'M' 莫比乌斯（v 域统一 [-90,90] → 带内 [-0.5,0.5]，联动域一致）。
// 交互：左键 orbit / 滚轮 dolly / 右键 pan。
#include "demos.h"
#include "../../QChartWidget.h"
#include "../../QChartWidget3D.h"
#include "../../QChartLayer.h"
#include "../../QChartLayer3D.h"
#include "../../QChartSurfaceSeries.h"
#include "../../QChartScatterSeries3D.h"
#include "../../QChartFunctionalProjection3D.h"
#include "../../QCartesianProjection.h"
#include "../../QScatterSeries.h"
#include "../../QValueAxis.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtMath>
#include <QDebug>

namespace {

// ===== 投影工厂（球面 / 莫比乌斯；v 域统一 [-90,90]，联动域一致）=====
std::unique_ptr<QChartFunctionalProjection3D> makeSphereProjection() {
    return std::make_unique<QChartFunctionalProjection3D>(
        [](qreal u, qreal v, qreal /*n2*/) -> QVector3D {   // §5.3 球面
            const qreal R = 1.0;
            const qreal uRad = qDegreesToRadians(u);
            const qreal vRad = qDegreesToRadians(v);
            return QVector3D(R * qCos(vRad) * qCos(uRad),
                             R * qCos(vRad) * qSin(uRad),
                             R * qSin(vRad));
        },
        nullptr,
        QVector3D(0, -90, 0), QVector3D(360, 90, 0),
        nullptr, "u", "v", "w");
}

std::unique_ptr<QChartFunctionalProjection3D> makeMobiusProjection() {
    return std::make_unique<QChartFunctionalProjection3D>(
        [](qreal u, qreal v, qreal /*n2*/) -> QVector3D {   // §5.3 莫比乌斯，v 域 [-90,90]→[-0.5,0.5]
            const qreal R = 1.0;
            const qreal vv = v / 180.0;                       // 统一域映射
            const qreal uRad = qDegreesToRadians(u);
            const qreal half = uRad * 0.5;                    // 半扭转
            return QVector3D((R + vv * qCos(half)) * qCos(uRad),
                             (R + vv * qCos(half)) * qSin(uRad),
                             vv * qSin(half));
        },
        nullptr,
        QVector3D(0, -90, 0), QVector3D(360, 90, 0),
        nullptr, "u", "v", "w");
}

// ===== 右：2D 平面（悬停/点选 → 3D 标记；无 Q_OBJECT，仅重写虚函数）=====
class PlaneLinkWidget : public QChartWidget {
public:
    QChartScatterSeries3D* marker3D = nullptr;   // 3D 侧高亮标记（只收不发）
    QChartWidget3D* w3 = nullptr;
protected:
    void mouseMoveEvent(QMouseEvent* e) override {
        QChartWidget::mouseMoveEvent(e);
        sendUV(e->position());
    }
    void mousePressEvent(QMouseEvent* e) override {
        QChartWidget::mousePressEvent(e);
        sendUV(e->position());   // 点选 → 持久高亮
    }
private:
    void sendUV(const QPointF& pixel) {
        if (!marker3D || !w3) return;
        QPointF uv = pixelToCartesian(pixel);
        uv.setX(qBound<qreal>(0.0, uv.x(), 360.0));
        uv.setY(qBound<qreal>(-90.0, uv.y(), 90.0));
        marker3D->clear();
        marker3D->append(QDataPoint3D(uv.x(), uv.y(), 0.0));
        w3->invalidateForeground();
        w3->update();
    }
};

// ===== 左：3D 按键过滤 'S'/'M'（无 Q_OBJECT）=====
class SurfaceKeyFilter : public QObject {
public:
    QChartWidget3D* w = nullptr;
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() == Qt::Key_S) {
                w->setProjection3D(makeSphereProjection());
                qDebug() << "曲面切换：球面";
                return true;
            }
            if (ke->key() == Qt::Key_M) {
                w->setProjection3D(makeMobiusProjection());
                qDebug() << "曲面切换：莫比乌斯环";
                return true;
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};

} // namespace

QWidget* buildDemoSurface3D() {
    qDebug() << "\n========== 参数曲面 + 双 Widget 联动 ==========";

    // ── 左：3D 曲面 ──
    auto* w3 = new QChartWidget3D();
    w3->setWindowTitle("3D 曲面 - 按键 S/M 切换球面/莫比乌斯；联动悬停");
    w3->setFocusPolicy(Qt::StrongFocus);
    w3->setObjectName("widget3d");

    auto* filter = new SurfaceKeyFilter();
    filter->setParent(w3);
    filter->w = w3;
    w3->installEventFilter(filter);

    auto* layer3 = new QChartLayer3D(w3);
    layer3->setGridFloorVisible(true);
    layer3->setGridFloorHalfSize(1.6);   // 略大于球半径 1

    auto* surface = new QChartSurfaceSeries("曲面", layer3);
    surface->setParametricGrid(64, 64, 0, 360, -90, 90);   // (u,v) 网格
    surface->setColor(QColor("#1E88E5"));
    layer3->addSeries3D(surface);

    auto* marker3 = new QChartScatterSeries3D("高亮(3D)", layer3);   // 只收不发
    marker3->setMarkerSize(8.0);
    marker3->setColor(QColor("#FFEB3B"));
    marker3->setObjectName("marker3d");
    layer3->addSeries3D(marker3);

    w3->addLayer3D(layer3);
    w3->setProjection3D(makeSphereProjection());   // 默认球面 + 自动 fit

    // ── 右：2D (u,v) 平面 ──
    auto* w2 = new PlaneLinkWidget();
    w2->setWindowTitle("2D (u,v) 平面 - 联动悬停");
    w2->setObjectName("plane");
    w2->setProjection(std::make_unique<QCartesianProjection>());

    auto* axisU = new QValueAxis(w2, Qt::AlignBottom);
    auto* axisV = new QValueAxis(w2, Qt::AlignLeft);
    axisU->setRange(0, 360);
    axisV->setRange(-90, 90);
    w2->addAxis(axisU);
    w2->addAxis(axisV);

    auto* layer2 = new QChartLayer(w2);
    layer2->setAxisX(axisU);
    layer2->setAxisY(axisV);
    w2->addLayer(layer2);

    auto* ref = new QScatterSeries("(u,v) 域", layer2);   // 参考格点
    ref->setMarkerSize(2.0);
    ref->setColor(QColor(180, 180, 180));
    for (qreal u = 0.0; u <= 360.0; u += 30.0)
        for (qreal v = -90.0; v <= 90.0; v += 30.0)
            ref->append(u, v);
    layer2->addSeries(ref);

    auto* marker2 = new QScatterSeries("高亮(2D)", layer2);   // 只收不发
    marker2->setMarkerSize(8.0);
    marker2->setColor(QColor("#FFEB3B"));
    marker2->setVisible(false);
    marker2->setObjectName("marker2d");
    layer2->addSeries(marker2);

    // ── 联动接线：3D → 2D（信号互发，§9.2）──
    auto updateMarker2 = [w2, marker2](qreal u, qreal v) {
        marker2->clear();
        marker2->append(u, v);
        marker2->setVisible(true);
        w2->invalidateForeground();
        w2->update();
    };
    QObject::connect(w3, &QChartWidget3D::uvHovered,   w2, updateMarker2);
    QObject::connect(w3, &QChartWidget3D::uvSelected,  w2, updateMarker2);
    QObject::connect(w3, &QChartWidget3D::uvHoveredEnd, w2, [w2, marker2]() {
        marker2->setVisible(false);
        w2->invalidateForeground();
        w2->update();
    });

    // 2D → 3D：PlaneLinkWidget 直连（§9.2「实现细节留给 demo」）；标记只收不发防回环
    w2->marker3D = marker3;
    w2->w3 = w3;

    // ── 双图容器（单窗口双图，联动可视）──
    auto* container = new QWidget();
    container->setWindowTitle("双 Widget 联动：3D 球面 ↔ 2D (u,v) 平面");
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(w3);
    layout->addWidget(w2);
    w3->resize(480, 480);
    w2->resize(400, 480);
    return container;
}
