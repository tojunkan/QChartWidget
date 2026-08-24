// demo_camera.cpp —— 相机漫游（QViewRectAnimation）
#include "demos.h"
#include "QChartWidget.h"
#include "QChartProjectionFactory.h"
#include "QScatterSeries.h"
#include "QValueAxis.h"
#include "QViewRectAnimation.h"
#include <QSequentialAnimationGroup>
#include <QRandomGenerator>
#include <QtMath>
#include <QDebug>

QWidget* buildDemoCamera() {
    qDebug() << "\n========== 相机漫游 ==========";

    auto* w = new QChartWidget();
    w->setWindowTitle("相机漫游 - waypoint + sizeCurve");
    w->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    w->setViewRectFitMode(ViewRectFitMode::Crop);

    auto* xAxis = new QValueAxis(w, Qt::AlignBottom);
    auto* yAxis = new QValueAxis(w, Qt::AlignLeft);
    w->addAxis(xAxis);
    w->addAxis(yAxis);
    xAxis->setRange(-10, 10);
    yAxis->setRange(-10, 10);

    auto* layer = new QChartLayer(w);
    layer->setAxisX(xAxis);
    layer->setAxisY(yAxis);
    w->addLayer(layer);

    auto* scatter = new QScatterSeries("points", layer);
    scatter->setColor(QColor("#FF9800"));
    scatter->setMarkerSize(6);
    for (int i = 0; i < 50; ++i) {
        qreal x = (QRandomGenerator::global()->generateDouble() - 0.5) * 20.0;
        qreal y = (QRandomGenerator::global()->generateDouble() - 0.5) * 20.0;
        scatter->append(x, y);
    }
    layer->addSeries(scatter);

    // 直推动画：全景 → 中心区域
    auto* push = new QViewRectAnimation(w);
    push->setDuration(3000);
    push->setEasingCurve(QEasingCurve::InOutCubic);
    push->setTargetWidget(w);
    push->setTargetViewRect(QRectF(0, 0, 4, 4));

    // 弧线动画：同样终点，但 waypoint 绕远 + sizeCurve 中途放大
    auto* arc = new QViewRectAnimation(w);
    arc->setDuration(3500);
    arc->setEasingCurve(QEasingCurve::InOutCubic);
    arc->setTargetWidget(w);
    arc->setTargetViewRect(QRectF(-2, -2, 3, 3)); // 换一个目标区域
    arc->setWaypoint(QPointF(5, -5));  // 相机绕道右下角
    arc->setSizeCurve([](qreal a) -> qreal {
        qreal midSize = 4.0 + std::sin(a * M_PI) * 14.0; // α=0.5 时 width≈18，拉远示全景
        return midSize;
    });

    // 串行：先直推，再弧线
    auto* seq = new QSequentialAnimationGroup(w);
    seq->addAnimation(push);
    seq->addAnimation(arc);
    QObject::connect(seq, &QSequentialAnimationGroup::finished, w, []() {
        qDebug() << "相机漫游完成";
    });
    seq->start();

    w->resize(500, 500);
    return w;
}
