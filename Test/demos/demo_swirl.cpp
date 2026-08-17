// demo_swirl.cpp —— 投影切换动画（恒等 ↔ Swirl 涡流）
#include "demos.h"
#include "../../QChartWidget.h"
#include "../../QChartProjectionFactory.h"
#include "../../ProjectionToolKit.h"
#include "../../QScatterSeries.h"
#include "../../QValueAxis.h"
#include "../../QProjectionSwitchAnimation.h"
#include <QTimer>
#include <QDebug>

QWidget* buildDemoSwirl() {
    qDebug() << "\n========== 投影切换（恒等 ↔ Swirl）==========";

    auto* w = new QChartWidget();
    w->setWindowTitle("投影切换 - 恒等 ↔ Swirl 涡流");
    w->setProjection(createIdentityProjection()); // 起点：恒等
    w->setViewRectFitMode(ViewRectFitMode::Crop);

    auto* xAxis = new QValueAxis(w, Qt::AlignBottom);
    auto* yAxis = new QValueAxis(w, Qt::AlignLeft);
    w->addAxis(xAxis);
    w->addAxis(yAxis);
    xAxis->setRange(-4, 4);
    yAxis->setRange(-4, 4);

    auto* layer = new QChartLayer(w);
    layer->setAxisX(xAxis);
    layer->setAxisY(yAxis);
    w->addLayer(layer);

    // 网格点阵：切换时最能看出扭曲
    auto* grid = new QScatterSeries("grid", layer);
    grid->setColor(QColor("#00BCD4"));
    grid->setMarkerSize(5);
    for (qreal x = -3.5; x <= 3.5 + 1e-6; x += 1.0)
        for (qreal y = -3.5; y <= 3.5 + 1e-6; y += 1.0)
            grid->append(x, y);
    layer->addSeries(grid);

    // 计时器反复切换恒等 ↔ Swirl（涡流旋转扭曲最直观）
    // 状态放窗口 property——不能用栈局部变量捕获引用：buildDemoSwirl 返回后栈帧销毁，
    // timer lambda 里的悬空引用会导致第二次切换读到垃圾值（表现为只动一次）
    w->setProperty("toSwirl", true);
    auto* timer = new QTimer(w);
    QObject::connect(timer, &QTimer::timeout, w, [w]() {
        bool toSwirl = w->property("toSwirl").toBool();
        auto* anim = new QProjectionSwitchAnimation(w);
        anim->setDuration(2500);
        anim->setEasingCurve(QEasingCurve::InOutCubic);
        anim->setTargetWidget(w);
        anim->setTargetProjection(toSwirl
            ? createSwirlProjection(1.0, 4.0)   // 强涡流
            : QChartProjectionFactory::create(CoordinateSystem::Cartesian));       // 回到恒等
        anim->start();
        // 动画期间禁交互，结束后恢复
        w->setPanEnabled(false);
        w->setZoomEnabled(false);
        QObject::connect(anim, &QProjectionSwitchAnimation::finished, w, [w]() {
            w->setPanEnabled(true);
            w->setZoomEnabled(true);
            w->setProperty("toSwirl", !w->property("toSwirl").toBool());
        });
    });
    timer->start(3500);

    w->resize(500, 500);
    return w;
}
