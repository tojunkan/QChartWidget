// test_qchartlegend.h —— QChartLegend 单元测试声明
#pragma once
#include <QObject>

class TestQChartLegend : public QObject {
    Q_OBJECT
private slots:
    void boundingRect_fourCorners();      // 四角锚点正确、落在 plotArea 内
    void seriesAt_hitAndMiss();           // 命中色块行返回 series、空白返回 nullptr
    void legendItems_filterEmptyName();   // widget 组装跳过空 name
    void render_visibleVsHidden();        // 渲染到 QImage：可见/隐藏像素不同
    void textColorChanged_signal();       // textColorChanged 信号（override 双槽）
};
