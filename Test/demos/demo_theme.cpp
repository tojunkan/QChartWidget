// demo_theme.cpp —— 深色模式演示（Phase 1）
// setTheme(Dark) + 多系列（调色板循环取色）+ 图例可见 + 点击切换 + 启动即导出三格式
#include "demos.h"
#include "../../QChartWidget.h"
#include "../../QChartProjectionFactory.h"
#include "../../QChartLayer.h"
#include "../../QValueAxis.h"
#include "../../QLineSeries.h"
#include "../../QScatterSeries.h"
#include <QtMath>
#include <QDebug>

QChartWidget* buildDemoTheme() {
    qDebug() << "\n========== 深色模式演示 ==========";

    auto* w = new QChartWidget();
    w->setWindowTitle("深色模式 - 主题 + 图例 + 一键导出");
    w->setTheme(QChartTheme::Preset::Dark);

    auto* xAxis = new QValueAxis(w, Qt::AlignBottom);
    auto* yAxis = new QValueAxis(w, Qt::AlignLeft);
    w->addAxis(xAxis);
    w->addAxis(yAxis);
    xAxis->setRange(0, 10);
    yAxis->setRange(0, 10);

    auto* layer = new QChartLayer(w);
    layer->setAxisX(xAxis);
    layer->setAxisY(yAxis);
    w->addLayer(layer);

    // 多系列：不显式设色 → A5 调色板循环取色（展示主题调色板）
    auto* lineA = new QLineSeries("line-a", layer);
    auto* lineB = new QLineSeries("line-b", layer);
    auto* lineC = new QLineSeries("line-c", layer);
    auto* dots  = new QScatterSeries("dots", layer);
    for (int i = 0; i <= 10; ++i) {
        lineA->append(i, 2.0 + qSin(i * 0.8) * 1.5);
        lineB->append(i, 5.0 + qCos(i * 0.6) * 1.5);
        lineC->append(i, 8.0 + qSin(i * 0.4 + 1.0) * 1.0);
        dots->append(i, 5.0 + qSin(i * 0.9) * 2.0);
    }
    layer->addSeries(lineA);
    layer->addSeries(lineB);
    layer->addSeries(lineC);
    layer->addSeries(dots);

    // 图例可见（默认），点击图例项切换系列可见性（B4）
    w->setLegendVisible(true);

    w->resize(640, 480);

    // 启动即导出三格式到当前目录
    const QString png = "demo_export.png";
    const QString svg = "demo_export.svg";
    const QString pdf = "demo_export.pdf";
    if (w->saveAsPng(png)) qDebug() << "已导出 PNG:" << png;
    else                  qDebug() << "PNG 导出失败:" << png;
    if (w->saveAsSvg(svg)) qDebug() << "已导出 SVG:" << svg;
    else                   qDebug() << "SVG 导出失败:" << svg;
    if (w->saveAsPdf(pdf)) qDebug() << "已导出 PDF:" << pdf;
    else                   qDebug() << "PDF 导出失败:" << pdf;

    return w;
}
