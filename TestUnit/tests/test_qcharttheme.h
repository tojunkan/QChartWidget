// test_qcharttheme.h —— QChartTheme 单元测试声明
#pragma once
#include <QObject>

class TestQChartTheme : public QObject {
    Q_OBJECT
private slots:
    void lightDark_frameworkColorsDiffer();  // light/dark 框架色两两互异
    void seriesPalette_nonEmpty();           // seriesPalette 非空且提亮
    void axis_overridePriority();            // 轴：setColor→切主题保留→clearColor 回默认
    void grid_overridePriority();            // 网格：同构 override 解析
    void series_overridePriority();          // 系列：同构 override 解析
    void widget_themeResolution();           // 切 Dark 后轴/网格/背景解析正确
    void followSystemPalette_switch();       // 默认 false；注入假信号切换 Dark/Light
    void seriesPalette_cycle();              // A5 循环取色 palette[0..2,0]
    void seriesPalette_explicitPriority();   // 显式色不占位、切主题不被覆盖
};
