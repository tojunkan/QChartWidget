// main.cpp —— TestUnit 聚合入口
// 新模块测试：tests/ 下新建 test_xxx.h（类声明）+ test_xxx.cpp（实现），
// 然后在本文件 include + 加一行 QTest::qExec
#include <QtTest>
#include "tests/test_qvalueaxis.h"
#include "tests/test_qbarcategoryaxis.h"
#include "tests/test_qlogaxis.h"
#include "tests/test_qdatetimeaxis.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    int rc = 0;
    rc += QTest::qExec(new TestQValueAxis, argc, argv);
    rc += QTest::qExec(new TestQBarCategoryAxis, argc, argv);
    rc += QTest::qExec(new TestQLogAxis, argc, argv);
    rc += QTest::qExec(new TestQDateTimeAxis, argc, argv);

    qDebug() << "\n========== TestUnit 全部完成 ==========";
    return rc;
}
