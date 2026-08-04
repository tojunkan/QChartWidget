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
#include "../QChartWidget.h"
#include "../QChartProjectionFactory.h"
#include "../QValueAxis.h"
#include "../ProjectionToolKit.h"

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
    chartWidget->setProjection(createPower2Projection());
	chartWidget->setViewRectFitMode(ViewRectFitMode::Fit);
	//chartWidget->setFixedAspectRatio(1.0);  // 强制 viewRect

    // dim0 = 角度 (AlignHCenter)
    QValueAxis * angleAxis = new QValueAxis(chartWidget, Qt::AlignHCenter);
    angleAxis->setColor(Qt::white);
    chartWidget->addAxis(angleAxis);
	angleAxis->setRange(-M_PI, M_PI);
    //angleAxis->setTickInterval(45);
    //angleAxis->setRange(0, 360);
    //angleAxis->setTickCount(9);

    // dim1 = 半径 (AlignVCenter)
    QValueAxis * radialAxis = new QValueAxis(chartWidget, Qt::AlignVCenter);
    radialAxis->setColor(Qt::white);
    chartWidget->addAxis(radialAxis);
    //radialAxis->setRange(0, 10);
    //radialAxis->setTickCount(5);

    QChartGeometry * geometry = new QChartGeometry(chartWidget);
    geometry->setAxisX(angleAxis);
    geometry->setAxisY(radialAxis);
    chartWidget->addGeometry(geometry);
    chartWidget->resize(800, 600);

	QRectF valid(-INFINITY, -INFINITY, INFINITY, INFINITY);

    chartWidget->show();
    return app.exec();
}
