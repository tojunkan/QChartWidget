// bench_main.cpp —— QChartBench 性能基准（console，无 Q_OBJECT）
// 方法学：QElapsedTimer + 预热 1 次 + 20 次迭代，输出 min/median/avg/max。
// 结果写 build 目录 bench_results.csv（列：bench,env,toolchain,N,culling,metric,min_ms,median_ms,avg_ms,max_ms），同时打印 stdout。
#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QList>
#include <QLoggingCategory>
#include <QSysInfo>
#include <QtMath>
#include <algorithm>
#include <functional>
#include <memory>

#include "QChartWidget.h"
#include "QPainterChartRenderer.h"
#include "QValueAxis.h"
#include "QChartLayer.h"
#include "QChartProjectionFactory.h"
#include "QChartProjection.h"
#include "QLineSeries.h"
#include "QScatterSeries.h"
#include "QBarSeries.h"

// ---------- 统计 ----------
struct Stats {
    double min = 0.0, median = 0.0, avg = 0.0, max = 0.0;
};

static Stats statsOf(QVector<double> t) {
    std::sort(t.begin(), t.end());
    Stats s;
    s.min = t.first();
    s.max = t.last();
    double sum = 0.0;
    for (double v : t) sum += v;
    s.avg = sum / t.size();
    const int n = t.size();
    s.median = (n % 2) ? t[n / 2] : (t[n / 2 - 1] + t[n / 2]) / 2.0;
    return s;
}

static Stats runBench(const std::function<void()>& fn, int iters = 20) {
    fn();   // 预热一次
    QVector<double> ms;
    ms.reserve(iters);
    for (int i = 0; i < iters; ++i) {
        QElapsedTimer timer;
        timer.start();
        fn();
        ms.append(timer.nsecsElapsed() / 1e6);   // ms
    }
    return statsOf(ms);
}

// ---------- 环境识别 ----------
static QString toolchainName() {
#if defined(__clang__)
    return QStringLiteral("Clang");
#elif defined(__GNUC__)
    return QStringLiteral("GCC");
#elif defined(_MSC_VER)
    return QStringLiteral("MSVC");
#elif defined(__MINGW32__)
    return QStringLiteral("MinGW");
#else
    return QStringLiteral("Unknown");
#endif
}

static QString compilerLower() {
#if defined(__clang__)
    return QStringLiteral("clang");
#elif defined(__GNUC__)
    return QStringLiteral("gcc");
#elif defined(_MSC_VER)
    return QStringLiteral("msvc");
#elif defined(__MINGW32__)
    return QStringLiteral("mingw");
#else
    return QStringLiteral("unknown");
#endif
}

// 运行时识别：kernel|platform|compiler（例：linux|offscreen|gcc）
static QString envName() {
    return QStringLiteral("%1|%2|%3")
        .arg(QSysInfo::kernelType(),
             QGuiApplication::platformName(),
             compilerLower());
}

// ---------- 输出 ----------
static QTextStream g_csv;
static QTextStream g_out(stdout);

static void emitRow(const QString& bench, const QString& n, const QString& culling,
                    const QString& metric, const Stats& s) {
    const QString tool = toolchainName() + "-Qt" + qVersion();
    const QString env = envName();
    g_csv << bench << ',' << env << ',' << tool << ',' << n << ',' << culling << ','
          << metric << ',' << s.min << ',' << s.median << ',' << s.avg << ',' << s.max << '\n';
    g_out << bench << ' ' << n << ' ' << culling << ' ' << metric
          << ": min=" << s.min << " median=" << s.median
          << " avg=" << s.avg << " max=" << s.max << " ms\n";
    g_csv.flush();
    g_out.flush();
}

// ---------- 场景构建 ----------
struct ScenePack {
    std::unique_ptr<QChartProjection> proj;
    std::unique_ptr<QValueAxis> xAxis;
    std::unique_ptr<QValueAxis> yAxis;
    std::unique_ptr<QChartLayer> layer;
    QChartSeries* series = nullptr;   // 由 layer 拥有（非持有）
    QChartScene scene;
};

static ScenePack makeBase(const QRectF& viewRect) {
    ScenePack p;
    p.proj = QChartProjectionFactory::create(CoordinateSystem::Cartesian);
    p.xAxis = std::make_unique<QValueAxis>(nullptr, Qt::AlignBottom);
    p.yAxis = std::make_unique<QValueAxis>(nullptr, Qt::AlignLeft);
    p.layer = std::make_unique<QChartLayer>();
    p.layer->setAxisX(p.xAxis.get());
    p.layer->setAxisY(p.yAxis.get());
    p.scene.plotArea = QRectF(0, 0, 800, 600);
    p.scene.projection = p.proj.get();
    p.scene.axes = QList<QChartAxis*>{ p.xAxis.get(), p.yAxis.get() };
    p.scene.layers = QList<QChartLayer*>{ p.layer.get() };
    p.scene.viewRect = viewRect;
    p.scene.dataBounds = viewRect;   // Cartesian 恒等
    return p;
}

