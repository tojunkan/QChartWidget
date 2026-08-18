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

// Phase 3 GL 场景（design_phase3.md §11，A8）
#include "QChartGL.h"
#include "QOpenGLChartRenderer.h"
#include "QChartLayer3D.h"
#include "QChartCamera3D.h"
#include "QChartScatterSeries3D.h"
#include "QChartCartesianProjection3D.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QOffscreenSurface>
#include <QCoreApplication>

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
                    const QString& metric, const Stats& s,
                    const QString& backend = QStringLiteral("qpaint")) {
    const QString tool = toolchainName() + "-Qt" + qVersion();
    const QString env = envName();
    g_csv << bench << ',' << env << ',' << tool << ',' << n << ',' << culling << ','
          << metric << ',' << s.min << ',' << s.median << ',' << s.avg << ',' << s.max
          << ',' << backend << '\n';   // backend 列（§11）：qpaint / gl
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

// ---------- GL 场景前置声明（定义在文件尾，§11）----------
static void benchGLVboUpload1M();
static bool benchGLRotate(int N, const QString& bench);
static void benchGLRss(const QString& bench, int N);

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
    g_csv << "bench,env,toolchain,N,culling,metric,min_ms,median_ms,avg_ms,max_ms,backend\n";

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

    // ---- Phase 3 GL 场景（§11；真实 GL 环境跑，offscreen/无 GL 跳过；会弹窗——已记录）----
    if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
        fprintf(stdout, "[GL 场景跳过] offscreen 平台无真实 GL（A8；wayland/xcb 手动跑）\n");
    } else {
        benchGLVboUpload1M();
        benchGLRotate(100000, "gl_rotate_100k");
        benchGLRotate(1000000, "gl_rotate_1M");
        benchGLRss("gl_rss_simple", 10000);
        benchGLRss("gl_rss_1M", 1000000);
    }

    fprintf(stdout, "\nQChartBench 完成，结果写入 bench_results.csv\n");
    return 0;
}

// ============================================================
// Phase 3 GL 场景（design_phase3.md §11；真实 GL 环境跑，offscreen/无 GL 跳过）
// ⚠ 会弹窗：gl_rotate/gl_rss 需 QOpenGLWidget 宿主（show 后初始化 GL 上下文），
//   窗口显示 <1s 即随场景结束隐藏——bench 窗口一闪即关属正常（t48 output 已记录）。
// ============================================================
static qint64 rssKB() {
#if defined(Q_OS_LINUX)
    // ⚠ 不能用 readLine/atEnd：procfs 文件 size()==0 → atEnd() 恒真 → 循环不执行（t48 实测）
    QFile f(QStringLiteral("/proc/self/status"));
    if (f.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> lines = f.readAll().split('\n');
        for (const QByteArray& line : lines)
            if (line.startsWith("VmRSS:"))
                return line.mid(6).trimmed().split(' ').first().toLongLong();
    }
#else
    // Windows：GetProcessMemoryInfo 属 t50 验收侧；此处返回 0（标记未测量）
#endif
    return 0;
}

/// gl_vbo_upload_1M：1M 点 World float3 批次（16B/顶点 = 16MB）glBufferData 耗时（ms）
static void benchGLVboUpload1M() {
    QChartGL::registerHost();
    if (!QChartGL::sharedContext()) {
        QChartGL::unregisterHost();
        fprintf(stderr, "gl_vbo_upload_1M: 无 GL 环境（offscreen？），跳过\n");
        return;
    }
    auto* surf = new QOffscreenSurface();
    surf->setFormat(QChartGL::surfaceFormat());
    surf->create();
    QOpenGLContext ctx;
    ctx.setFormat(QChartGL::surfaceFormat());
    if (!ctx.create() || !surf->isValid()) {
        delete surf;
        QChartGL::unregisterHost();
        fprintf(stderr, "gl_vbo_upload_1M: 上下文创建失败，跳过\n");
        return;
    }
    ctx.makeCurrent(surf);
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(&ctx);
    QVector<GLVertex> verts(1000000);   // 1M × 16B（A5 顶点布局）
    GLuint vbo = 0;
    f->glGenBuffers(1, &vbo);
    f->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    auto fn = [&]() {
        f->glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * sizeof(GLVertex)),
                        verts.constData(), GL_STATIC_DRAW);
    };
    const Stats s = runBench(fn, 10);
    emitRow("gl_vbo_upload_1M", "1M", "-", "upload_ms", s, "gl");
    f->glDeleteBuffers(1, &vbo);
    ctx.doneCurrent();
    delete surf;
    QChartGL::unregisterHost();
}

