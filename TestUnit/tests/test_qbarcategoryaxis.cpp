// test_qbarcategoryaxis.cpp —— QBarCategoryAxis 单元测试
// 历史 bug 回归：qreal 直通（Bar 用小数索引）、类别名线性映射（5 类别 4 槽位）
// 类声明见 test_qbarcategoryaxis.h
#include <QtTest>
#include "../../QBarCategoryAxis.h"
#include "test_qbarcategoryaxis.h"

// ===== qreal 直通：Bar 用小数索引定位，不查类别表 =====
void TestQBarCategoryAxis::toNumeric_qrealPassThrough() {
    QBarCategoryAxis axis;
    axis.setCategories({"苹果","香蕉","橙子","葡萄","西瓜"});

    QCOMPARE(axis.toNumeric(QVariant(1.5)), 1.5);
    QCOMPARE(axis.toNumeric(QVariant(-0.3)), -0.3);
    QCOMPARE(axis.toNumeric(QVariant(3)), 3.0);     // int 也直通
}

// ===== 类别名 → 线性映射到 [numericMin, numericMax] =====
// 5 类别 + mapping(-0.5, 4.5)：索引 0 → -0.5，索引 4 → 4.5
void TestQBarCategoryAxis::toNumeric_categoryMapping() {
    QBarCategoryAxis axis;
    axis.setCategories({"苹果","香蕉","橙子","葡萄","西瓜"});
    axis.setNumericMapping(-0.5, 4.5);

    QCOMPARE(axis.toNumeric(QVariant("苹果")), -0.5);
    QCOMPARE(axis.toNumeric(QVariant("香蕉")), 0.75);   // 1/4 位置
    QCOMPARE(axis.toNumeric(QVariant("西瓜")), 4.5);
}

// ===== 未知类别 → NaN（调用方负责跳过）=====
void TestQBarCategoryAxis::toNumeric_unknownCategory() {
    QBarCategoryAxis axis;
    axis.setCategories({"苹果","香蕉"});

    QVERIFY(std::isnan(axis.toNumeric(QVariant("不存在"))));
}

// ===== fromNumeric 逆映射：-0.5 → 第一个类别 =====
void TestQBarCategoryAxis::fromNumeric_reverseMapping() {
    QBarCategoryAxis axis;
    axis.setCategories({"苹果","香蕉","橙子","葡萄","西瓜"});
    axis.setNumericMapping(-0.5, 4.5);

    QCOMPARE(axis.fromNumeric(-0.5).toString(), QString("苹果"));
    QCOMPARE(axis.fromNumeric(4.5).toString(), QString("西瓜"));
}

// ===== tickValues/tickLabels：每类别一个刻度 =====
void TestQBarCategoryAxis::tickValues_labels() {
    QBarCategoryAxis axis;
    axis.setCategories({"a","b","c"});

    QVector<qreal> ticks = axis.tickValues(0, 2);
    QCOMPARE(ticks.size(), 3);
    QCOMPARE(ticks[0], 0.0);
    QCOMPARE(ticks[2], 2.0);

    QStringList labels = axis.tickLabels(ticks);
    QCOMPARE(labels, QStringList({"a","b","c"}));
}

// ===== 空类别：不崩 =====
void TestQBarCategoryAxis::emptyCategories() {
    QBarCategoryAxis axis;
    QVERIFY(std::isnan(axis.toNumeric(QVariant("x"))));
    QCOMPARE(axis.tickValues(0, 1).size(), 0);
}
