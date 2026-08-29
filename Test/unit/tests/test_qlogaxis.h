// test_qlogaxis.h —— QLogAxis 单元测试声明
#pragma once
#include <QObject>

class TestQLogAxis : public QObject {
    Q_OBJECT
private slots:
    void toNumeric_positive();     // 100 → 2, 0.1 → -1
    void toNumeric_nonPositive();  // 0 和负数 → NaN
    void fromNumeric_roundtrip();  // 2 → 100
    void tickValues_logSpace();    // [0,3] → {0,1,2,3} 每个数量级
};
