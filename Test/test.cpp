// test.cpp —— 五空间架构重构验证
// 分别在 Cartesian 和 Polar 投影下测试 axis + grid 绘制
// 通过 qCDebug 打印坐标位置，人工检验是否正确
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QtMath>
#include <cmath>
#include <QTextStream>
#include <QDateTime>
#include <QRandomGenerator>
#include "../QChartWidget.h"
#include "../QChartProjectionFactory.h"
#include "../QScatterSeries.h"
#include "../QLineSeries.h"
#include "../QPolygonSeries.h"
#include "../QBarSeries.h"
#include "../QValueAxis.h"
#include "../ProjectionToolKit.h"
#include "../QDateTimeAxis.h"
#include "../QBarCategoryAxis.h"
#include "../QLogAxis.h"
#include "../QDataPoint.h"
#include "../QChartAnimation.h"
#include "../QNumericSeriesAnimation.h"
#include "../QBarAnimation.h"
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>

// 将 Qt 日志同时写入文件和 stderr（Windows 下 qDebug 默认不输出到控制台）
static QFile g_logFile;
static QTextStream g_logStream;

static void logToFile(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    QString line = QString("[%1] %2\n")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
        .arg(msg);
    g_logStream << line;
    g_logStream.flush();
    // 也输出到 stderr，方便 cmd 窗口即时看到
    fprintf(stderr, "%s", qPrintable(line));
    fflush(stderr);
}

