// QChartTheme.cpp —— 主题/调色板预设实现
#include "QChartTheme.h"

namespace {
// matplotlib tab10 经典 10 色（顺序即循环取色顺序）
const QVector<QColor>& tab10Palette() {
    static const QVector<QColor> palette = {
        QColor("#1f77b4"),  // blue
        QColor("#ff7f0e"),  // orange
        QColor("#2ca02c"),  // green
        QColor("#d62728"),  // red
        QColor("#9467bd"),  // purple
        QColor("#8c564b"),  // brown
        QColor("#e377c2"),  // pink
        QColor("#7f7f7f"),  // gray
        QColor("#bcbd22"),  // olive
        QColor("#17becf"),  // cyan
    };
    return palette;
}
} // namespace

QChartTheme QChartTheme::light() {
    QChartTheme t;
    t.backgroundColor = QColor("#FFFFFF");
    t.gridColor       = QColor("#DCDCDC");
    t.axisColor       = QColor("#000000");
    t.textColor       = t.axisColor;
    t.seriesPalette   = tab10Palette();
    return t;
}

QChartTheme QChartTheme::dark() {
    QChartTheme t;
    t.backgroundColor = QColor("#1E1E1E");
    t.gridColor       = QColor("#3A3A3A");
    t.axisColor       = QColor("#E0E0E0");
    t.textColor       = t.axisColor;

    // 同色系提亮版：保证深底可读（决策：dark palette = tab10 提亮）
    t.seriesPalette = tab10Palette();
    for (QColor& c : t.seriesPalette)
        c = c.lighter(150);
    return t;
}
