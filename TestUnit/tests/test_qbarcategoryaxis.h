// test_qbarcategoryaxis.h —— QBarCategoryAxis 单元测试声明
#pragma once
#include <QObject>

class TestQBarCategoryAxis : public QObject {
    Q_OBJECT
private slots:
    void toNumeric_qrealPassThrough();    // qreal 直通，不查类别表
    void toNumeric_categoryMapping();     // 类别名 → 线性映射
    void toNumeric_unknownCategory();     // 未知类别 → NaN
    void fromNumeric_reverseMapping();    // Numeric → 类别名
    void tickValues_labels();             // 刻度 = 类别索引
    void emptyCategories();               // 空类别不崩
};
