// test.cpp —— 五空间架构验证主程序
// 每个演示独立封装在 demos/ 下，main 按需调用
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QLoggingCategory>
#include <QStringList>
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

    // ── 演示调用表 ──
    //   无参数：全部启动；带参数：只启动指定名称
    //   用法：QChartDemo.exe [polar bar pendulum sort camera swirl stress]
    struct Demo { const char* name; QChartWidget* (*build)(); };
    const Demo demos[] = {
        { "polar",    buildDemoPolar },
        { "bar",      buildDemoBar },
        { "pendulum", buildDemoPendulum },
        { "sort",     buildDemoSort },
        { "camera",   buildDemoCamera },
        { "swirl",    buildDemoSwirl },
        { "stress",   buildDemoStress },
    };

    QStringList selected;
    for (int i = 1; i < argc; ++i)
        selected << QString::fromLocal8Bit(argv[i]);
    const bool runAll = selected.isEmpty();

    int shown = 0;
    for (const Demo& d : demos) {
        if (!runAll && !selected.contains(QString::fromLatin1(d.name)))
            continue;
        QChartWidget* w = d.build();
        if (!w) {
            qWarning() << "构建演示失败:" << d.name;
            continue;
        }
        w->show();
        ++shown;
        qDebug() << "已启动演示:" << d.name;
    }
    if (shown == 0) {
        qWarning() << "没有匹配的演示。可用名称: polar bar pendulum sort camera swirl stress";
        return 1;
    }

    return app.exec();
}
