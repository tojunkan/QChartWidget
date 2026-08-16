// test_qdatetimeaxis.cpp —— QDateTimeAxis 单元测试
// Data(QDateTime) ↔ Numeric(epoch ms) + 自适应时间步长
// 类声明见 test_qdatetimeaxis.h
#include <QtTest>
#include "../../QDateTimeAxis.h"
#include "test_qdatetimeaxis.h"

void TestQDateTimeAxis::toNumeric_epoch() {
    QDateTimeAxis axis;
    QDateTime dt(QDate(2026, 8, 6), QTime(12, 30, 0));

    qreal num = axis.toNumeric(QVariant::fromValue(dt));
    QCOMPARE(num, static_cast<qreal>(dt.toMSecsSinceEpoch()));

    // 往返一致
    QVariant back = axis.fromNumeric(num);
    QCOMPARE(back.toDateTime(), dt);
}

void TestQDateTimeAxis::setRange_datetimeSugar() {
    QDateTimeAxis axis;
    QDateTime min(QDate(2026, 8, 1), QTime(0, 0, 0));
    QDateTime max(QDate(2026, 8, 8), QTime(0, 0, 0));

    // QDateTime 版 setRange 应转发到基类语法糖
    axis.setRange(min, max);
    QCOMPARE(axis.min(), static_cast<qreal>(min.toMSecsSinceEpoch()));
    QCOMPARE(axis.max(), static_cast<qreal>(max.toMSecsSinceEpoch()));
}

// 小时级范围（4 小时）：刻度应是整小时对齐
void TestQDateTimeAxis::tickValues_hourRange() {
    QDateTimeAxis axis;
    qreal min = static_cast<qreal>(QDateTime(QDate(2026, 8, 6), QTime(0, 0, 0)).toMSecsSinceEpoch());
    qreal max = min + 4 * 3600 * 1000.0;  // +4 小时

    QVector<qreal> ticks = axis.tickValues(min, max);
    QVERIFY(ticks.size() >= 2);

    // 每两个刻度之间是整小时（3600000 ms）
    qreal step = ticks[1] - ticks[0];
    QCOMPARE(step, 3600000.0);
    // 起始对齐整点
    QCOMPARE(static_cast<qint64>(ticks[0]) % 3600000LL, 0LL);
}

// 天级范围（5 天）：刻度应是整天对齐
void TestQDateTimeAxis::tickValues_dayRange() {
    QDateTimeAxis axis;
    qreal min = static_cast<qreal>(QDateTime(QDate(2026, 8, 1), QTime(0, 0, 0)).toMSecsSinceEpoch());
    qreal max = min + 5 * 86400 * 1000.0;  // +5 天

    QVector<qreal> ticks = axis.tickValues(min, max);
    QVERIFY(ticks.size() >= 2);

    qreal step = ticks[1] - ticks[0];
    QCOMPARE(step, 86400000.0);
}

void TestQDateTimeAxis::tickLabels_customFormat() {
    QDateTimeAxis axis;
    axis.setFormat("yyyy-MM-dd");

    qreal min = static_cast<qreal>(QDateTime(QDate(2026, 8, 6), QTime(0, 0, 0)).toMSecsSinceEpoch());
    QVector<qreal> ticks = axis.tickValues(min, min + 2 * 86400 * 1000.0);
    QStringList labels = axis.tickLabels(ticks);

    QVERIFY(!labels.isEmpty());
    QCOMPARE(labels[0], QString("2026-08-06"));
}

void TestQDateTimeAxis::degenerateRange() {
    QDateTimeAxis axis;
    QVector<qreal> ticks = axis.tickValues(5, 5);
    QVERIFY(ticks.size() >= 1);
    QCOMPARE(ticks[0], 5.0);
}
