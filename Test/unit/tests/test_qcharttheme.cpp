// test_qcharttheme.cpp —— QChartTheme 单元测试
// 覆盖：light/dark 框架色两两互异、textColor 默认=axisColor、seriesPalette 非空且提亮、
//       三处 override 双槽解析（axis / grid / series）、widget 主题推送与系统跟随。
#include <QtTest>
#include <optional>
#include <QApplication>
#include <QPalette>
#include "QChartTheme.h"
#include "QChartWidget.h"
#include "QValueAxis.h"
#include "QChartLayer.h"
#include "QScatterSeries.h"
#include "test_qcharttheme.h"

// ===== light/dark 的框架色两两互异 =====
void TestQChartTheme::lightDark_frameworkColorsDiffer() {
    const QChartTheme l = QChartTheme::light();
    const QChartTheme d = QChartTheme::dark();

    // light 内部：bg / grid / axis 两两互异
    QVERIFY(l.backgroundColor != l.gridColor);
    QVERIFY(l.backgroundColor != l.axisColor);
    QVERIFY(l.gridColor != l.axisColor);
    // textColor 默认 = axisColor
    QVERIFY(l.textColor == l.axisColor);

    // dark 内部：bg / grid / axis 两两互异
    QVERIFY(d.backgroundColor != d.gridColor);
    QVERIFY(d.backgroundColor != d.axisColor);
    QVERIFY(d.gridColor != d.axisColor);
    QVERIFY(d.textColor == d.axisColor);

    // light vs dark：框架色两两不同
    QVERIFY(l.backgroundColor != d.backgroundColor);
    QVERIFY(l.gridColor != d.gridColor);
    QVERIFY(l.axisColor != d.axisColor);
    QVERIFY(l.textColor != d.textColor);
}

// ===== seriesPalette 非空（dark 为同色系提亮版）=====
void TestQChartTheme::seriesPalette_nonEmpty() {
    const QChartTheme l = QChartTheme::light();
    const QChartTheme d = QChartTheme::dark();

    QVERIFY(!l.seriesPalette.isEmpty());
    QVERIFY(!d.seriesPalette.isEmpty());
    QVERIFY(l.seriesPalette.size() == d.seriesPalette.size());

    // dark 是 light 的同色系提亮版 → 同一位置颜色应不同（提亮生效）
    for (int i = 0; i < l.seriesPalette.size(); ++i)
        QVERIFY(l.seriesPalette[i] != d.seriesPalette[i]);
}

// ===== 轴：显式设色优先（A3），切主题保留，clearColor 回主题默认 =====
void TestQChartTheme::axis_overridePriority() {
    QValueAxis axis;
    const QChartTheme dark = QChartTheme::dark();

    // 无 override → 主题默认生效
    axis.setThemeColor(dark.axisColor);
    QVERIFY(axis.color() == dark.axisColor);
    QVERIFY(!axis.colorOverride().has_value());

    // 显式设色 → 写 override
    axis.setColor(Qt::red);
    QVERIFY(axis.color() == Qt::red);
    QVERIFY(axis.colorOverride().has_value() && *axis.colorOverride() == Qt::red);

    // 切主题（setThemeColor）→ override 仍优先
    axis.setThemeColor(Qt::yellow);
    QVERIFY(axis.color() == Qt::red);

    // clearColor → 回主题默认
    axis.clearColor();
    QVERIFY(axis.color() == Qt::yellow);
    QVERIFY(!axis.colorOverride().has_value());
}

// ===== 网格：override 解析同构 =====
void TestQChartTheme::grid_overridePriority() {
    QChartLayer layer;
    const QChartTheme dark = QChartTheme::dark();

    layer.setThemeGridColor(dark.gridColor);
    QVERIFY(layer.gridColor() == dark.gridColor);
    QVERIFY(!layer.gridColorOverride().has_value());

    layer.setGridColor(Qt::blue);
    QVERIFY(layer.gridColor() == Qt::blue);
    QVERIFY(layer.gridColorOverride().has_value() && *layer.gridColorOverride() == Qt::blue);

    layer.setThemeGridColor(Qt::cyan);   // 切主题
    QVERIFY(layer.gridColor() == Qt::blue);  // override 保留

    layer.clearGridColor();
    QVERIFY(layer.gridColor() == Qt::cyan);
    QVERIFY(!layer.gridColorOverride().has_value());
}

// ===== 系列：override 解析同构（Q_PROPERTY color WRITE setColor 仍可用）=====
void TestQChartTheme::series_overridePriority() {
    QScatterSeries series;
    const QChartTheme dark = QChartTheme::dark();

    series.setThemeColor(dark.seriesPalette[0]);
    QVERIFY(series.color() == dark.seriesPalette[0]);
    QVERIFY(!series.colorOverride().has_value());

    series.setColor(Qt::green);          // QPropertyAnimation 目标：写 override
    QVERIFY(series.color() == Qt::green);
    QVERIFY(series.colorOverride().has_value() && *series.colorOverride() == Qt::green);

    series.setThemeColor(dark.seriesPalette[1]);  // 切主题
    QVERIFY(series.color() == Qt::green);          // override 保留

    series.clearColor();
    QVERIFY(series.color() == dark.seriesPalette[1]);
    QVERIFY(!series.colorOverride().has_value());
}