static ScenePack makeLine(int N, bool zoomed, bool culling) {
    const qreal half = 1.2;
    const QRectF viewRect = zoomed
        ? QRectF(N / 2.0 - 500.0, -half, 1000.0, 2.0 * half)   // ~1000 点可见
        : QRectF(0.0, -half, N, 2.0 * half);                    // 全览
    ScenePack p = makeBase(viewRect);
    auto* line = new QLineSeries("line");
    line->setCullingEnabled(culling);
    line->setLineWidth(1.0);
    for (int i = 0; i < N; ++i) {
        const qreal x = i;
        const qreal y = qSin(x * 0.02) * 0.5 + qSin(x * 0.007) * 0.5;
        line->append(x, y);
    }
    p.layer->addSeries(line);
    p.series = line;
    return p;
}

static ScenePack makeScatter(int N, bool zoomed) {
    const qreal half = 1.2;
    const QRectF viewRect = zoomed
        ? QRectF(N / 2.0 - 500.0, -half, 1000.0, 2.0 * half)
        : QRectF(0.0, -half, N, 2.0 * half);
    ScenePack p = makeBase(viewRect);
    auto* sc = new QScatterSeries("scatter");
    sc->setMarkerSize(4);
    for (int i = 0; i < N; ++i) {
        const qreal x = i;
        const qreal y = qSin(x * 0.02) * 0.5 + qCos(x * 0.013) * 0.5;
        sc->append(x, y);
    }
    p.layer->addSeries(sc);
    p.series = sc;
    return p;
}

static ScenePack makeBar(int bars, bool zoomed) {
    const qreal half = 1.2;
    const QRectF viewRect = zoomed
        ? QRectF(bars / 2.0 - 100.0, -half, 200.0, 2.0 * half)
        : QRectF(0.0, -half, bars, 2.0 * half);
    ScenePack p = makeBase(viewRect);
    auto* bar = new QBarSeries("bars");
    for (int i = 0; i < bars; ++i) {
        const qreal h = 0.3 + 0.6 * qAbs(qSin(i * 0.1));
        bar->append(i, 0.0, i + 1.0, h);
    }
    p.layer->addSeries(bar);
    p.series = bar;
    return p;
}

static ScenePack makeGrid(bool zoomed) {
    const qreal half = 1.2;
    const QRectF viewRect = zoomed
        ? QRectF(50000.0 - 500.0, -half, 1000.0, 2.0 * half)
        : QRectF(0.0, -half, 100000.0, 2.0 * half);
    return makeBase(viewRect);   // 只有轴 + 网格，无系列
}

// ---------- 各测量项 ----------
static void benchRender(ScenePack& pack, const QString& bench, const QString& n,
                        const QString& culling) {
    QPainterChartRenderer renderer;
    QImage img(800, 600, QImage::Format_ARGB32_Premultiplied);
    auto fn = [&]() { renderer.renderUncached(pack.scene, &img); };
    const Stats s = runBench(fn);
    emitRow(bench, n, culling, "render_ms", s);
}

static void benchCacheVsUncached() {
    ScenePack pack = makeLine(100000, /*zoomed=*/false, /*culling=*/true);
    QPainterChartRenderer renderer;
    QImage img(800, 600, QImage::Format_ARGB32_Premultiplied);

    // 缓存重建：缓存开启 + invalidate 强制重建（等价屏显 invalidate+grab 的渲染路径）
    renderer.setCachingEnabled(true);
    auto fnRebuild = [&]() {
        renderer.invalidateBackground();
        renderer.invalidateForeground();
        renderer.render(pack.scene, &img);
    };
    Stats sr = runBench(fnRebuild);
    emitRow("cache_rebuild", "100k", "on", "rebuild_ms", sr);

    // 无缓存：renderUncached 直绘
    auto fnUncached = [&]() { renderer.renderUncached(pack.scene, &img); };
    Stats su = runBench(fnUncached);
    emitRow("uncached", "100k", "on", "render_ms", su);
}

