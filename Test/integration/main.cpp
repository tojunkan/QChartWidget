// Test/integration/main.cpp —— 集成测试聚合入口
// 支持 -test <ClassName> 按类过滤（与单元测试完全相同）
#include <QtTest>
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

// 包含所有集成测试头文件
#include "tests/test_initialization_flow.h"
// 未来新增的集成测试类在这里加 include
// #include "tests/test_resize_flow.h"
// #include "tests/test_theme_flow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_UNUSED(app);

    // 解析自定义参数 -test <ClassName>
    QString filterClass;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "-test" && i + 1 < argc) {
            filterClass = QString::fromLocal8Bit(argv[i + 1]);
            break;
        }
    }

    // 构造给 QTest 的 argv：去掉 -test 和它的参数
    QVector<QByteArray> filteredArgvStorage;
    QVector<char*> filteredArgv;
    for (int i = 0; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-test" && i + 1 < argc) {
            ++i;
            continue;
        }
        filteredArgvStorage.append(argv[i]);
        filteredArgv.append(filteredArgvStorage.last().data());
    }
    int filteredArgc = filteredArgv.size();

    auto runTest = [&](QObject* test, const char* name) -> int {
        if (!filterClass.isEmpty() && filterClass != name) {
            delete test;
            return 0;
        }
        qDebug().noquote() << "▶ 运行集成测试类:" << name;
        return QTest::qExec(test, filteredArgc, filteredArgv.data());
    };

    int rc = 0;
    rc += runTest(new TestInitializationFlow, "TestInitializationFlow");
    // 未来新增的集成测试在这里加一行
    // rc += runTest(new TestResizeFlow, "TestResizeFlow");
    // rc += runTest(new TestThemeFlow, "TestThemeFlow");

    if (!filterClass.isEmpty()) {
        qDebug().noquote() << "========== 仅运行集成测试类:" << filterClass << "==========";
    } else {
        qDebug().noquote() << "========== 集成测试全部完成 ==========";
    }
    return rc;
}