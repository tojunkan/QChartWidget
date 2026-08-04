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
#include "../QRegionSeries.h"
#include "../QBarSeries.h"
#include "../QValueAxis.h"
#include "../ProjectionToolKit.h"
#include "../QDateTimeAxis.h"
#include "../QBarCategoryAxis.h"
#include "../QLogAxis.h"
#include "../QDataPoint.h"

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
    QLoggingCategory::setFilterRules("chart.*.debug=false");

    QApplication app(argc, argv);
    qDebug() << "========== 测试开始 ==========";
    qDebug() << "日志文件:" << logPath;

    qDebug() << "\n========== 图形化交互测试 ==========";

    QChartWidget * chartWidget = new QChartWidget();
    chartWidget->setProjection(createSwirlProjection());
	chartWidget->setViewRectFitMode(ViewRectFitMode::Fit);
	//chartWidget->setFixedAspectRatio(1.0);  // 强制 viewRect

    // dim0 = 角度 (AlignHCenter)
    QValueAxis * angleAxis = new QValueAxis(chartWidget, Qt::AlignHCenter);
    angleAxis->setColor(Qt::white);
    chartWidget->addAxis(angleAxis);
    //angleAxis->setRange(0, 360);
    //angleAxis->setTickCount(9);

    // dim1 = 半径 (AlignVCenter)
    QValueAxis * radialAxis = new QValueAxis(chartWidget, Qt::AlignVCenter);
    radialAxis->setColor(Qt::white);
    chartWidget->addAxis(radialAxis);
    //radialAxis->setTickInterval(M_PI / 3);
    //radialAxis->setRange(0, 10);
    //radialAxis->setTickCount(5);

    QChartGeometry * geometry = new QChartGeometry(chartWidget);
    geometry->setAxisX(angleAxis);
    geometry->setAxisY(radialAxis);
    chartWidget->addGeometry(geometry);
    chartWidget->resize(800, 600);

    // ===== QScatterSeries 验证 =====
    // 在 Swirl 投影下加 200 个散点，验证 Data→toPixel 渲染链路
    auto* scatter = new QScatterSeries("swirl-scatter", geometry);
    scatter->setMarkerShape(QScatterSeries::MarkerShape::Circle);
    scatter->setMarkerSize(6);
    scatter->setColor(QColor("#4CAF50"));
    scatter->setFillColor(QColor("#4CAF50"));

    // 随机点在 [-4,4]² 内（Swirl 的有效输入范围）
    auto* rng = QRandomGenerator::global();
    for (int i = 0; i < 200; ++i) {
        qreal x = -4.0 + rng->generateDouble() * 8.0;
        qreal y = -4.0 + rng->generateDouble() * 8.0;
        scatter->append(x, y);
    }
    qDebug() << "Scatter points:" << scatter->count();
    geometry->addSeries(scatter);

    // ===== QLineSeries 验证：正弦线 =====
    auto* line = new QLineSeries("sine", geometry);
    line->setColor(QColor("#FF9800"));
    line->setLineWidth(2.0);
    for (int i = 0; i < 100; ++i) {
        qreal x = -3.0 + i * 6.0 / 99.0;
        qreal y = qSin(x);
        line->append(x, y);
    }
    geometry->addSeries(line);

    // ===== QRegionSeries 验证：填充三角形 =====
    auto* region = new QRegionSeries("triangle", geometry);
    region->setColor(QColor("#9C27B0"));
    region->setFillColor(QColor(156, 39, 176, 80));  // 半透明紫
    region->append(-3.0, -1.0);
    region->append(3.0, -1.0);
    region->append(0.0, 2.0);
    geometry->addSeries(region);

    // ===== QBarSeries 验证：笛卡尔矩形柱 =====
    auto* bars = new QBarSeries("bars", geometry);
    bars->setColor(QColor("#00BCD4"));
    bars->setFillColor(QColor(0, 188, 212, 180));
    // 在 Swirl 输入范围 [-4,4]² 内放几个矩形柱
    bars->append(-4.0, -4.0, -2.0, -1.0);
    bars->append(-1.0, -3.0, 1.0, -2.0);
    bars->append(2.0, -4.0, 3.5, -1.5);
    geometry->addSeries(bars);

    qDebug() << "All series added: line, region, bars";

    // ===== Test 4: QDateTimeAxis (Data=QDateTime, Numeric=epoch ms) =====
    qDebug() << "\n===== Test 4: QDateTimeAxis =====";
    auto* dtWidget = new QChartWidget();
    dtWidget->setWindowTitle("QDateTimeAxis 验证");
    dtWidget->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    auto* dateAxis = new QDateTimeAxis(dtWidget, Qt::AlignBottom);
    dtWidget->addAxis(dateAxis);
    dateAxis->setRange(QDateTime(QDate(2026, 1, 1), QTime(0, 0)),
                       QDateTime(QDate(2026, 12, 31), QTime(0, 0)));
    auto* dtYAxis = new QValueAxis(dtWidget, Qt::AlignLeft);
    dtWidget->addAxis(dtYAxis);
    dtYAxis->setRange(0, 100);
    auto* dtGeo = new QChartGeometry(dtWidget);
    dtGeo->setAxisX(dateAxis);
    dtGeo->setAxisY(dtYAxis);
    dtWidget->addGeometry(dtGeo);
    auto* dtLine = new QLineSeries("Temperatures", dtGeo);
    dtLine->setColor(QColor("#FF5722"));
    for (int i = 0; i < 12; ++i) {
        QDateTime d(QDate(2026, i + 1, 15), QTime(0, 0));
        dtLine->append(QDataPoint(QVariant::fromValue(d),
                                  QVariant::fromValue(20.0 + qSin(i * 0.8) * 15.0)));
    }
    dtGeo->addSeries(dtLine);
    dtWidget->resize(800, 400);
    dtWidget->show();

    // ===== Test 5: QBarCategoryAxis (Data=QString, Numeric=index) =====
    qDebug() << "\n===== Test 5: QBarCategoryAxis =====";
    auto* catWidget = new QChartWidget();
    catWidget->setWindowTitle("QBarCategoryAxis 验证");
    catWidget->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    auto* catAxis = new QBarCategoryAxis(catWidget, Qt::AlignBottom);
    catWidget->addAxis(catAxis);
    catAxis->setCategories({QStringLiteral("苹果"), QStringLiteral("香蕉"),
                            QStringLiteral("橙子"), QStringLiteral("葡萄"),
                            QStringLiteral("西瓜")});
    auto* catYAxis = new QValueAxis(catWidget, Qt::AlignLeft);
    catWidget->addAxis(catYAxis);
    catYAxis->setRange(0, 100);
    auto* catGeo = new QChartGeometry(catWidget);
    catGeo->setAxisX(catAxis);
    catGeo->setAxisY(catYAxis);
    catWidget->addGeometry(catGeo);
    auto* catBars = new QBarSeries("Fruit Sales", catGeo);
    catBars->setColor(QColor("#4CAF50"));
    catBars->setFillColor(QColor(76, 175, 80, 160));
    catBars->append(-0.3, 0, 0.3, 45);   // 苹果: index 0
    catBars->append(0.7, 0, 1.3, 72);    // 香蕉: index 1
    catBars->append(1.7, 0, 2.3, 33);    // 橙子: index 2
    catBars->append(2.7, 0, 3.3, 88);    // 葡萄: index 3
    catBars->append(3.7, 0, 4.3, 56);    // 西瓜: index 4
    catGeo->addSeries(catBars);
    catWidget->resize(600, 400);
    catWidget->show();

    // ===== Test 6: QLogAxis (Data=qreal>0, Numeric=log10) =====
    qDebug() << "\n===== Test 6: QLogAxis =====";
    auto* logWidget = new QChartWidget();
    logWidget->setWindowTitle("QLogAxis 验证");
    logWidget->setProjection(QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    auto* logAxis = new QLogAxis(logWidget, Qt::AlignBottom);
    logWidget->addAxis(logAxis);
    logAxis->setRange(1, 10000); // Data 空间 1~10000
    auto* logYAxis = new QValueAxis(logWidget, Qt::AlignLeft);
    logWidget->addAxis(logYAxis);
    logYAxis->setRange(0, 10);
    auto* logGeo = new QChartGeometry(logWidget);
    logGeo->setAxisX(logAxis);
    logGeo->setAxisY(logYAxis);
    logWidget->addGeometry(logGeo);
    auto* logScatter = new QScatterSeries("log-points", logGeo);
    logScatter->setMarkerShape(QScatterSeries::MarkerShape::Circle);
    logScatter->setMarkerSize(6);
    logScatter->setColor(QColor("#E91E63"));
    for (qreal v = 1.0; v <= 10000.0; v *= 1.5) {
        logScatter->append(QDataPoint(QVariant::fromValue(v), QVariant::fromValue(5.0)));
    }
    logGeo->addSeries(logScatter);
    logWidget->resize(800, 400);
    logWidget->show();

    chartWidget->show();
    return app.exec();
}
