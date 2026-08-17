// main.cpp —— TestUnit 聚合入口
// 新模块测试：tests/ 下新建 test_xxx.h（类声明）+ test_xxx.cpp（实现），
// 然后在本文件 include + 加一行 QTest::qExec
// 使用 QApplication：test_qchartrenderer 渲染文字/QPixmap 缓存需 QGuiApplication，
// 而 test_qcharttheme 需实例化 QChartWidget（QWidget）→ 需要 QApplication；
// 无头跑由 ctest 设 QT_QPA_PLATFORM=offscreen。
#include <QtTest>
#include <QApplication>
#include "tests/test_qvalueaxis.h"
#include "tests/test_qbarcategoryaxis.h"
#include "tests/test_qlogaxis.h"
#include "tests/test_qdatetimeaxis.h"
#include "tests/test_qchartcamera.h"
#include "tests/test_qchartcamera3d.h"
#include "tests/test_qchartmath.h"
#include "tests/test_qchartprojection.h"
#include "tests/test_qchartsurface3d.h"
#include "tests/test_qchartrenderer.h"
#include "tests/test_qchartrenderer3d.h"
#include "tests/test_hittest.h"
#include "tests/test_qcharttheme.h"
#include "tests/test_qchartlegend.h"
#include "tests/test_legend_interact.h"
#include "tests/test_export.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_UNUSED(app);

    int rc = 0;
    rc += QTest::qExec(new TestQValueAxis, argc, argv);
    rc += QTest::qExec(new TestQBarCategoryAxis, argc, argv);
    rc += QTest::qExec(new TestQLogAxis, argc, argv);
    rc += QTest::qExec(new TestQDateTimeAxis, argc, argv);
    rc += QTest::qExec(new TestQChartCamera2D, argc, argv);
    rc += QTest::qExec(new TestQChartCamera3D, argc, argv);
    rc += QTest::qExec(new TestQChartMath, argc, argv);
    rc += QTest::qExec(new TestQChartProjection, argc, argv);
    rc += QTest::qExec(new TestQChartSurface3D, argc, argv);
    rc += QTest::qExec(new TestQChartRenderer, argc, argv);
    rc += QTest::qExec(new TestQChartRenderer3D, argc, argv);
    rc += QTest::qExec(new TestHitTest, argc, argv);
    rc += QTest::qExec(new TestQChartTheme, argc, argv);
    rc += QTest::qExec(new TestQChartLegend, argc, argv);
    rc += QTest::qExec(new TestLegendInteract, argc, argv);
    rc += QTest::qExec(new TestExport, argc, argv);

    qDebug() << "\n========== TestUnit 全部完成 ==========";
    return rc;
}
