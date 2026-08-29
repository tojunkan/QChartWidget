// main.cpp —— TestUnit 聚合入口（支持 -test <ClassName> 按类过滤）
#include <QtTest>
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

// 包含所有测试头文件
#include "tests/test_qvalueaxis.h"
#include "tests/test_qbarcategoryaxis.h"
#include "tests/test_qlogaxis.h"
#include "tests/test_qdatetimeaxis.h"
#include "tests/test_qchartcamera.h"
#include "tests/test_qchartcamera3d.h"
#include "tests/test_qchartaxes3d.h"
#include "tests/test_qchartmath.h"
#include "tests/test_qchartprojection.h"
#include "tests/test_qchartsurface3d.h"
#include "tests/test_qchartrenderer.h"
#include "tests/test_qchartrenderer3d.h"
#include "tests/test_hittest.h"
#include "tests/test_qcharthittester.h"
#include "tests/test_qcharttheme.h"
#include "tests/test_qchartlegend.h"
#include "tests/test_legend_interact.h"
#include "tests/test_export.h"
#include "tests/test_qchartgl.h"
#include "tests/test_qopenglrenderer.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_UNUSED(app);

    // 解析自定义参数 -test <ClassName>
    QString filterClass;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "-test" && i + 1 < argc) {
            filterClass = QString::fromLocal8Bit(argv[i + 1]);
            // 移除 -test 和类名，避免透传给 QTest::qExec（会报 unrecognized option）
            // 但这里我们选择保留，因为 QTest 不认识 -test 会报错。
            // 所以我们不直接传 argc, argv 给 qExec，而是构造新的 argv。
            break;
        }
    }

    // 构造给 QTest 的 argv：去掉 -test 和它的参数
    QVector<QByteArray> filteredArgvStorage;
    QVector<char*> filteredArgv;
    for (int i = 0; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-test" && i + 1 < argc) {
            ++i; // 跳过 -test 和类名本身
            continue;
        }
        filteredArgvStorage.append(argv[i]);
        filteredArgv.append(filteredArgvStorage.last().data());
    }
    int filteredArgc = filteredArgv.size();

    // Lambda：如果指定了 -test，只运行匹配的类；否则运行全部
    auto runTest = [&](QObject* test, const char* name) -> int {
        if (!filterClass.isEmpty() && filterClass != name) {
            // 静默跳过（或者你可以加 qDebug() 打印过滤信息）
            // qDebug() << "Skipping" << name;
            delete test;
            return 0;
        }
        qDebug().noquote() << "▶ 运行测试类:" << name;
        // 注意：这里必须传递 filteredArgc, filteredArgv.data()，否则 -test 会被 QTest 报错
        return QTest::qExec(test, filteredArgc, filteredArgv.data());
    };

    int rc = 0;
    // 所有测试类列表（按字母顺序或任意顺序）
    rc += runTest(new TestQValueAxis, "TestQValueAxis");
    rc += runTest(new TestQBarCategoryAxis, "TestQBarCategoryAxis");
    rc += runTest(new TestQLogAxis, "TestQLogAxis");
    rc += runTest(new TestQDateTimeAxis, "TestQDateTimeAxis");
    rc += runTest(new TestQChartCamera2D, "TestQChartCamera2D");
    rc += runTest(new TestQChartCamera3D, "TestQChartCamera3D");
    rc += runTest(new TestQChartAxes3D, "TestQChartAxes3D");
    rc += runTest(new TestQChartMath, "TestQChartMath");
    rc += runTest(new TestQChartProjection, "TestQChartProjection");
    rc += runTest(new TestQChartSurface3D, "TestQChartSurface3D");
    rc += runTest(new TestQChartRenderer, "TestQChartRenderer");
    rc += runTest(new TestQChartRenderer3D, "TestQChartRenderer3D");
    rc += runTest(new TestHitTest, "TestHitTest");
    rc += runTest(new TestQChartHitTester, "TestQChartHitTester");
    rc += runTest(new TestQChartTheme, "TestQChartTheme");
    rc += runTest(new TestQChartLegend, "TestQChartLegend");
    rc += runTest(new TestLegendInteract, "TestLegendInteract");
    rc += runTest(new TestExport, "TestExport");
    rc += runTest(new TestQChartGL, "TestQChartGL");
    rc += runTest(new TestQOpenGLRenderer, "TestQOpenGLRenderer");

    if (!filterClass.isEmpty()) {
        qDebug().noquote() << "========== 仅运行测试类:" << filterClass << "==========";
    } else {
        qDebug().noquote() << "========== TestUnit 全部完成 ==========";
    }
    return rc;
}