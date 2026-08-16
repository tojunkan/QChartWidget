// test_qvalueaxis.cpp —— QValueAxis 单元测试
// tickValues/niceStep 纯函数断言：输入 Numeric 范围，输出刻度序列
// 类声明见 test_qvalueaxis.h（Q_OBJECT 在头文件，moc 由 QtMoc 处理）
#include <QtTest>
#include "../../QValueAxis.h"
#include "test_qvalueaxis.h"

// ===== (0,100)：niceStep 应选 20 =====
void TestQValueAxis::tickValues_regularRange() {
    QValueAxis axis;
    QVector<qreal> ticks = axis.tickValues(0, 100);

    QCOMPARE(ticks.size(), 6);                      // {0,20,40,60,80,100}
    QCOMPARE(ticks[0], 0.0);
    QCOMPARE(ticks[1], 20.0);
    QCOMPARE(ticks[5], 100.0);
    QCOMPARE(ticks[2] - ticks[1], ticks[1] - ticks[0]); // 等距
}

// ===== (-5,5)：默认 tickCount=5 → roughStep=2 命中候选 2.0 =====
void TestQValueAxis::tickValues_symmetricRange() {
    QValueAxis axis;
    QVector<qreal> ticks = axis.tickValues(-5, 5);

    // niceStep: range=10, targetTicks=5 → roughStep=2 → 候选表命中 2.0
    QCOMPARE(ticks.size(), 5);                      // {-4,-2,0,2,4}
    QCOMPARE(ticks[0], -4.0);
    QCOMPARE(ticks[2], 0.0);

    // 显式 tickInterval(1) 才能得到 -5..5 逐 1
    axis.setTickInterval(1);
    QVector<qreal> fine = axis.tickValues(-5, 5);
    QCOMPARE(fine.size(), 11);
    QCOMPARE(fine[0], -5.0);
    QCOMPARE(fine[10], 5.0);
}

// ===== 退化范围：min==max → 单刻度，不崩 =====
void TestQValueAxis::tickValues_degenerateRange() {
    QValueAxis axis;
    QVector<qreal> ticks = axis.tickValues(5, 5);

    QVERIFY(ticks.size() >= 1);
    QCOMPARE(ticks[0], 5.0);
}

// ===== 显式 tickInterval：极坐标角度轴 360° → 90° 分度 =====
void TestQValueAxis::tickValues_explicitInterval() {
    QValueAxis axis;
    axis.setTickInterval(90);
    QVector<qreal> ticks = axis.tickValues(0, 360);

    QCOMPARE(ticks.size(), 5);                      // {0,90,180,270,360}
    QCOMPARE(ticks[1], 90.0);
    QCOMPARE(ticks[3], 270.0);
    QCOMPARE(ticks[4], 360.0);
}

// ===== fromNumeric：数值 → Data → 数值 往返 =====
void TestQValueAxis::fromNumeric_roundtrip() {
    QValueAxis axis;
    qreal original = 42.5;
    QVariant data = axis.fromNumeric(original);
    QCOMPARE(axis.toNumeric(data), original);
}

// ===== subTickValues：主刻度区间内均分，跨主刻度处间隔 2×subStep =====
// 实现：每区间独立补 subTickCount 个次刻度（不跨主刻度），
// 所以区间末→下区间首的间隔是 2×subStep
void TestQValueAxis::subTickValues_division() {
    QValueAxis axis;
    axis.setTickInterval(10);
    axis.setSubTickCount(4);

    QVector<qreal> subs = axis.subTickValues(0, 100);

    QVERIFY(!subs.isEmpty());
    qreal subStep = 10.0 / (4 + 1);   // = 2
    for (int i = 1; i < subs.size(); ++i) {
        qreal gap = subs[i] - subs[i-1];
        // 要么是区间内均分（subStep），要么跨主刻度（2×subStep）
        QVERIFY(qAbs(gap - subStep) < 1e-9 || qAbs(gap - 2 * subStep) < 1e-9);
    }
    // 首尾都不越界
    QVERIFY(subs.first() > 0.0);
    QVERIFY(subs.last() < 100.0);
}
