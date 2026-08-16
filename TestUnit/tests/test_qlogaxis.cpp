// test_qlogaxis.cpp —— QLogAxis 单元测试
// Data(qreal>0) ↔ Numeric(log10) 换算 + 对数域刻度
// 类声明见 test_qlogaxis.h
#include <QtTest>
#include "../../QLogAxis.h"
#include "test_qlogaxis.h"

void TestQLogAxis::toNumeric_positive() {
    QLogAxis axis;
    QCOMPARE(axis.toNumeric(QVariant(100.0)), 2.0);
    QCOMPARE(axis.toNumeric(QVariant(0.1)), -1.0);
    QCOMPARE(axis.toNumeric(QVariant(1.0)), 0.0);
}

void TestQLogAxis::toNumeric_nonPositive() {
    QLogAxis axis;
    QVERIFY(std::isnan(axis.toNumeric(QVariant(0.0))));
    QVERIFY(std::isnan(axis.toNumeric(QVariant(-5.0))));
}

void TestQLogAxis::fromNumeric_roundtrip() {
    QLogAxis axis;
    QCOMPARE(axis.fromNumeric(2.0).toDouble(), 100.0);
    QCOMPARE(axis.fromNumeric(-1.0).toDouble(), 0.1);
}

void TestQLogAxis::tickValues_logSpace() {
    QLogAxis axis;
    // Numeric 域 [0,3] = 数据域 [1,1000]：每个数量级一个刻度
    QVector<qreal> ticks = axis.tickValues(0, 3);

    QVERIFY(ticks.size() >= 4);
    QCOMPARE(ticks[0], 0.0);
    QVERIFY(ticks.contains(1.0));   // 10
    QVERIFY(ticks.contains(2.0));   // 100
    QVERIFY(ticks.contains(3.0));   // 1000
}
