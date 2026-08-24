// QChartTheme.h —— 主题/调色板（Phase 1）
// 纯数据，无 Q_OBJECT。主题 = 框架色 + 系列调色板（决策 A1）。
// 主题只当默认值：显式设色优先（决策 A3），override 双槽模型见 design_theme.md §2。
// 系列调色板按 addSeries 顺序循环取色（决策 A5）。
#ifndef QCHARTTHEME_H
#define QCHARTTHEME_H

#include <QColor>
#include <QVector>

struct QChartTheme {
    // 预设枚举（并入 QChartTheme，无裸 ChartTheme 名）
    enum class Preset { Light, Dark };

    QColor backgroundColor;         // 画布底色（整控件矩形）；plotArea 跟随此色，无独立填充
    QColor gridColor;
    QColor axisColor;               // 轴线 + 刻度 + 刻度标签
    QColor textColor;               // 图例文字（默认 = axisColor）
    QVector<QColor> seriesPalette;  // A5：系列循环取色

    static QChartTheme light();
    static QChartTheme dark();
};

#endif // QCHARTTHEME_H
