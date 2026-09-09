// Test/integration/main.cpp —— 分期集成测试入口（S0 起重写）
// 旧版 main_OLD.cpp 保留参考。阶梯约定：新集成测试类在此 include + 追加 qExec 行。
#include <QtTest>
#include <QApplication>

// S0：在此 include 新集成测试头
#include "test_axis_matrix.h"
// 批次 A：widget GL 宿主冒烟
#include "test_widget_gl.h"
// 批次 B1：3D 轴 GL 冒烟
#include "test_axes3d_gl.h"
// 批次 B2：widget3D GL 冒烟
#include "test_widget3d_gl.h"
// 批次 B3：3D 轴矩阵 + spherical
#include "test_axes3d_matrix.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_UNUSED(app);
    int rc = 0;
    // S0：在此追加 QTest::qExec(new TestXxx, argc, argv)
    // 注意（B3/t21）：offscreen 下 QSKIP 类之后的 qExec 类零执行（QtTest 怪癖）——
    // 纯 CPU 类必须排在 GL/QSKIP 类之前，否则 offscreen ctest 静默漏跑。
    rc |= QTest::qExec(new TestAxisMatrixCpu, argc, argv);
    rc |= QTest::qExec(new TestAxes3dMatrixCpu, argc, argv);
    rc |= QTest::qExec(new TestAxisMatrixGl, argc, argv);
    rc |= QTest::qExec(new TestAxes3dMatrixGl, argc, argv);
    rc |= QTest::qExec(new TestWidgetGl, argc, argv);
    rc |= QTest::qExec(new TestAxes3DGl, argc, argv);
    rc |= QTest::qExec(new TestWidget3DGl, argc, argv);

    return rc;
}
