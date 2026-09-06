// Test/integration/main.cpp —— 分期集成测试入口（S0 起重写）
// 旧版 main_OLD.cpp 保留参考。阶梯约定：新集成测试类在此 include + 追加 qExec 行。
#include <QtTest>
#include <QApplication>

// S0：在此 include 新集成测试头
#include "test_axis_matrix.h"
// 批次 A：widget GL 宿主冒烟
#include "test_widget_gl.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_UNUSED(app);
    int rc = 0;
    // S0：在此追加 QTest::qExec(new TestXxx, argc, argv)
    rc |= QTest::qExec(new TestAxisMatrixCpu, argc, argv);
    rc |= QTest::qExec(new TestAxisMatrixGl, argc, argv);
    rc |= QTest::qExec(new TestWidgetGl, argc, argv);

    return rc;
}
