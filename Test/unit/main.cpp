// Test/unit/main.cpp —— 分期单元测试入口（S0 起重写）
// 旧版 main_OLD.cpp 保留参考。阶梯约定：新测试类在此 include + 追加 qExec 行。
#include <QtTest>
#include <QApplication>

// S0：在此 include 新测试头
#include "test_axis_pipeline.h"
// 批次 A：widget 容器冒烟
#include "test_widget_smoke.h"
// 批次 B1：3D 轴渲染冒烟
#include "test_axes3d_smoke.h"
// 批次 B2：widget3D 轻量容器冒烟
#include "test_widget3d_smoke.h"
// 批次 B3：drawAtEdge 专项
#include "test_axis_edge.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_UNUSED(app);
    int rc = 0;
    // S0：在此追加 QTest::qExec(new TestXxx, argc, argv)
    rc |= QTest::qExec(new TestAxisPipeline, argc, argv);
    rc |= QTest::qExec(new TestWidgetSmoke, argc, argv);
    rc |= QTest::qExec(new TestAxes3DSmoke, argc, argv);
    rc |= QTest::qExec(new TestWidget3DSmoke, argc, argv);
    rc |= QTest::qExec(new TestAxisEdge, argc, argv);

    return rc;
}