// ===== QChartWidget：切 Dark 后轴/网格/背景解析正确（push 模型）=====
void TestQChartTheme::widget_themeResolution() {
    QChartWidget w;
    const QChartTheme dark = QChartTheme::dark();
    const QChartTheme light = QChartTheme::light();

    // 默认主题 = Light
    QVERIFY(w.theme().backgroundColor == light.backgroundColor);
    QVERIFY(w.backgroundColor() == light.backgroundColor);

    // 切 Dark → 背景解析更新
    w.setTheme(QChartTheme::Preset::Dark);
    QVERIFY(w.theme().backgroundColor == dark.backgroundColor);
    QVERIFY(w.backgroundColor() == dark.backgroundColor);

    // addLayer/addAxis 补推 Dark 默认色
    auto* layer = new QChartLayer();
    w.addLayer(layer);
    auto* xAxis = new QValueAxis(nullptr, Qt::AlignBottom);
    w.addAxis(xAxis);

    QVERIFY(xAxis->color() == dark.axisColor);
    QVERIFY(layer->gridColor() == dark.gridColor);

    // 背景 override：显式设色优先，clear 回主题默认
    w.setBackgroundColor(Qt::red);
    QVERIFY(w.backgroundColor() == Qt::red);
    w.clearBackgroundColor();
    QVERIFY(w.backgroundColor() == dark.backgroundColor);
}

// ===== followSystemPalette：默认 false；注入假信号切换 Dark/Light =====
void TestQChartTheme::followSystemPalette_switch() {
    QChartWidget w;
    QVERIFY(!w.followSystemPalette());   // 默认关

    w.setFollowSystemPalette(true);
    QVERIFY(w.followSystemPalette());

    // 注入深色调色板 → ApplicationPaletteChange → 切 Dark
    QPalette darkPal;
    darkPal.setColor(QPalette::Window, QColor("#1E1E1E"));
    QApplication::setPalette(darkPal);
    QCoreApplication::processEvents();   // ApplicationPaletteChange 为排队事件，需 flush
    QVERIFY(w.theme().backgroundColor == QChartTheme::dark().backgroundColor);

    // 注入浅色调色板 → 切 Light
    QPalette lightPal;
    lightPal.setColor(QPalette::Window, QColor("#FFFFFF"));
    QApplication::setPalette(lightPal);
    QCoreApplication::processEvents();
    QVERIFY(w.theme().backgroundColor == QChartTheme::light().backgroundColor);

    w.setFollowSystemPalette(false);
    QVERIFY(!w.followSystemPalette());
}

// ===== A5：调色板循环取色（4 个未设色 series → palette[0,1,2,0]）=====
void TestQChartTheme::seriesPalette_cycle() {
    QChartWidget w;
    // 用 3 色调色板便于验证循环回绕
    QChartTheme theme = QChartTheme::light();
    theme.seriesPalette = { QColor("#111111"), QColor("#222222"), QColor("#333333") };
    w.setTheme(theme);

    auto* layer = new QChartLayer();
    w.addLayer(layer);

    QScatterSeries* s0 = new QScatterSeries();
    QScatterSeries* s1 = new QScatterSeries();
    QScatterSeries* s2 = new QScatterSeries();
    QScatterSeries* s3 = new QScatterSeries();
    layer->addSeries(s0);
    layer->addSeries(s1);
    layer->addSeries(s2);
    layer->addSeries(s3);

    QVERIFY(s0->color() == theme.seriesPalette[0]);
    QVERIFY(s1->color() == theme.seriesPalette[1]);
    QVERIFY(s2->color() == theme.seriesPalette[2]);
    QVERIFY(s3->color() == theme.seriesPalette[0]);   // 循环回 0
}

// ===== A5：显式 setColor 的 series 不占位、切主题不被覆盖 =====
void TestQChartTheme::seriesPalette_explicitPriority() {
    QChartWidget w;
    QChartTheme theme = QChartTheme::light();
    theme.seriesPalette = { QColor("#111111"), QColor("#222222") };
    w.setTheme(theme);

    auto* layer = new QChartLayer();
    w.addLayer(layer);

    // 显式设色的 series：加入时已有 override → 不占调色板槽位
    QScatterSeries* explicitSeries = new QScatterSeries();
    explicitSeries->setColor(Qt::red);
    layer->addSeries(explicitSeries);

    // 未设色 series 应拿到 palette[0]（显式色未占位）
    QScatterSeries* autoSeries = new QScatterSeries();
    layer->addSeries(autoSeries);
    QVERIFY(autoSeries->color() == theme.seriesPalette[0]);

    // 切 Dark → 显式色不被覆盖；自动色按 add 顺序重推为 Dark palette[0]
    w.setTheme(QChartTheme::Preset::Dark);
    QVERIFY(explicitSeries->color() == Qt::red);
    QVERIFY(autoSeries->color() == w.theme().seriesPalette[0]);
}
