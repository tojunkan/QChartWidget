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
#include <QParallelAnimationGroup>

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
    QLoggingCategory::setFilterRules("chart.*.verbose=true");

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

    auto* polarGeo = new QChartGeometry(polarW);
    polarGeo->setAxisX(angleAxis);
    polarGeo->setAxisY(radialAxis);
    polarW->addGeometry(polarGeo);

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

    auto* barGeo = new QChartGeometry(barW);
    barGeo->setAxisX(xAxis);
    barGeo->setAxisY(valAxis);
    barW->addGeometry(barGeo);

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
    pendW->setViewRectFitMode(ViewRectFitMode::Stretch);

    auto* pendX = new QValueAxis(pendW, Qt::AlignBottom);
    pendX->setColor(Qt::white);
    pendW->addAxis(pendX);
    pendX->setRange(-5, 5);

    auto* pendY = new QValueAxis(pendW, Qt::AlignLeft);
    pendY->setColor(Qt::white);
    pendW->addAxis(pendY);
    pendY->setRange(-5, 1);

    auto* pendGeo = new QChartGeometry(pendW);
    pendGeo->setAxisX(pendX);
    pendGeo->setAxisY(pendY);
    pendW->addGeometry(pendGeo);

    // 物理参数：L=4, g=9.8, θ₀=60°，动画跑两个周期
    const qreal L = 4.0, g = 9.8, theta0 = M_PI / 3.0;
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

    pendW->resize(600, 500);
    pendW->show();

    return app.exec();
}
