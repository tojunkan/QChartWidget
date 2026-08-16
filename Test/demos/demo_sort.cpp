// demo_sort.cpp —— 冒泡排序动画（QBarAnimation Generator 模式）
#include "demos.h"
#include "../../QChartWidget.h"
#include "../../QChartProjectionFactory.h"
#include "../../QBarSeries.h"
#include "../../QValueAxis.h"
#include "../../QBarCategoryAxis.h"
#include "../../QDataPoint.h"
#include "../../QBarAnimation.h"
#include <QSequentialAnimationGroup>
#include <QDebug>

QChartWidget* buildDemoSort() {
    qDebug() << "\n========== 冒泡排序动画 ==========";

    auto* w = new QChartWidget();
    w->setWindowTitle("冒泡排序 - QBarAnimation 交换动画");
    w->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    w->setViewRectFitMode(ViewRectFitMode::Stretch);

    auto* xAxis = new QBarCategoryAxis(w, Qt::AlignBottom);
    w->addAxis(xAxis);

    auto* yAxis = new QValueAxis(w, Qt::AlignLeft);
    w->addAxis(yAxis);
    yAxis->setRange(0, 10);

    auto* layer = new QChartLayer(w);
    layer->setAxisX(xAxis);
    layer->setAxisY(yAxis);
    w->addLayer(layer);

    auto* bars = new QBarSeries("bars", layer);
    bars->setColor(QColor("#4CAF50"));
    bars->setFillColor(QColor(76, 175, 80, 200));

    // 初始乱序高度（Numeric 空间 x∈[-0.5, n-0.5]，y∈[0, 10]）
    QVector<qreal> heights = { 3, 7, 1, 9, 4, 8, 2, 6, 5 };
    const int N = heights.size();
    xAxis->setCategories(QStringList{"a","b","c","d","e","f","g","h","i"});
    xAxis->setNumericMapping(-0.5, N - 0.5);

    for (int i = 0; i < N; ++i)
        bars->append(i - 0.4, 0, i + 0.4, heights[i]);
    layer->addSeries(bars);

    // 冒泡排序：每步一个交换动画（QBarAnimation 模式 B Generator）
    // 注意：动画期间 Series 真实数据不修改——算法侧维护 working 高度数组，
    // 每步动画从"交换前快照"lerp 到"交换后目标"；动画结束后一次性落地真实数据
    auto* seq = new QSequentialAnimationGroup(w);

    QVector<qreal> working = heights;   // 算法侧跟踪的当前高度（真实数据不动）
    bool swapped = true;
    int pass = 0;
    while (swapped && pass < N - 1) {
        swapped = false;
        for (int j = 0; j < N - 1 - pass; ++j) {
            if (working[j] > working[j + 1]) {
                // 交换前先快照 src（逐索引 lerp：只有 j/j+1 两柱会动）
                QVector<QRectF> src, dst;
                for (int k = 0; k < N; ++k)
                    src << QRectF(k - 0.4, 0, 0.8, working[k]);

                std::swap(working[j], working[j + 1]);
                swapped = true;

                for (int k = 0; k < N; ++k)
                    dst << QRectF(k - 0.4, 0, 0.8, working[k]);

                // 每个交换 = 一个自含 Generator 的动画（src→dst 逐矩形 lerp）
                auto* anim = new QBarAnimation(seq);
                anim->setDuration(280);
                anim->setEasingCurve(QEasingCurve::InOutQuad);
                anim->setTargetSeries(bars);
                anim->setGenerator([src, dst](qreal alpha, QVector<QRectF>& out) {
                    int n = qMin(src.size(), dst.size());
                    out.resize(n);
                    for (int k = 0; k < n; ++k) {
                        const QRectF& a = src[k];
                        const QRectF& b = dst[k];
                        out[k] = QRectF(a.left()   + (b.left()   - a.left())   * alpha,
                                        a.top()    + (b.top()    - a.top())    * alpha,
                                        a.width()  + (b.width()  - a.width())  * alpha,
                                        a.height() + (b.height() - a.height()) * alpha);
                    }
                });
                seq->addAnimation(anim);
            }
        }
        ++pass;
    }

    // 动画结束：数据落地 + 清除覆盖层
    QObject::connect(seq, &QSequentialAnimationGroup::finished, w, [w, bars, working]() {
        // 真实数据更新为排序结果（之前动画期间从未动过）
        for (int k = 0; k < working.size(); ++k)
            bars->replace(k, QDataRect(k - 0.4, 0, k + 0.4, working[k]));
        bars->clearRenderOverride();
        qDebug() << "排序完成，数据已落地，覆盖层已清除";
    });

    seq->start();

    w->resize(600, 400);
    return w;
}
