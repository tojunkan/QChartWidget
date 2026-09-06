// Test/unit/main.cpp —— 分期单元测试入口（S0 起重写）
// 旧版 main_OLD.cpp 保留参考。阶梯约定：新测试类在此 include + 追加 qExec 行。
#include <QtTest>
#include <QApplication>

// S0：在此 include 新测试头
#include "test_axis_pipeline.h"
// 批次 A：widget 容器冒烟
#include "test_widget_smoke.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_UNUSED(app);
    int rc = 0;
    // S0：在此追加 QTest::qExec(new TestXxx, argc, argv)
    rc |= QTest::qExec(new TestAxisPipeline, argc, argv);
    rc |= QTest::qExec(new TestWidgetSmoke, argc, argv);

    return rc;
}
