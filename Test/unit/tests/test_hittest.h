// test_hittest.h —— Series 命中检测单元测试声明
#pragma once
#include <QObject>

class TestHitTest : public QObject {
    Q_OBJECT
private slots:
    void scatter_hitAndMiss();   // QScatterSeries 点阈值命中/未命中
    void line_hitAndMiss();      // QLineSeries 线阈值命中/未命中
    void bar_rectHit();          // QBarSeries 矩形内命中
};
