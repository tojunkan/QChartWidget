// test_qvalueaxis.h —— QValueAxis 单元测试声明
#pragma once
#include <QObject>

class TestQValueAxis : public QObject {
    Q_OBJECT
private slots:
    void tickValues_regularRange();      // (0,100) → step 20
    void tickValues_symmetricRange();    // (-5,5) → step 1
    void tickValues_degenerateRange();   // (5,5) 单点不崩
    void tickValues_explicitInterval();  // setTickInterval(90) 极坐标角度轴
    void fromNumeric_roundtrip();
    void subTickValues_division();
};