static void benchExport() {
    // PNG 1M 全览
    {
        QChartWidget w;
        auto* layer = new QChartLayer();
        w.addLayer(layer);
        auto* xAxis = new QValueAxis(nullptr, Qt::AlignBottom);
        auto* yAxis = new QValueAxis(nullptr, Qt::AlignLeft);
        w.addAxis(xAxis); w.addAxis(yAxis);
        layer->setAxisX(xAxis); layer->setAxisY(yAxis);
        auto* line = new QLineSeries("line");
        line->setLineWidth(1.0);
        const int N = 1000000;
        for (int i = 0; i < N; ++i)
            line->append(i, qSin(i * 0.02) * 0.5 + qSin(i * 0.007) * 0.5);
        layer->addSeries(line);
        w.setViewRect(QRectF(0, -1.2, N, 2.4));
        w.resize(800, 600);
        auto fn = [&]() { w.saveAsPng("/tmp/qbench_export.png"); };
        Stats s = runBench(fn);
        emitRow("export_png", "1M", "-", "export_ms", s);
    }
    // SVG 100k / PDF 100k
    {
        QChartWidget w;
        auto* layer = new QChartLayer();
        w.addLayer(layer);
        auto* xAxis = new QValueAxis(nullptr, Qt::AlignBottom);
        auto* yAxis = new QValueAxis(nullptr, Qt::AlignLeft);
        w.addAxis(xAxis); w.addAxis(yAxis);
        layer->setAxisX(xAxis); layer->setAxisY(yAxis);
        auto* line = new QLineSeries("line");
        line->setLineWidth(1.0);
        const int N = 100000;
        for (int i = 0; i < N; ++i)
            line->append(i, qSin(i * 0.02) * 0.5 + qSin(i * 0.007) * 0.5);
        layer->addSeries(line);
        w.setViewRect(QRectF(0, -1.2, N, 2.4));
        w.resize(800, 600);
        auto fnSvg = [&]() { w.saveAsSvg("/tmp/qbench_export.svg"); };
        Stats ss = runBench(fnSvg);
        emitRow("export_svg", "100k", "-", "export_ms", ss);
        auto fnPdf = [&]() { w.saveAsPdf("/tmp/qbench_export.pdf"); };
        Stats sp = runBench(fnPdf);
        emitRow("export_pdf", "100k", "-", "export_ms", sp);
    }
}

// ---------- main ----------
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    Q_UNUSED(app);

    // 基准输出保持干净：静默 debug 日志
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));

    QFile csvFile(QStringLiteral("bench_results.csv"));
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fprintf(stderr, "无法打开 bench_results.csv\n");
        return 1;
    }
    g_csv.setDevice(&csvFile);
    g_csv << "bench,env,toolchain,N,culling,metric,min_ms,median_ms,avg_ms,max_ms\n";

    // 折线 N × culling × 视图
    for (int N : {10000, 100000, 1000000}) {
        for (bool culling : {true, false}) {
            for (bool zoomed : {false, true}) {
                ScenePack p = makeLine(N, zoomed, culling);
                const QString view = zoomed ? "zoom" : "full";
                benchRender(p, QString("line_%1").arg(view), QString::number(N),
                            culling ? "on" : "off");
            }
        }
    }
    // 散点 N × 视图（QScatterSeries 无显式裁剪，culling 列记 "-"）。
    // 注意：scatter_zoom 明显快于 scatter_full 并非 QChart 的 culling，而是
    // drawForeground 里 setClipRect(plotArea) 的 QPainter clip 隐式拒掉屏外 marker；
    // 因此该收益来自 QPainter 裁剪，不是 Series 级裁剪。
    for (int N : {10000, 100000}) {
        for (bool zoomed : {false, true}) {
            ScenePack p = makeScatter(N, zoomed);
            const QString view = zoomed ? "zoom" : "full";
            benchRender(p, QString("scatter_%1").arg(view), QString::number(N), "-");
        }
    }
    fprintf(stdout,
            "[注] scatter_zoom 快于 scatter_full 是 QPainter clip（drawForeground "
            "setClipRect(plotArea)）隐式拒掉屏外 marker 所致，并非 QChart 的 Series 级裁剪。\n");
    fflush(stdout);
    // 柱状 1000 × 视图
    for (bool zoomed : {false, true}) {
        ScenePack p = makeBar(1000, zoomed);
        const QString view = zoomed ? "zoom" : "full";
        benchRender(p, QString("bar_%1").arg(view), "1000", "-");
    }
    // 网格 × 视图
    for (bool zoomed : {false, true}) {
        ScenePack p = makeGrid(zoomed);
        const QString view = zoomed ? "zoom" : "full";
        benchRender(p, QString("grid_%1").arg(view), "-", "-");
    }
    // 缓存重建 vs 无缓存
    benchCacheVsUncached();
    // 导出
    benchExport();

    fprintf(stdout, "\nQChartBench 完成，结果写入 bench_results.csv\n");
    return 0;
}
