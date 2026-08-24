// demo_pendulum.cpp —— 单摆动画（QNumericSeriesAnimation Generator 模式）
#include "demos.h"
#include "QChartWidget.h"
#include "QChartProjectionFactory.h"
#include "QScatterSeries.h"
#include "QLineSeries.h"
#include "QValueAxis.h"
#include "QNumericSeriesAnimation.h"
#include <QParallelAnimationGroup>
#include <QtMath>
#include <QDebug>

QWidget* buildDemoPendulum() {
    qDebug() << "\n========== 单摆动画 ==========";

    auto* w = new QChartWidget();
    w->setWindowTitle("单摆动画 - Generator 模式");
    w->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    w->setViewRectFitMode(ViewRectFitMode::Crop); // 物理模拟不能变形

    auto* xAxis = new QValueAxis(w, Qt::AlignBottom);
    w->addAxis(xAxis);
    xAxis->setRange(-5, 5);

    auto* yAxis = new QValueAxis(w, Qt::AlignLeft);
    w->addAxis(yAxis);
    yAxis->setRange(-5, 1);

    auto* layer = new QChartLayer(w);
    layer->setAxisX(xAxis);
    layer->setAxisY(yAxis);
    w->addLayer(layer);

    // 物理参数：L=4, g=9.8, θ₀=18°，动画跑两个周期
    const qreal L = 4.0, g = 9.8, theta0 = M_PI / 10.0;
    const qreal T = 2 * M_PI * std::sqrt(L / g);   // 周期 ≈4.01s
    const int durationMs = qRound(T * 2 * 1000);   // ≈8024ms

    // 杆：悬挂点 (0,0) → 球心；球：单点散点
    auto* rod = new QLineSeries("rod", layer);
    rod->setColor(QColor("#2196F3"));
    layer->addSeries(rod);

    auto* ball = new QScatterSeries("ball", layer);
    ball->setColor(QColor("#F44336"));
    ball->setMarkerSize(14);
    layer->addSeries(ball);

    // 共享物理核：θ(t) = θ₀·cos(√(g/L)·t)，t = alpha·两周期
    // 两个动画各自包装 Generator——输出点集形状不同（杆 2 点、球 1 点）
    auto thetaAt = [L, g, theta0, T](qreal alpha) -> qreal {
        qreal t = alpha * 2.0 * T;
        return theta0 * std::cos(std::sqrt(g / L) * t);
    };

    QNumericSeriesAnimation::Generator genRod = [thetaAt, L](qreal alpha, QVector<QPointF>& out) {
        qreal th = thetaAt(alpha);
        out = { QPointF(0, 0), QPointF(L * std::sin(th), -L * std::cos(th)) };
    };
    QNumericSeriesAnimation::Generator genBall = [thetaAt, L](qreal alpha, QVector<QPointF>& out) {
        qreal th = thetaAt(alpha);
        out = { QPointF(L * std::sin(th), -L * std::cos(th)) };
    };

    auto* rodAnim = new QNumericSeriesAnimation(w);
    rodAnim->setDuration(durationMs);
    rodAnim->setEasingCurve(QEasingCurve::Linear); // 物理时间必须线性推进
    rodAnim->setTargetSeries(rod);
    rodAnim->setGenerator(genRod);

    auto* ballAnim = new QNumericSeriesAnimation(w);
    ballAnim->setDuration(durationMs);
    ballAnim->setEasingCurve(QEasingCurve::Linear);
    ballAnim->setTargetSeries(ball);
    ballAnim->setGenerator(genBall);

    auto* group = new QParallelAnimationGroup(w);
    group->addAnimation(rodAnim);
    group->addAnimation(ballAnim);

    // 动画结束：清除覆盖层，恢复真实数据渲染
    QObject::connect(group, &QParallelAnimationGroup::finished, rod, [rod, ball]() {
        rod->clearRenderOverride();
        ball->clearRenderOverride();
        qDebug() << "单摆动画结束，覆盖层已清除";
    });

    group->start();

    w->resize(600, 600);
    return w;
}
