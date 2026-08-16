// demo_stress.cpp —— 折线粗筛压力测量（1M 点缩放到局部，绝大多数屏外）
// 用途：验证 QLineSeries 像素空间可见性裁剪的收益与正确性。
//   开 chart.series.verbose=true 可看 culled 计数占比；
//   帧时打印在日志里（关闭 verbose 复测以排除日志开销）。
#include "demos.h"
#include "../../QChartWidget.h"
#include "../../QChartProjectionFactory.h"
#include "../../QLineSeries.h"
#include "../../QValueAxis.h"
#include "../../QDataPoint.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QVector>
#include <QtMath>
#include <QDebug>

QChartWidget* buildDemoStress() {
    qDebug() << "\n========== 折线粗筛压力测量 ==========";

    const int N = 1000000;   // 1M 点

    auto* w = new QChartWidget();
    w->setWindowTitle("折线粗筛压力 - 1M 点缩放到局部");
    w->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    w->setViewRectFitMode(ViewRectFitMode::Crop);

    auto* xAxis = new QValueAxis(w, Qt::AlignBottom);
    auto* yAxis = new QValueAxis(w, Qt::AlignLeft);
    xAxis->setColor(Qt::white);
    yAxis->setColor(Qt::white);
    w->addAxis(xAxis);
    w->addAxis(yAxis);
    xAxis->setRange(0, N);
    yAxis->setRange(-1.5, 1.5);

    auto* layer = new QChartLayer(w);
    layer->setAxisX(xAxis);
    layer->setAxisY(yAxis);
    w->addLayer(layer);

    auto* line = new QLineSeries("stress", layer);
    line->setColor(QColor("#2196F3"));
    line->setLineWidth(1.0);
    QVector<QDataPoint> pts;
    pts.reserve(N);
    for (int i = 0; i < N; ++i) {
        qreal x = static_cast<qreal>(i);
        qreal y = std::sin(x * 0.01);   // 波长约 628 点
        pts.append(QDataPoint(QVariant::fromValue(x), QVariant::fromValue(y)));
    }
    line->setPoints(pts);
    layer->addSeries(line);

    w->resize(900, 600);

    // 事件循环跑起来后：缩放到 x∈[500000,501000]，约 1000 点可见、99.9% 屏外
    QTimer::singleShot(100, w, [w]() {
        w->setViewRect(QRectF(500000, -1.5, 1000, 3.0));

        const int ITERS = 20;
        w->grab();   // 预热：建缓存

        QElapsedTimer t;
        t.start();
        for (int i = 0; i < ITERS; ++i) {
            w->invalidateForeground();   // 强制重建前景缓存（含折线）
            w->grab();
        }
        qint64 ms = t.elapsed();
        qDebug() << "[stress] N=" << N << " visible≈1000, iters=" << ITERS
                 << ", total=" << ms << "ms"
                 << ", avg=" << (ms / double(ITERS)) << "ms/frame"
                 << "（开 chart.series.verbose=true 看 culled 计数）";
    });

    return w;
}
