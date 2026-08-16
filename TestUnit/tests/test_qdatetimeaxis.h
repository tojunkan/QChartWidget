// test_qdatetimeaxis.h —— QDateTimeAxis 单元测试声明
#pragma once
#include <QObject>

class TestQDateTimeAxis : public QObject {
    Q_OBJECT
private slots:
    void toNumeric_epoch();              // QDateTime → epoch ms 往返
    void setRange_datetimeSugar();       // QDateTime 版 setRange
    void tickValues_hourRange();         // 小时级范围 → 整小时刻度
    void tickValues_dayRange();          // 天级范围 → 整天刻度
    void tickLabels_customFormat();      // 自定义 format 生效
    void degenerateRange();              // min==max 不崩
};
