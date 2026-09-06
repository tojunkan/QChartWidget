// test_widget_smoke.h —— 批次 A widget 容器化冒烟（offscreen CPU）
#pragma once
#include <QObject>

class TestWidgetSmoke : public QObject
{
    Q_OBJECT
private slots:
    void cpuContainerRenders();   // CPU：plotArea 扣除边距 + 网格/边框轴真实出图 + 广播
};