/// 3D 场景（N 散点，Cartesian3D）：float3 数值路径（t51）+ worldCache/VBO（t42/t44）
struct GLRotatePack {
    QChartLayer3D layer;
    QChartCamera3D cam;
    std::unique_ptr<QChartProjection3D> proj;
    QChartScene scene;
    QChartScatterSeries3D* series = nullptr;   // layer 拥有
    void build(int N) {
        proj = std::make_unique<QChartCartesianProjection3D>();
        layer.setProjection3D(proj.get());
        series = new QChartScatterSeries3D("s", &layer);
        series->setMarkerSize(2.0f);
        for (int i = 0; i < N; ++i) {
            // 确定性伪随机（黄金角分布）：(θ,φ) 均匀覆盖球面
            const qreal theta = i * 2.399963229728653;
            const qreal phi   = qAcos(1.0 - 2.0 * (i % 10000) / 10000.0) - M_PI_2;
            const qreal r = 50.0;
            series->append(r * qCos(theta) * qCos(phi),
                           r * qSin(theta) * qCos(phi),
                           r * qSin(phi));
        }
        layer.addSeries3D(series);
        cam.setViewCube(QChartWorldBox{ QVector3D(-60, -60, -60), QVector3D(60, 60, 60) });
        scene.plotArea = QRectF(0, 0, 800, 600);
        scene.backgroundColor = QColor(Qt::white);
        scene.camera3D = &cam;
        scene.layers3D.append(&layer);
    }
};

/// gl_rotate_N：相机旋转 200 帧中位帧耗时（排除首帧 shader 编译/批次构建，§10.2 ⚠）
static bool benchGLRotate(int N, const QString& bench) {
    QOpenGLWidget w;
    w.setFormat(QChartGL::surfaceFormat());
    w.resize(800, 600);
    w.show();
    for (int i = 0; i < 200 && !w.context(); ++i)
        QCoreApplication::processEvents();
    if (!w.context() || !w.context()->isValid()) {
        w.hide();
        fprintf(stderr, "%s: GL 上下文不可用（offscreen？），跳过\n", qPrintable(bench));
        return false;
    }
    w.makeCurrent();
    QOpenGLChartRenderer r(&w);
    r.initializeGL();                 // 首帧 shader 编译——不计时（A8 排除首帧）
    if (!r.isReady()) { w.doneCurrent(); w.hide(); fprintf(stderr, "%s: 渲染器未就绪，跳过\n", qPrintable(bench)); return false; }
    GLRotatePack pack;
    pack.build(N);
    r.paintGL(pack.scene);            // 首帧：建批次 + VBO 上传（不计时）

    QVector<double> ms;
    ms.reserve(200);
    for (int i = 0; i < 200; ++i) {
        pack.cam.orbit(0.5, 0.3);     // 每帧旋转（R6：viewCube 不动，orientation 转）
        QElapsedTimer t;
        t.start();
        r.paintGL(pack.scene);
        ms.append(t.nsecsElapsed() / 1e6);
    }
    emitRow(bench, QString::number(N), "-", "frame_ms", statsOf(ms), "gl");
    w.doneCurrent();
    w.hide();
    return true;
}

/// gl_rss_simple / gl_rss_1M：进程 RSS 增量（MB；Linux VmRSS；数据+worldCache+VBO 建好后测）
static void benchGLRss(const QString& bench, int N) {
    QOpenGLWidget w;
    w.setFormat(QChartGL::surfaceFormat());
    w.resize(800, 600);
    w.show();
    for (int i = 0; i < 200 && !w.context(); ++i)
        QCoreApplication::processEvents();
    if (!w.context() || !w.context()->isValid()) {
        w.hide();
        fprintf(stderr, "%s: GL 上下文不可用（offscreen？），跳过\n", qPrintable(bench));
        return;
    }
    w.makeCurrent();
    QOpenGLChartRenderer r(&w);
    r.initializeGL();
    GLRotatePack pack;
    pack.build(N);
    const qint64 before = rssKB();
    r.paintGL(pack.scene);            // 数据缓存 + worldCache + 批次 VBO 上传（内存增量发生点）
    const qint64 after = rssKB();
    w.doneCurrent();
    w.hide();
    const double deltaMB = (after - before) / 1024.0;
    const Stats s{ deltaMB, deltaMB, deltaMB, deltaMB };
    emitRow(bench, QString::number(N), "-", "rss_delta_mb", s, "gl");
    fprintf(stderr, "%s: RSS 增量 %+.1f MB（before=%lld KB after=%lld KB）\n",
            qPrintable(bench), deltaMB, before, after);
}
