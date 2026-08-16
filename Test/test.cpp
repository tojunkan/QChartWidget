// test.cpp —— 五空间架构验证主程序
// 每个演示独立封装在 demos/ 下，main 按需调用
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QLoggingCategory>
#include "../QChartWidget.h"
#include "demos/demos.h"

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
    QString logPath = "test_log.txt";  // 相对路径：落在程序当前工作目录
    g_logFile.setFileName(logPath);
    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fprintf(stderr, "无法打开日志文件: %s\n", qPrintable(logPath));
        return 1;
    }
    g_logStream.setDevice(&g_logFile);
    qInstallMessageHandler(logToFile);
    QLoggingCategory::setFilterRules("chart.*.verbose=false");

    QApplication app(argc, argv);
    qDebug() << "========== 测试开始 ==========";
    qDebug() << "日志文件:" << logPath;

    // ── 演示调用表：想跑哪个解注释哪个 ──
    auto* w = buildDemoSwirl();      // 投影切换（当前验证目标）
    // auto* w = buildDemoPolar();   // Polar 五边形
    // auto* w = buildDemoBar();     // Cartesian 柱状图
    // auto* w = buildDemoPendulum();// 单摆动画
    // auto* w = buildDemoSort();    // 冒泡排序
    // auto* w = buildDemoCamera();  // 相机漫游
    // auto* w = buildDemoStress();  // 折线粗筛压力（1M 点缩放）
    if (w) w->show();

    return app.exec();
}