int main(int argc, char* argv[]) {
    // 重定向日志到文件
    QString logPath = "E:/Dujia/DuRunHan/Programs/cplusplus/GoodsSystem/x64/Debug/test_log.txt";
    g_logFile.setFileName(logPath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    g_logStream.setDevice(&g_logFile);
    qInstallMessageHandler(logToFile);
    // 启用所有 chart 分类的 debug 日志
    //QLoggingCategory::setFilterRules("chart.axis.debug=true");
    QLoggingCategory::setFilterRules("chart.*.verbose=false");

    QApplication app(argc, argv);
    qDebug() << "========== 测试开始 ==========";
    qDebug() << "日志文件:" << logPath;

    qDebug() << "\n========== 窗口1: Polar 五边形 ==========";

    auto* polarW = new QChartWidget();
    polarW->setWindowTitle("Polar 五边形 - 曲线边验证");
    polarW->setProjection(QChartProjectionFactory::create(CoordinateSystem::Polar));

    auto* angleAxis = new QValueAxis(polarW, Qt::AlignHCenter);
    angleAxis->setColor(Qt::white);
    polarW->addAxis(angleAxis);
    angleAxis->setLabelFormat("%g°");
    angleAxis->setRange(0, 360);

    auto* radialAxis = new QValueAxis(polarW, Qt::AlignVCenter);
    radialAxis->setColor(Qt::white);
    polarW->addAxis(radialAxis);
    radialAxis->setRange(0, 5);

    auto* polarGeo = new QChartLayer(polarW);
    polarGeo->setAxisX(angleAxis);
    polarGeo->setAxisY(radialAxis);
    polarW->addLayer(polarGeo);

    // 五边形顶点：θ=0°,72°,144°,216°,288°， r=4
    auto* penta = new QPolygonSeries("pentagon", polarGeo);
    penta->setColor(QColor("#FF5722"));
    penta->setFillColor(QColor(255, 87, 34, 80));
    for (int i = 0; i < 5; ++i) {
        qreal theta = i * 72.0;
        penta->append(theta, 4.0);
    }
    polarGeo->addSeries(penta);

    // 顶点加标记
    auto* vertexMarks = new QScatterSeries("vertices", polarGeo);
    vertexMarks->setColor(QColor("#FFC107"));
    vertexMarks->setMarkerSize(8);
    for (int i = 0; i < 5; ++i)
        vertexMarks->append(i * 72.0, 4.0);
    polarGeo->addSeries(vertexMarks);

    polarW->resize(600, 500);
    polarW->show();

    // ===== 窗口 2: Cartesian 柱状图 =====
    qDebug() << "\n========== 窗口2: Cartesian 柱状图 ==========";

    auto* barW = new QChartWidget();
    barW->setWindowTitle("Cartesian 柱状图 - drawRect 快路径");
    barW->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    barW->setViewRectFitMode(ViewRectFitMode::Stretch); // Cartesian 下直接拉伸即可

    auto* xAxis = new QBarCategoryAxis(barW, Qt::AlignBottom);
    xAxis->setColor(Qt::white);
    barW->addAxis(xAxis);
    xAxis->setCategories({"苹果","香蕉","橙子","葡萄","西瓜"});
    xAxis->setNumericMapping(-0.5, 4.5);  // 槽位居中：索引0→-0.5, 索引4→4.5

    auto* valAxis = new QValueAxis(barW, Qt::AlignLeft);
    valAxis->setColor(Qt::white);
    barW->addAxis(valAxis);
    valAxis->setRange(0, 100);

    auto* barGeo = new QChartLayer(barW);
    barGeo->setAxisX(xAxis);
    barGeo->setAxisY(valAxis);
    barW->addLayer(barGeo);

    auto* bars = new QBarSeries("bars", barGeo);
    bars->setColor(QColor("#4CAF50"));
    bars->setFillColor(QColor(76, 175, 80, 160));
    bars->append(-0.3, 0, 0.3, 45);
    bars->append(0.7, 0, 1.3, 72);
    bars->append(1.7, 0, 2.3, 33);
    bars->append(2.7, 0, 3.3, 88);
    bars->append(3.7, 0, 4.3, 56);
    barGeo->addSeries(bars);

    barW->resize(600, 400);
    barW->show();

    // ===== 窗口 3: 单摆动画（QNumericSeriesAnimation Generator 模式）=====
    qDebug() << "\n========== 窗口3: 单摆动画 ==========";

    auto* pendW = new QChartWidget();
    pendW->setWindowTitle("单摆动画 - Generator 模式");
    pendW->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    pendW->setViewRectFitMode(ViewRectFitMode::Crop);

    auto* pendX = new QValueAxis(pendW, Qt::AlignBottom);
    pendX->setColor(Qt::white);
    pendW->addAxis(pendX);
    pendX->setRange(-5, 5);

    auto* pendY = new QValueAxis(pendW, Qt::AlignLeft);
    pendY->setColor(Qt::white);
    pendW->addAxis(pendY);
    pendY->setRange(-5, 1);

    auto* pendGeo = new QChartLayer(pendW);
    pendGeo->setAxisX(pendX);
    pendGeo->setAxisY(pendY);
    pendW->addLayer(pendGeo);

    // 物理参数：L=4, g=9.8, θ₀=60°，动画跑两个周期
    const qreal L = 4.0, g = 9.8, theta0 = M_PI / 10.0;
    const qreal T = 2 * M_PI * std::sqrt(L / g);   // 周期 ≈4.01s
    const int durationMs = qRound(T * 2 * 1000);   // ≈8024ms

    // 杆：悬挂点 (0,0) → 球心；球：单点散点
    auto* rod = new QLineSeries("rod", pendGeo);
    rod->setColor(QColor("#2196F3"));
    pendGeo->addSeries(rod);

    auto* ball = new QScatterSeries("ball", pendGeo);
    ball->setColor(QColor("#F44336"));
    ball->setMarkerSize(14);
    pendGeo->addSeries(ball);

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

    auto* rodAnim = new QNumericSeriesAnimation(pendW);
    rodAnim->setDuration(durationMs);
    rodAnim->setEasingCurve(QEasingCurve::Linear); // 物理时间必须线性推进
    rodAnim->setTargetSeries(rod);
    rodAnim->setGenerator(genRod);

    auto* ballAnim = new QNumericSeriesAnimation(pendW);
    ballAnim->setDuration(durationMs);
    ballAnim->setEasingCurve(QEasingCurve::Linear);
    ballAnim->setTargetSeries(ball);
    ballAnim->setGenerator(genBall);

    auto* group = new QParallelAnimationGroup(pendW);
    group->addAnimation(rodAnim);
    group->addAnimation(ballAnim);

    // 动画结束：清除覆盖层，恢复真实数据渲染
    QObject::connect(group, &QParallelAnimationGroup::finished, rod, [rod, ball]() {
        rod->clearRenderOverride();
        ball->clearRenderOverride();
        qDebug() << "单摆动画结束，覆盖层已清除";
    });

    group->start();

    pendW->resize(600, 600);
    pendW->show();

    // ===== 窗口 4: 排序算法演示（QBarAnimation 模式 A lerp）=====
    qDebug() << "\n========== 窗口4: 冒泡排序动画 ==========";

    auto* sortW = new QChartWidget();
    sortW->setWindowTitle("冒泡排序 - QBarAnimation 交换动画");
    sortW->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    sortW->setViewRectFitMode(ViewRectFitMode::Stretch);

    auto* sortX = new QBarCategoryAxis(sortW, Qt::AlignBottom);
    sortX->setColor(Qt::white);
    sortW->addAxis(sortX);

    auto* sortY = new QValueAxis(sortW, Qt::AlignLeft);
    sortY->setColor(Qt::white);
    sortW->addAxis(sortY);
    sortY->setRange(0, 10);

    auto* sortLayer = new QChartLayer(sortW);
    sortLayer->setAxisX(sortX);
    sortLayer->setAxisY(sortY);
    sortW->addLayer(sortLayer);

    auto* sortBars = new QBarSeries("bars", sortLayer);
    sortBars->setColor(QColor("#4CAF50"));
    sortBars->setFillColor(QColor(76, 175, 80, 200));

    // 初始乱序高度（Numeric 空间 x∈[-0.5, n-0.5]，y∈[0, 10]）
    QVector<qreal> heights = { 3, 7, 1, 9, 4, 8, 2, 6, 5 };
    const int N = heights.size();
    sortX->setCategories(QStringList{"a","b","c","d","e","f","g","h","i"});
    sortX->setNumericMapping(-0.5, N - 0.5);

    for (int i = 0; i < N; ++i)
        sortBars->append(i - 0.4, 0, i + 0.4, heights[i]);
    sortLayer->addSeries(sortBars);

    // 冒泡排序：每步一个交换动画（QBarAnimation 模式 B Generator）
    // 注意：动画期间 Series 真实数据不修改——算法侧维护 working 高度数组，
    // 每步动画从"交换前快照"lerp 到"交换后目标"；动画结束后一次性落地真实数据
    auto* sortSeq = new QSequentialAnimationGroup(sortW);

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
                auto* anim = new QBarAnimation(sortSeq);
                anim->setDuration(280);
                anim->setEasingCurve(QEasingCurve::InOutQuad);
                anim->setTargetSeries(sortBars);
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
                sortSeq->addAnimation(anim);
            }
        }
        ++pass;
    }

    // 动画结束：数据落地 + 清除覆盖层
    QObject::connect(sortSeq, &QSequentialAnimationGroup::finished, sortW, [sortW, sortBars, working]() {
        // 真实数据更新为排序结果（之前动画期间从未动过）
        for (int k = 0; k < working.size(); ++k)
            sortBars->replace(k, QDataRect(k - 0.4, 0, k + 0.4, working[k]));
        sortBars->clearRenderOverride();
        qDebug() << "排序完成，数据已落地，覆盖层已清除";
    });

    sortSeq->start();

    sortW->resize(600, 400);
    sortW->show();

    return app.exec();
}
