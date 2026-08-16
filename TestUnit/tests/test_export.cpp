// test_export.cpp —— 导出（PNG/SVG/PDF）单元测试
// 导出产物写入系统临时目录，不污染源码树。
#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QFile>
#include <QDir>
#include <QLoggingCategory>
#include "../../QChartWidget.h"
#include "../../QChartLayer.h"
#include "../../QValueAxis.h"
#include "../../QScatterSeries.h"
#include "test_export.h"

namespace {
// 构造一个有内容的 widget：Cartesian + 两个边框轴 + 一个散点系列
struct Fixture {
    QChartWidget w;
    Fixture() {
        auto* layer = new QChartLayer();
        w.addLayer(layer);                       // 创建 projection
        auto* xAxis = new QValueAxis(nullptr, Qt::AlignBottom);
        auto* yAxis = new QValueAxis(nullptr, Qt::AlignLeft);
        w.addAxis(xAxis);
        w.addAxis(yAxis);
        layer->setAxisX(xAxis);
        layer->setAxisY(yAxis);

        auto* s = new QScatterSeries("pts");
        s->setColor(Qt::red);
        s->append(2, 2);
        s->append(8, 8);
        layer->addSeries(s);

        w.resize(400, 300);
        w.show();
        QCoreApplication::processEvents();
    }
};

QString tmpPath(const QString& name) {
    return QDir::temp().filePath(name);
}
} // namespace

// ===== PNG 尺寸/背景（C4：size*dpr；C5：默认背景填充）=====
void TestExport::png_sizeAndBackground() {
    Fixture fx;
    const QString path = tmpPath("qchart_export_png_size.png");
    QVERIFY(fx.w.saveAsPng(path, QSize(320, 240), 2.0));

    QImage img(path);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(640, 480));               // 320x240 * 2.0

    // 角落像素 == backgroundColor（light 主题 #FFFFFF，WholeWidget 整设备填充）
    QCOMPARE(img.pixel(0, 0), QColor("#FFFFFF").rgba());

    // 非背景像素存在（系列/轴/文字确实画了）
    int nonBg = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.pixel(x, y) != QColor("#FFFFFF").rgba()) ++nonBg;
    QVERIFY(nonBg > 0);
}

// ===== 默认 WholeWidget = 控件尺寸；显式 PlotArea = plotArea 尺寸 =====
void TestExport::png_defaultAndPlotAreaScope() {
    Fixture fx;

    const QString whole = tmpPath("qchart_export_png_whole.png");
    QVERIFY(fx.w.saveAsPng(whole));                      // 默认 WholeWidget
    QImage imgWhole(whole);
    QCOMPARE(imgWhole.size(), QSize(400, 300));          // 控件尺寸

    const QString plot = tmpPath("qchart_export_png_plot.png");
    QVERIFY(fx.w.saveAsPng(plot, QChartExportScope::PlotArea));
    QImage imgPlot(plot);
    QCOMPARE(imgPlot.size(), fx.w.plotArea().size().toSize());  // plotArea 尺寸
}

// ===== SVG 真矢量：含 <svg>/<path>，不含 <image>/base64 =====
void TestExport::svg_isVector() {
    Fixture fx;
    const QString path = tmpPath("qchart_export.svg");
    QVERIFY(fx.w.saveAsSvg(path));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());

    QVERIFY(text.contains("<svg"));
    QVERIFY(text.contains("<path"));
    QVERIFY(!text.contains("<image"));
    QVERIFY(!text.contains("base64"));
}

// ===== PDF 以 %PDF- 开头 =====
void TestExport::pdf_header() {
    Fixture fx;
    const QString path = tmpPath("qchart_export.pdf");
    QVERIFY(fx.w.saveAsPdf(path));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray head = f.read(5);
    QCOMPARE(head, QByteArray("%PDF-"));
}

// ===== 透明背景开关（C5）：PNG 角落 alpha==0 =====
void TestExport::transparentBackground() {
    Fixture fx;
    fx.w.setExportTransparentBackground(true);
    const QString path = tmpPath("qchart_export_transparent.png");
    QVERIFY(fx.w.saveAsPng(path));

    QImage img(path);
    QVERIFY(!img.isNull());
    QCOMPARE(qAlpha(img.pixel(0, 0)), 0);   // 背景置 invalid → 不填充 → 透明
}

// ===== 调试黄框不泄漏进导出：PlotArea 顶部边框 == backgroundColor（非黄 #FFFF00）=====
void TestExport::debugYellowBoxNotInExport() {
    QChartWidget w;   // 无 axes/layers/投影 → 纯背景，避免轴/网格混淆边框
    QLoggingCategory::setFilterRules(QStringLiteral("chart.widget.debug=true"));

    const QString path = tmpPath("qchart_export_plot_debug.png");
    QVERIFY(w.saveAsPng(path, QChartExportScope::PlotArea, QSize(200, 150)));

    QImage img(path);
    QVERIFY(!img.isNull());
    // PlotArea 场景 plotArea=QRectF(0,0,w,h)。顶部边框中点无轴/图例/网格：
    // 若调试黄框泄漏，此处会变黄；修复后应为 backgroundColor(#FFFFFF)。
    QCOMPARE(img.pixel(img.width() / 2, 0), QColor("#FFFFFF").rgba());

    QLoggingCategory::setFilterRules(QString());   // 恢复默认
}

// ===== PDF 忽略透明开关：transparent 开/关两次导出内容一致（仅 CreationDate 不同）=====
void TestExport::pdf_transparentIgnoresSwitch() {
    Fixture fx;
    fx.w.setTheme(QChartTheme::Preset::Dark);

    fx.w.setExportTransparentBackground(false);
    const QString off = tmpPath("qchart_export_pdf_off.pdf");
    QVERIFY(fx.w.saveAsPdf(off));

    fx.w.setExportTransparentBackground(true);
    const QString on = tmpPath("qchart_export_pdf_on.pdf");
    QVERIFY(fx.w.saveAsPdf(on));

    auto readStrip = [](const QString& path) -> QByteArray {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        QByteArray d = f.readAll();
        // 去掉时间戳字段（内容唯一差异）
        int i = d.indexOf("/CreationDate");
        if (i >= 0) {
            int j = d.indexOf(')', i);
            if (j >= 0) d.remove(i, j - i + 1);
        }
        return d;
    };

    QCOMPARE(readStrip(on), readStrip(off));   // PDF 始终填背景，透明开关无影响
}

// ===== 非法路径 → SVG/PDF 返回 false（与 PNG 一致）=====
void TestExport::svgPdf_writeFailure() {
    Fixture fx;
    const QString badSvg = "/nonexistent_dir_qchart/out.svg";
    const QString badPdf = "/nonexistent_dir_qchart/out.pdf";
    QVERIFY(!fx.w.saveAsSvg(badSvg));
    QVERIFY(!fx.w.saveAsPdf(badPdf));
}
