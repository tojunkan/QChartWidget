// test_qdatetimeaxis.h —— QDateTimeAxis 单元测试声明
#pragma once
#include <QObject>

class TestQDateTimeAxis : public QObject {
    Q_OBJECT
private slots:
    void toNumeric_epoch();              // QDateTime → epoch ms 往返
    void setRange_datetime();            // QDateTime 版 setRange（4c 措辞：范围写入，非"语法糖"）
    void tickValues_hourRange();         // 小时级范围 → 整小时刻度
    void tickValues_dayRange();          // 天级范围 → 整天刻度
    void tickLabels_customFormat();      // 自定义 format 生效
    void degenerateRange();              // min==max 不崩
};
