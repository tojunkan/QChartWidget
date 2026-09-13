// test_widget_gl.cpp —— 批次 A widget GL 宿主冒烟（实窗验证）
// 覆盖：QChartWidget 切 OpenGL 后端 → plotArea 对齐 QOpenGLWidget 子控件（GL 只画 plotArea 内）→
//       host FBO readback 取证网格/轴出墨 + 宿主几何==plotArea（外部边框轴由外层画，区域互不重叠）。
#include "test_widget_gl.h"

#include <QtTest>
#include <QtMath>
#include <cmath>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QApplication>   // t74：真实鼠标/滚轮事件 + 事件循环驱动
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

#include "QChartWidget.h"
#include "QChartLayer.h"
#include "QChartCamera.h"   // t72：viewMatrix/project 朝向断言
#include "QValueAxis.h"
#include "QOpenGLChartRenderer.h"

namespace {
bool isInk(const QColor& c)
{
    // 网格默认色 (220,220,220)：离白 35 → 阈值 25 使浅灰网格也算墨
    return qAbs(c.red() - 255) > 25 || qAbs(c.green() - 255) > 25 || qAbs(c.blue() - 255) > 25;
}
int inkIn(const QImage& img, const QRect& r)
{
    int n = 0;
    const int x0 = qMax(0, r.left()), y0 = qMax(0, r.top());
    const int x1 = qMin(img.width() - 1, r.right()), y1 = qMin(img.height() - 1, r.bottom());
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}
} // namespace

void TestWidgetGl::initTestCase()
{
    if (QGuiApplication::platformName() == "offscreen")
        QSKIP("offscreen 平台无真实 GL：widget GL 冒烟跳过（wayland/xcb 实跑或 Windows 侧）");
}

void TestWidgetGl::glWidgetRenders()
{
    // t72：QtTest 有**全局**消息预算（默认 2000 条；超出后只打一行 "Maximum amount of warnings exceeded"
    // 并**静默跳过本测试函数剩余的断言**）。本套件之前的轴矩阵用例已消耗约 1.7k 条 → 本函数（断言最多、
    // 且含 t72 跨后端朝向断言与 4h-b 偏心回归）随时可能被截断。故在整个函数作用域内抑制高噪声的
    // chart.axis*/layer/camera 调试类别；不影响断言语义：QTest::ignoreMessage 匹配的是无类别的
    // qWarning（"No current OpenGL context!"）。函数退出（含 QVERIFY 提前 return）时由析构恢复默认规则。
    struct DebugSilencer {
        DebugSilencer()
        {
            QLoggingCategory::setFilterRules(QStringLiteral(
                "chart.axis=false\nchart.axis.value=false\nchart.axis.category=false\n"
                "chart.axis.datetime=false\nchart.axis.log=false\nchart.layer=false\nchart.camera=false"));
        }
        ~DebugSilencer() { QLoggingCategory::setFilterRules(QString()); }
    } t72DebugSilencer;

    QChartWidget w;
    QValueAxis ax(nullptr, Qt::AlignBottom);
    QValueAxis ay(nullptr, Qt::AlignLeft);
    ax.setRange(-10, 10); ax.setTickCount(5); ax.setColor(Qt::black);
    ay.setRange(-10, 10); ay.setTickCount(5); ay.setColor(Qt::black);

    QChartLayer layer;
    w.addAxis(&ax);
    w.addAxis(&ay);
    w.addLayer(&layer);
    w.resize(420, 340);

    w.setRenderBackend(QChartAbstractWidget::RenderBackend::OpenGL);
    w.show();
    if (!QTest::qWaitForWindowExposed(&w))
        QSKIP("无可用显示/窗口未暴露：GL widget 冒烟跳过（转用户 Windows 侧）");

    // 等首帧 + 布局（plotArea 计算并摆放宿主）
    for (int i = 0; i < 20; ++i) {
        QTest::qWait(20);
        if (w.plotArea().width() > 0) break;
    }
    QVERIFY2(w.plotArea().width() > 100 && w.plotArea().height() > 100,
             "布局后 plotArea 应有效");

    auto* host = qobject_cast<QOpenGLWidget*>(w.glHostWidget());
    QVERIFY2(host, "GL 后端应创建 plotArea 对齐的 QOpenGLWidget 宿主");
    QVERIFY2(host->isVisible(), "GL 宿主应可见");

    // 宿主几何 == plotArea（GL 只画 plotArea 内 → 与外层边框轴区域互不重叠/不遮挡）
    QCOMPARE(host->geometry(), w.plotArea().toRect());
    QVERIFY2(host->geometry().right() < w.width() && host->geometry().bottom() < w.height(),
             "宿主应位于 plotArea（外层留出边距画边框轴/标题）");

    // 环境记录（vendor/renderer）
    host->makeCurrent();
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(host->context());
    QVERIFY2(f, "GL 3.3 Core 函数不可用");
    const auto* v = reinterpret_cast<const char*>(f->glGetString(GL_VENDOR));
    const auto* r = reinterpret_cast<const char*>(f->glGetString(GL_RENDERER));
    const auto* ver = reinterpret_cast<const char*>(f->glGetString(GL_VERSION));
    QString vendorStr = QString::fromLatin1(v ? v : "unknown");
    qInfo().noquote() << QString("widget GL 环境: platform=%1 vendor=%2 renderer=%3 version=%4")
        .arg(QGuiApplication::platformName())
        .arg(v ? v : "?").arg(r ? r : "?").arg(ver ? ver : "?");
    host->doneCurrent();

    // 等若干帧让 paintGL 真正执行
    QTest::qWait(200);

    // host FBO 取证：网格/轴脊落屏（FBO 行序底→顶，翻转后与顶层行序一致）
    const QImage fbo = host->grabFramebuffer();
    QVERIFY2(!fbo.isNull(), "host grabFramebuffer 应成功");

    // ===== t13(HiDPI)：FBO 尺寸锁定 + dpr 感知采样比例 =====
    // FBO（默认帧缓冲）按设备像素分配：期望 == hostGeo(逻辑) × devicePixelRatioF（±1px 容差）。
    const int expFW = qRound(host->geometry().width() * host->devicePixelRatioF());
    const int expFH = qRound(host->geometry().height() * host->devicePixelRatioF());
    QVERIFY2(qAbs(fbo.width() - expFW) <= 1 && qAbs(fbo.height() - expFH) <= 1,
             qPrintable(QString("FBO 尺寸应≈hostGeo×dpr：fbo=%1x%2 hostGeo=%3x%4 dpr=%5 期望=%6x%7")
                        .arg(fbo.width()).arg(fbo.height())
                        .arg(host->geometry().width()).arg(host->geometry().height())
                        .arg(host->devicePixelRatioF()).arg(expFW).arg(expFH)));
    // 采样缩放（标签带等按逻辑坐标给定 → 设备像素）；DPR=1 时 s=1，采样矩形与现值逐位一致
    const double sX = double(fbo.width()) / double(qMax(1, host->geometry().width()));
    const double sY = double(fbo.height()) / double(qMax(1, host->geometry().height()));

    // ===== t12 诊断 =====
    // 环境/几何/刻度信息：无条件 QINFO（2-3 行，供 Windows 裸跑带回关键行，不污染断言）；
    // 逐环扫描/8 方向/全图 ink/PNG：QCHART_WIDGETGL_DUMP=1 门控。
    {
        const QRectF pa = w.plotArea();
        const QRectF db = w.dataBounds();
        const QRectF vr = layer.camera() ? layer.camera()->viewRect() : QRectF();
        const QVector<qreal> xt = ax.tickValues(db.left(), db.right());
        const QVector<qreal> yt = ay.tickValues(db.bottom(), db.top());
        QString xs, ys;
        for (qreal v : xt) xs += QString::number(v, 'g', 6) + " ";
        for (qreal v : yt) ys += QString::number(v, 'g', 6) + " ";
        qInfo().noquote() << "DUMP fbo" << fbo.width() << "x" << fbo.height()
                          << "dpr" << host->devicePixelRatioF()
                          << "hostGeo" << host->geometry();
        qInfo().noquote() << "DUMP plotArea" << pa << "dataBounds" << db << "viewRect" << vr;
        qInfo().noquote() << QString("DUMP xTicks(%1): %2").arg(xt.size()).arg(xs.trimmed());
        qInfo().noquote() << QString("DUMP yTicks(%1): %2").arg(yt.size()).arg(ys.trimmed());
        qInfo().noquote() << QString("DUMP axisX range=%1..%2 axisY range=%3..%4 gridVisible=%5")
            .arg(ax.min(), 0, 'g', 6).arg(ax.max(), 0, 'g', 6)
            .arg(ay.min(), 0, 'g', 6).arg(ay.max(), 0, 'g', 6)
            .arg(layer.isGridVisible());
    }

    if (qEnvironmentVariableIsSet("QCHART_WIDGETGL_DUMP")) {
        const QRectF pa = w.plotArea();
        const QRectF vr = layer.camera() ? layer.camera()->viewRect() : QRectF();

        // 中心逐环扫描：半径 1..48，环定义 max(|dx|,|dy|)==r（方形环），统计环内 ink
        const int cx = fbo.width() / 2, cy = fbo.height() / 2;
        int firstRing = -1;
        double firstAng = 0;
        for (int r = 1; r <= 48; ++r) {
            int n = 0;
            double angSum = 0;
            int angCnt = 0;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (qMax(qAbs(dx), qAbs(dy)) != r) continue;
                    const int px = cx + dx, py = cy + dy;
                    if (px < 0 || px >= fbo.width() || py < 0 || py >= fbo.height()) continue;
                    if (isInk(fbo.pixelColor(px, py))) {
                        ++n;
                        angSum += qAtan2(double(dy), double(dx));
                        ++angCnt;
                    }
                }
            }
            if (n > 0 && firstRing < 0) {
                firstRing = r;
                firstAng = (angCnt > 0) ? (angSum / angCnt) : 0.0;
            }
            qInfo().noquote() << QString("DUMP ring r=%1 ink=%2").arg(r).arg(n);
        }
        qInfo().noquote() << "DUMP firstInkRing" << firstRing
                          << "avgAngleDeg" << qRadiansToDegrees(firstAng);

        // 补丁(队长批准)：全图 ink 总计数（判整帧空白/部分内容）
        int fullInk = 0;
        for (int y = 0; y < fbo.height(); ++y)
            for (int x = 0; x < fbo.width(); ++x)
                if (isInk(fbo.pixelColor(x, y))) ++fullInk;
        qInfo().noquote() << "DUMP fullInk" << fullInk;

        // 补丁：8 方向（上/下/左/右 + 四角 45° 带）自中心出发的首个 ink 偏移（像素；48 内未遇 → -1）
        struct Dir { const char* name; int dx; int dy; };
        const Dir dirs[8] = {
            { "up", 0, -1 }, { "down", 0, 1 }, { "left", -1, 0 }, { "right", 1, 0 },
            { "ne", 1, -1 }, { "nw", -1, -1 }, { "se", 1, 1 }, { "sw", -1, 1 }
        };
        for (const Dir& d : dirs) {
            int hit = -1;
            for (int k = 1; k <= 48; ++k) {
                const int px = cx + d.dx * k, py = cy + d.dy * k;
                if (px < 0 || px >= fbo.width() || py < 0 || py >= fbo.height()) break;
                if (isInk(fbo.pixelColor(px, py))) { hit = k; break; }
            }
            qInfo().noquote() << QString("DUMP dir %1 firstInkOffset=%2").arg(d.name).arg(hit);
        }

        // 补丁：映射参考点——numeric (0,0) 与 plotArea 四角 numeric 端点经相机投影到像素
        if (layer.camera()) {
            const QChartCamera* cam = layer.camera();
            auto proj = [&](qreal x, qreal y) {
                return cam->project(QVector3D(x, y, 0.0f), pa).screen;
            };
            qInfo().noquote() << "DUMP projOrigin(numeric 0,0)=" << proj(0.0, 0.0)
                              << "plotArea=" << pa;
            qInfo().noquote() << "DUMP projCorner(lo,lo)=" << proj(vr.left(), vr.top())
                              << "projCorner(hi,hi)=" << proj(vr.right(), vr.bottom());
            qInfo().noquote() << "DUMP projCorner(lo,hi)=" << proj(vr.left(), vr.bottom())
                              << "projCorner(hi,lo)=" << proj(vr.right(), vr.top());
        }

        // 另存整帧 FBO PNG（工作目录 widgetgl_fbo_<vendor>.png，防多后端覆盖）
        QString safeVendor = vendorStr;
        safeVendor.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")),
                           QStringLiteral("_"));
        const QString pngName = QStringLiteral("widgetgl_fbo_%1.png").arg(safeVendor);
        const bool saved = fbo.save(pngName);
        qInfo().noquote() << "DUMP pngSaved" << saved << fbo.size() << pngName;
    }

    const QPoint center(fbo.width() / 2, fbo.height() / 2);
    QVERIFY2(inkIn(fbo, QRect(center.x() - 3, center.y() - 3, 7, 7)) > 0,
             "host FBO 中心应有网格墨迹（轴域 0,0 交叉）");
    QVERIFY2(inkIn(fbo, QRect(0, 0, fbo.width(), fbo.height())) > 400,
             "host FBO 应产生大量墨迹（网格+刻度）");

    // ===== t31 契约锁定：GL 纯 GPU 后端不渲染自由标签（二维网格脊标签在 GL 消失）=====
    // 二维网格脊标签（批次2 A）以**自由标签**提交（全 NaN 锚点 + refPrimitiveId=-1）；
    // 既定契约（批次1 过审）：GL cull 对 tier3 一律置不可见 → 标签层图为空（既定已接受差异）。
    {
        const QRectF pa = w.plotArea();
        const QSize sz(qRound(pa.width()), qRound(pa.height()));
        QChartScene probeFree = layer.scene();
        QVERIFY2(!probeFree.labels.isEmpty(), "二维网格脊标签应已收集（自由标签）");
        for (const QChartTextLabel& l : probeFree.labels)
            QVERIFY2(l.refPrimitiveId == -1 && std::isnan(l.numericAnchor.x()),
                     "前置条件：本场景标签应全部为自由标签");

        QImage imgFree(sz, QImage::Format_ARGB32_Premultiplied);
        imgFree.fill(Qt::transparent);
        if (QOpenGLContext* ctx = QOpenGLContext::currentContext()) ctx->doneCurrent();
        {
            QOpenGLChartRenderer r;
            QTest::ignoreMessage(QtWarningMsg, "No current OpenGL context!");
            r.render(probeFree, &imgFree);   // 步骤 2 cull + 步骤 4 标签覆盖层
        }
        int freeVisible = 0;
        for (const QChartTextLabel& l : probeFree.labels)
            if (l.visible) ++freeVisible;
        // 注意：透明图不能用 isInk(RGB) 计数（alpha=0 的像素 RGB 未定义会被误判为墨）
        int inkFree = 0;
        for (int y = 0; y < sz.height(); ++y)
            for (int x = 0; x < sz.width(); ++x)
                if (imgFree.pixelColor(x, y).alpha() > 0) ++inkFree;
        qInfo().noquote() << QString("widget GL 自由标签契约探针: labels=%1 visible=%2 ink=%3")
                                 .arg(probeFree.labels.size()).arg(freeVisible).arg(inkFree);
        QVERIFY2(freeVisible == 0, "GL 不渲染自由标签：cull 后自由标签应全部不可见（t31 契约）");
        QVERIFY2(inkFree == 0, "GL 不渲染自由标签：标签层图应为空（二维网格脊标签在 GL 消失）");
    }

    // ===== F1(t8) 标签 translate 回归守卫（t31/收尾：自建含显式坐标标签的合成场景）=====
    // GL 不再渲染自由标签 → 原"网格脊自由标签层"无内容；按 t30 结论改为**合成场景**：
    //   场景内自建 tier1 显式坐标标签（numericAnchor 非 NaN）+ 一个 tier2 指向图元标签
    //   （两类均在 GL 存活，与批次1 过审契约一致），清空收集到的自由标签，
    //   渲染到与绘图区对齐的设备（plotArea 尺寸透明图，等价 widget 的 plotArea 对齐 GL 宿主；
    //   无 GL context → 图元绘制跳过、只留标签覆盖层）；
    //   A = 父系 plotArea（topLeft != 0，translate(-plotArea.topLeft()) 生效）
    //   B = 同尺寸 (0,0) plotArea（translate 为零）
    //   两次渲染应产出**同构**标签层图（同一 local 坐标系）；translate 缺失时 A 整体
    //   偏移 +plotArea.topLeft()，掩膜差异巨大 → 判别力保留。
    {
        const QRectF pa = w.plotArea();
        const QSize sz(qRound(pa.width()), qRound(pa.height()));
        QChartScene probeA = layer.scene();
        QChartScene probeB = probeA;
        probeB.plotArea = QRectF(0, 0, pa.width(), pa.height());

        QVERIFY2(!probeA.primitives.isEmpty(), "场景应含图元（tier2 绑定目标）");
        // 清空收集到的自由标签（GL 不渲染，且避免污染掩膜对照）；自建两类 GL 存活标签：
        QChartTextLabel t1a;   // tier1 显式坐标（数值域原点 → 绘图区中心）
        t1a.text = QStringLiteral("t1");
        t1a.color = Qt::black;
        t1a.numericAnchor = QVector3D(0, 0, 0);
        QChartTextLabel t1b;   // tier1 显式坐标（偏移位置，覆盖不同排版方向）
        t1b.text = QStringLiteral("t1b");
        t1b.color = Qt::black;
        t1b.numericAnchor = QVector3D(-5, 4, 0);
        QChartTextLabel t2;    // tier2 指向图元（第 0 号图元）
        t2.text = QStringLiteral("t2");
        t2.color = Qt::black;
        t2.refPrimitiveId = 0;
        for (QChartScene* sc : {&probeA, &probeB}) {
            sc->labels.clear();
            sc->labels.append(t1a);
            sc->labels.append(t1b);
            sc->labels.append(t2);
        }

        QImage imgA(sz, QImage::Format_ARGB32_Premultiplied);
        QImage imgB(sz, QImage::Format_ARGB32_Premultiplied);
        imgA.fill(Qt::transparent);
        imgB.fill(Qt::transparent);

        if (QOpenGLContext* ctx = QOpenGLContext::currentContext()) ctx->doneCurrent();
        {
            QOpenGLChartRenderer rA;
            QTest::ignoreMessage(QtWarningMsg, "No current OpenGL context!");
            rA.render(probeA, &imgA);
        }
        if (QOpenGLContext* ctx = QOpenGLContext::currentContext()) ctx->doneCurrent();
        {
            QOpenGLChartRenderer rB;
            QTest::ignoreMessage(QtWarningMsg, "No current OpenGL context!");
            rB.render(probeB, &imgB);
        }

        int explicitVisible = 0, boundVisible = 0;
        for (const QChartTextLabel& l : probeA.labels) {
            if (!l.visible) continue;
            if (l.refPrimitiveId == -1) ++explicitVisible;   // tier1（自建场景内无自由标签）
            else ++boundVisible;                             // tier2
        }
        QVERIFY2(explicitVisible == 2, "GL tier1（显式坐标）标签应可见（t31：两类标签不受影响）");
        QVERIFY2(boundVisible == 1, "GL tier2（指向图元）标签应可见（t31：两类标签不受影响）");

        int inkA = 0, inkB = 0, diff = 0;
        for (int y = 0; y < sz.height(); ++y)
            for (int x = 0; x < sz.width(); ++x) {
                const bool a = imgA.pixelColor(x, y).alpha() > 0;
                const bool b = imgB.pixelColor(x, y).alpha() > 0;
                if (a) ++inkA;
                if (b) ++inkB;
                if (a != b) ++diff;
            }
        qInfo().noquote() << QString("widget GL 标签 translate 探针: tier1Visible=%1 tier2Visible=%2 "
                                     "inkA(parent)=%3 inkB(local)=%4 maskDiff=%5")
                                 .arg(explicitVisible).arg(boundVisible).arg(inkA).arg(inkB).arg(diff);
        QVERIFY2(inkA > 0, "父系 plotArea 下合成标签层应有字形墨迹（标签确实绘制）");
        QVERIFY2(inkB > 0, "local plotArea 对照应有字形墨迹");
        QVERIFY2(diff <= qMax(50, inkA / 50),
                 "父系(translate 生效)与 local 对照的标签掩膜应基本一致（translate 缺失时整体偏移）");
    }

    // ===== 4h-b：偏心 viewRect（中心 (120,-80)）——GL 墨迹 > 0 且与 CPU 同场景落位一致 =====
    // 场景内容中心 (120,-80)：x∈[100,140]、y∈[-110,-50]。修复前 viewMatrix=T·S（平移量被当缩放用）
    // ⇒ 几何整体出画、GL 宿主 plotArea 内墨迹为 0；修复后（M=S·T）应正常落屏。
    {
        struct EccentricScene {
            QChartWidget w;
            QValueAxis ax{nullptr, Qt::AlignBottom};
            QValueAxis ay{nullptr, Qt::AlignLeft};
            QChartLayer layer;
            explicit EccentricScene(bool gl)
            {
                ax.setRange(100.0, 140.0); ax.setTickCount(5); ax.setColor(Qt::black);
                ay.setRange(-110.0, -50.0); ay.setTickCount(5); ay.setColor(Qt::black);
                w.addAxis(&ax); w.addAxis(&ay); w.addLayer(&layer);
                layer.setGridVisible(true);
                layer.setGridColor(QColor(120, 120, 120));
                if (gl) w.setRenderBackend(QChartAbstractWidget::RenderBackend::OpenGL);
                w.resize(420, 340);
                w.show();
            }
        };
        auto inkOnRow = [](const QImage& img, int row) {
            if (row < 0 || row >= img.height()) return 0;
            int n = 0;
            for (int x = 0; x < img.width(); ++x)
                if (isInk(img.pixelColor(x, row))) ++n;
            return n;
        };

        // CPU 孪生（同场景、同尺寸）
        EccentricScene cs(false);
        QVERIFY2(QTest::qWaitForWindowExposed(&cs.w), "偏心场景 CPU 窗口应暴露");
        QTest::qWait(60);
        const QImage cpuImg = cs.w.grab().toImage();
        const double csScale = double(cpuImg.width()) / double(cs.w.width());
        const QRectF cpuPa = cs.w.plotArea();
        const QRectF cpuVr = cs.layer.camera()->viewRect();

        // GL 侧
        EccentricScene gs(true);
        QVERIFY2(QTest::qWaitForWindowExposed(&gs.w), "偏心场景 GL 窗口应暴露");
        for (int i = 0; i < 20; ++i) { QTest::qWait(20); if (gs.w.plotArea().width() > 0) break; }
        auto* host2 = qobject_cast<QOpenGLWidget*>(gs.w.glHostWidget());
        QVERIFY2(host2 && host2->isVisible(), "偏心场景应创建并显示 GL 宿主");
        QTest::qWait(200);
        const QImage eccFbo = host2->grabFramebuffer();
        QVERIFY2(!eccFbo.isNull(), "偏心场景 host grabFramebuffer 应成功");
        const double gsScale = double(eccFbo.width()) / double(qMax(1, host2->geometry().width()));
        const QRectF glPa = gs.w.plotArea();
        const QRectF glVr = gs.layer.camera()->viewRect();

        int eccInk = 0;
        for (int y = 0; y < eccFbo.height(); ++y)
            for (int x = 0; x < eccFbo.width(); ++x)
                if (isInk(eccFbo.pixelColor(x, y))) ++eccInk;
        int cpuInk = 0;
        for (int y = 0; y < cpuImg.height(); ++y)
            for (int x = 0; x < cpuImg.width(); ++x)
                if (isInk(cpuImg.pixelColor(x, y))) ++cpuInk;

        qInfo().noquote() << QString("4h-b 偏心视图: viewRect(cpu)=%1 viewRect(gl)=%2 cpuInk=%3 glInk=%4 fbo=%5x%6")
                                 .arg(QString("(%1,%2 %3x%4)").arg(cpuVr.left()).arg(cpuVr.top())
                                          .arg(cpuVr.width()).arg(cpuVr.height()))
                                 .arg(QString("(%1,%2 %3x%4)").arg(glVr.left()).arg(glVr.top())
                                          .arg(glVr.width()).arg(glVr.height()))
                                 .arg(cpuInk).arg(eccInk).arg(eccFbo.width()).arg(eccFbo.height());
        QVERIFY2(qAbs(cpuVr.center().x() - 120.0) < 1e-6 && qAbs(cpuVr.center().y() + 80.0) < 1e-6,
                 "偏心场景 viewRect 中心应为 (120,-80)");
        QVERIFY2(eccInk > 0,
                 qPrintable(QString("4h-b：偏心 viewRect 下 GL 宿主 plotArea 内应有墨迹（修复前为 0），实为 %1").arg(eccInk)));

        // 落位一致（同代数探针）：逐条 y 网格脊行，CPU 与 GL 均须在该行有墨，且行位置差 ≤ 3px
        const QVector<qreal> yTicks = cs.ay.tickValues(qMin(cpuVr.top(), cpuVr.bottom()),
                                                       qMax(cpuVr.top(), cpuVr.bottom()));
        int matched = 0;
        for (qreal v : yTicks) {
            // 均换算为「相对 plotArea 顶部」的逻辑行：CPU 抓图为整窗坐标、GL 宿主图恰为 plotArea 尺寸
            const double cpuLocalRow = cs.layer.camera()->project(QVector3D(0.0f, float(v), 0.0f), cpuPa).screen.y() - cpuPa.top();
            const double glLocalRow  = gs.layer.camera()->project(QVector3D(0.0f, float(v), 0.0f), glPa).screen.y() - glPa.top();
            // 边界脊（恰落 plotArea 上下缘者）不作比较（裁剪边界效应），只比内部脊
            const double localH = cpuPa.height();
            if (cpuLocalRow < 2.0 || cpuLocalRow > localH - 2.0) continue;
            // 行容差 ±1：抗锯齿/栅格差异
            auto inkNearRow = [&inkOnRow](const QImage& img, int row) {
                return inkOnRow(img, row - 1) + inkOnRow(img, row) + inkOnRow(img, row + 1);
            };
            const int cInk = inkNearRow(cpuImg, int(qRound(cpuLocalRow * csScale)));
            const int gInk = inkNearRow(eccFbo, int(qRound(glLocalRow * gsScale)));
            QVERIFY2(cInk > 0, qPrintable(QString("CPU 侧 y=%1（plotArea 内行 %2）应有网格脊墨迹").arg(v).arg(cpuLocalRow)));
            QVERIFY2(gInk > 0, qPrintable(QString("GL 侧 y=%1（plotArea 内行 %2）应有网格脊墨迹（同代数探针落位）").arg(v).arg(glLocalRow)));
            QVERIFY2(qAbs(cpuLocalRow - glLocalRow) <= 3.0,
                     qPrintable(QString("CPU/GL 同一条脊的行位置应一致：cpu=%1 gl=%2（逻辑 px）")
                                    .arg(cpuLocalRow).arg(glLocalRow)));
            ++matched;
        }
        qInfo().noquote() << QString("4h-b 偏心视图落位: yTicks=%1 条，CPU/GL 逐条对齐=%2（容差 3px）")
                                 .arg(yTicks.size()).arg(matched);
        QVERIFY2(matched >= 3, "内部网格脊（≥3 条）应逐条 CPU/GL 对齐");
    }

    // ===== t72：跨后端朝向（非对称夹具 · **整窗抓图**逐特征对位 · 动态同号单调）=====
    // 背景（t71 诊断）：2D viewMatrix 的 Y 缩放曾取负号 ⇒ GL 画面相对 CPU 关于 plotArea 中心**垂直镜像**。
    // 既有测试全漏的根因：①夹具刻度集关于视图中心对称（等差刻度镜像后仍是同一集合 ⇒“镜像≡平移”）；
    // ②GL 侧只做过墨迹计数/同代数探针（两边都调用 project()，从不读 GL 实测像素行）。
    // 本段：非对称夹具（range[−13,7]，刻度{−12,−8,−4,0,4}）+ 真实 widget 合成抓图（QWidget::grab，
    // 非裸 FBO）+ 逐特征行号对位 + 范围单调上移时“特征行号同号单调”。
    {
        struct AsymScene {
            QChartWidget w;
            QValueAxis ax{nullptr, Qt::AlignBottom};
            QValueAxis ay{nullptr, Qt::AlignLeft};
            QChartLayer layer;
            explicit AsymScene(bool gl)
            {
                ax.setRange(-13.0, 7.0); ax.setTickCount(5); ax.setColor(Qt::black);
                ay.setRange(-13.0, 7.0); ay.setTickCount(5); ay.setColor(Qt::black);
                w.addAxis(&ax); w.addAxis(&ay); w.addLayer(&layer);
                layer.setGridVisible(true);
                layer.setGridColor(QColor(120, 120, 120));   // 深灰网格（isInk 阈值 25 可判）
                if (gl) w.setRenderBackend(QChartAbstractWidget::RenderBackend::OpenGL);
                w.resize(420, 340);
                w.show();
            }
        };
        // 水平网格线行簇：plotArea 内“最长连续墨迹游程 ≥ 50% 绘图区宽”的行（文字游程远短于此）
        auto gridRowClusters = [](const QImage& img, const QRectF& pa, double s) {
            QVector<int> rows;
            const int y0 = qMax(0, int(std::floor(pa.top() * s)));
            const int y1 = qMin(img.height() - 1, int(std::ceil((pa.top() + pa.height()) * s)));
            const int x0 = qMax(0, int(std::floor(pa.left() * s)));
            const int x1 = qMin(img.width() - 1, int(std::ceil((pa.left() + pa.width()) * s)));
            const int need = int((x1 - x0 + 1) * 0.5);
            for (int y = y0; y <= y1; ++y) {
                int best = 0, run = 0;
                for (int x = x0; x <= x1; ++x) {
                    if (isInk(img.pixelColor(x, y))) { ++run; best = qMax(best, run); } else run = 0;
                }
                if (best >= need) rows.append(y);
            }
            QVector<qreal> centers;                 // 归并相邻行（抗锯齿允许 2px 间隙）为簇中心
            int start = -1, prev = -2;
            for (int r : rows) {
                if (start < 0) { start = r; prev = r; continue; }
                if (r - prev <= 2) { prev = r; continue; }
                centers.append((start + prev) / 2.0 / s - pa.top());
                start = r; prev = r;
            }
            if (start >= 0) centers.append((start + prev) / 2.0 / s - pa.top());
            return centers;
        };

        AsymScene cs(false);                       // CPU 孪生
        QVERIFY2(QTest::qWaitForWindowExposed(&cs.w), "非对称场景 CPU 窗口应暴露");
        QTest::qWait(80);
        AsymScene gs(true);                        // GL
        QVERIFY2(QTest::qWaitForWindowExposed(&gs.w), "非对称场景 GL 窗口应暴露");
        for (int i = 0; i < 20; ++i) { QTest::qWait(20); if (gs.w.plotArea().width() > 0) break; }
        auto* hostA = qobject_cast<QOpenGLWidget*>(gs.w.glHostWidget());
        QVERIFY2(hostA && hostA->isVisible(), "非对称场景应创建并显示 GL 宿主");
        QTest::qWait(200);

        const QRectF paA = cs.w.plotArea();
        const QRectF paG = gs.w.plotArea();
        QVERIFY2(paA.width() > 100.0 && paA.height() > 100.0, "CPU plotArea 应有效");
        QVERIFY2(qAbs(paA.x() - paG.x()) < 1e-6 && qAbs(paA.y() - paG.y()) < 1e-6
                     && qAbs(paA.width() - paG.width()) < 1e-6 && qAbs(paA.height() - paG.height()) < 1e-6,
                 "CPU/GL 孪生场景 plotArea 应一致");

        // ① 夹具自检：刻度行集**不关于视图中心对称**（防“镜像≡平移”自欺夹具）
        {
            const QChartCamera* camA = cs.layer.camera();
            QVERIFY2(camA, "CPU 场景相机应有效");
            const QRectF vr = camA->viewRect();
            const QVector<qreal> tks = cs.ay.tickValues(qMin(vr.top(), vr.bottom()),
                                                       qMax(vr.top(), vr.bottom()));
            QVector<qreal> rows;
            for (qreal v : tks)
                rows.append(camA->project(QVector3D(0.0f, float(v), 0.0f), paA).screen.y());
            QVERIFY2(rows.size() >= 4, "非对称夹具至少 4 条刻度行");
            int mirrorHits = 0;
            for (qreal r : rows) {
                for (qreal r2 : rows) {
                    if (qAbs((2.0 * paA.center().y() - r) - r2) <= 1.0) { ++mirrorHits; break; }
                }
            }
            QVERIFY2(mirrorHits * 2 <= rows.size(),
                     qPrintable(QString("夹具刻度行集必须不关于视图中心对称（镜像命中 %1/%2）")
                                .arg(mirrorHits).arg(rows.size())));
            qInfo().noquote() << QString("t72 夹具自检: yTicks=%1 条 行集={%2} 镜像命中=%3/%1（非对称）")
                .arg(rows.size()).arg([&rows]() {
                    QString s2; for (qreal r : rows) s2 += QString::number(r, 'f', 1) + " ";
                    return s2.trimmed(); }()).arg(mirrorHits);
        }

        // ② 静态逐特征对位：整窗抓图（CPU 与 GL 各一张）的水平网格行簇必须**逐条一致（±1px）**
        {
            const QImage imgCpu = cs.w.grab().toImage();
            const QImage imgGl  = gs.w.grab().toImage();
            QVERIFY2(!imgCpu.isNull() && !imgGl.isNull(), "整窗抓图应成功");
            const double sC = double(imgCpu.width()) / double(cs.w.width());
            const double sG = double(imgGl.width()) / double(gs.w.width());
            const QVector<qreal> rowsCpu = gridRowClusters(imgCpu, paA, sC);
            const QVector<qreal> rowsGl  = gridRowClusters(imgGl,  paG, sG);
            QString sc, sg;
            for (qreal r : rowsCpu) sc += QString::number(r, 'f', 1) + " ";
            for (qreal r : rowsGl)  sg += QString::number(r, 'f', 1) + " ";
            qInfo().noquote() << QString("t72 整窗抓图行簇: CPU=[%1] GL=[%2]（plotArea 内逻辑行）")
                                 .arg(sc.trimmed()).arg(sg.trimmed());
            QVERIFY2(rowsCpu.size() >= 4, "CPU 整窗抓图应至少 4 条网格行");
            QVERIFY2(rowsGl.size() == rowsCpu.size(),
                     qPrintable(QString("GL 与 CPU 网格行数应一致：cpu=%1 gl=%2（镜像会改变集合大小）")
                                .arg(rowsCpu.size()).arg(rowsGl.size())));
            for (int i = 0; i < rowsCpu.size(); ++i) {
                QVERIFY2(qAbs(rowsCpu[i] - rowsGl[i]) <= 1.0,
                         qPrintable(QString("第 %1 条网格行 CPU/GL 应一致（±1px）：cpu=%2 gl=%3")
                                    .arg(i).arg(rowsCpu[i]).arg(rowsGl[i])));
            }
            qInfo().noquote() << QString("t72 静态对位: %1/%1 条网格行 CPU↔GL 一致（±1px）").arg(rowsCpu.size());
        }

        // ③ 动态方向：范围单调上移 ≥5 步 ⇒ 同一特征（数值 0 的脊）行号 CPU 与 GL 必须**同号单调**
        //    （t71 判据：范围上移 ⇒ 内容下移 ⇒ 行号增大；镜像时 GL 行号递减）
        {
            QVector<qreal> rowsCpuTrace, rowsGlTrace;
            for (int step = 0; step < 6; ++step) {
                if (step > 0) {
                    cs.w.panViewCartesian(0.0, 2.0);      // 范围上移 2 个单位（两孪生同步）
                    gs.w.panViewCartesian(0.0, 2.0);
                    cs.w.repaint();
                    gs.w.repaint();
                    QTest::qWait(80);
                }
                const QImage iC = cs.w.grab().toImage();
                const QImage iG = gs.w.grab().toImage();
                const double sC = double(iC.width()) / double(cs.w.width());
                const double sG = double(iG.width()) / double(gs.w.width());
                const QVector<qreal> cCl = gridRowClusters(iC, paA, sC);
                const QVector<qreal> gCl = gridRowClusters(iG, paG, sG);
                // 特征 = 数值 0 的那条脊：预测行取自 CPU project()，再在实测簇里取最近者（窗口 ±8px）
                const double pred = cs.layer.camera()
                        ->project(QVector3D(0.0f, 0.0f, 0.0f), paA).screen.y() - paA.top();
                auto nearest = [&pred](const QVector<qreal>& cl) {
                    double best = 1e9;
                    for (qreal c : cl) if (qAbs(c - pred) < qAbs(best - pred)) best = c;
                    return best;
                };
                const double cRow = nearest(cCl);
                const double gRow = nearest(gCl);
                QVERIFY2(qAbs(cRow - pred) <= 8.0,
                         qPrintable(QString("step %1：CPU 应能定位数值 0 的脊（预测行 %2，实测 %3）")
                                    .arg(step).arg(pred).arg(cRow)));
                QVERIFY2(qAbs(gRow - pred) <= 8.0,
                         qPrintable(QString("step %1：GL 应能定位数值 0 的脊（预测行 %2，实测 %3）——"
                                            "镜像时该脊落在中心另一侧，超出窗口")
                                    .arg(step).arg(pred).arg(gRow)));
                QVERIFY2(qAbs(cRow - gRow) <= 1.0,
                         qPrintable(QString("step %1：CPU/GL 同一脊行号应一致（±1px）：cpu=%2 gl=%3")
                                    .arg(step).arg(cRow).arg(gRow)));
                rowsCpuTrace.append(cRow);
                rowsGlTrace.append(gRow);
            }
            QString tc, tg;
            for (qreal r : rowsCpuTrace) tc += QString::number(r, 'f', 1) + " ";
            for (qreal r : rowsGlTrace)  tg += QString::number(r, 'f', 1) + " ";
            qInfo().noquote() << QString("t72 动态轨道: CPU=[%1] GL=[%2]（范围每步上移 +2）")
                                 .arg(tc.trimmed()).arg(tg.trimmed());
            QVERIFY2(rowsCpuTrace.size() >= 6, "动态轨道应至少 6 步（≥5 步单调判据）");
            int monoCpu = 0, monoGl = 0;
            for (int i = 1; i < rowsCpuTrace.size(); ++i) {
                if (rowsCpuTrace[i] > rowsCpuTrace[i - 1]) ++monoCpu;
                if (rowsGlTrace[i] > rowsGlTrace[i - 1]) ++monoGl;
            }
            QVERIFY2(monoCpu == rowsCpuTrace.size() - 1,
                     qPrintable(QString("CPU：范围上移 ⇒ 内容必须下移（行号递增），实测递增步 %1/%2")
                                .arg(monoCpu).arg(rowsCpuTrace.size() - 1)));
            QVERIFY2(monoGl == rowsGlTrace.size() - 1,
                     qPrintable(QString("GL：必须与 CPU **同号单调**（递增），实测递增步 %1/%2（镜像时为 0）")
                                .arg(monoGl).arg(rowsGlTrace.size() - 1)));
            qInfo().noquote() << QString("t72 动态方向: CPU 递增 %1/%2，GL 递增 %3/%2（同号单调）")
                                 .arg(monoCpu).arg(rowsCpuTrace.size() - 1).arg(monoGl);
        }
    }

    // ===== t74：GL 模式下边框轴（plotArea 外）必须跟随视图变化更新 =====
    // 症状（t70 §1）：GL 分支的 scheduleRepaint 只 update() GL 宿主，从不调度外层 widget ⇒
    // plotArea 外的边框轴刻度/标签从第二帧起冻结（真实交互期间外层 Paint=0、GL 宿主 Paint=1）。
    // 本段：真实 widget（GL/CPU 各一），用事件过滤器统计外层 widget 与 GL 宿主的 Paint 次数，
    // 对四种视图变化（拖动平移 / 滚轮缩放 / 轴范围变更 / 绘图区尺寸变化）断言：
    //   外层 Paint ≥ 1，且边框轴带像素差异 > 0、plotArea 内差异 > 0；
    // 并断言空闲 20 帧内外层与宿主 Paint 均为 0（防每帧无条件重绘）。
    {
        struct T74Counters : public QObject {
            QObject* outer = nullptr;
            QObject* host = nullptr;
            int outerCount = 0, hostCount = 0;
            T74Counters(QObject* o, QObject* h) : outer(o), host(h) {}
            bool eventFilter(QObject* obj, QEvent* e) override
            {
                if (e->type() == QEvent::Paint) {
                    if (obj == outer) ++outerCount;
                    else if (obj == host) ++hostCount;
                }
                return false;
            }
            void reset() { outerCount = hostCount = 0; }
        };
        auto diffIn = [](const QImage& a, const QImage& b, const QRect& r) {
            const QRect rr = r.intersected(a.rect()).intersected(b.rect());
            int n = 0;
            for (int y = rr.top(); y <= rr.bottom(); ++y)
                for (int x = rr.left(); x <= rr.right(); ++x)
                    if (a.pixelColor(x, y) != b.pixelColor(x, y)) ++n;
            return n;
        };
        auto sendPan2 = [](QWidget* ww, const QPointF& a, const QPointF& b) {
            auto* p = new QMouseEvent(QEvent::MouseButtonPress, a, ww->mapToGlobal(a),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(ww, p); delete p;
            auto* m = new QMouseEvent(QEvent::MouseMove, b, ww->mapToGlobal(b),
                                      Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(ww, m); delete m;
            auto* r = new QMouseEvent(QEvent::MouseButtonRelease, b, ww->mapToGlobal(b),
                                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(ww, r); delete r;
        };

        struct T74Scene {
            QChartWidget w;
            QValueAxis ax{nullptr, Qt::AlignBottom};
            QValueAxis ay{nullptr, Qt::AlignLeft};
            QChartLayer layer;
            explicit T74Scene(bool gl)
            {
                ax.setRange(-10.0, 10.0); ax.setTickCount(6); ax.setColor(Qt::black);
                ay.setRange(-12.0, 8.0);  ay.setTickCount(6); ay.setColor(Qt::black);
                w.addAxis(&ax); w.addAxis(&ay); w.addLayer(&layer);
                layer.setGridVisible(true);
                layer.setGridColor(QColor(150, 150, 150));
                if (gl) w.setRenderBackend(QChartAbstractWidget::RenderBackend::OpenGL);
                w.resize(720, 540);
                w.show();
            }
        };

        for (bool gl : {false, true}) {
            const char* be = gl ? "GL" : "CPU";
            T74Scene s(gl);
            QVERIFY2(QTest::qWaitForWindowExposed(&s.w), qPrintable(QString("[%1] 窗口应暴露").arg(be)));
            for (int i = 0; i < 20; ++i) { QTest::qWait(20); if (s.w.plotArea().width() > 0) break; }
            auto* host = qobject_cast<QOpenGLWidget*>(s.w.glHostWidget());
            if (gl) {
                QVERIFY2(host && host->isVisible(), "GL 模式应创建并显示 GL 宿主");
            }
            T74Counters cnt(&s.w, host);
            s.w.installEventFilter(&cnt);
            if (host) host->installEventFilter(&cnt);

            // 让首帧绘制彻底结束（此后计数才有意义）
            QTest::qWait(400);
            s.w.grab();
            QTest::qWait(150);
            QVERIFY2(cnt.outerCount >= 1, qPrintable(QString("[%1] 首帧后外层至少绘制过 1 次（计数通道自检）").arg(be)));

            // ---- 空闲不重绘：连续 20 帧事件循环内，外层与宿主 Paint 都必须为 0 ----
            cnt.reset();
            for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QTest::qWait(10); }
            qInfo().noquote() << QString("t74 [%1] 空闲 20 帧：外层 Paint=%2 宿主 Paint=%3")
                                 .arg(be).arg(cnt.outerCount).arg(cnt.hostCount);
            QVERIFY2(cnt.outerCount == 0 && cnt.hostCount == 0,
                     qPrintable(QString("[%1] 空闲不得重绘（外层 %2 / 宿主 %3）——防每帧无条件重绘")
                                .arg(be).arg(cnt.outerCount).arg(cnt.hostCount)));

            struct Change { const char* tag; };
            const Change changes[4] = { { "拖动平移" }, { "滚轮缩放" }, { "轴范围变更" }, { "绘图区尺寸变化" } };
            for (int ci = 0; ci < 4; ++ci) {
                const QRectF paBefore = s.w.plotArea();
                const QImage before = s.w.grab().toImage();
                cnt.reset();

                switch (ci) {
                case 0:
                    sendPan2(&s.w, QPointF(360, 270), QPointF(250, 350));
                    break;
                case 1: {
                    QWheelEvent we(QPointF(360, 270), s.w.mapToGlobal(QPointF(360, 270)),
                                   QPoint(0, 0), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                                   Qt::NoScrollPhase, false);
                    QApplication::sendEvent(&s.w, &we);
                    break;
                }
                case 2:
                    s.ax.setRange(-6.0, 14.0);
                    break;
                default:
                    s.w.resize(760, 560);
                    break;
                }
                // ★ 只跑事件循环（不 repaint/grab）——外层是否重绘完全由产品调度路径决定
                for (int i = 0; i < 15; ++i) { QCoreApplication::processEvents(); QTest::qWait(20); }
                const int outerPaints = cnt.outerCount, hostPaints = cnt.hostCount;
                const QImage after = s.w.grab().toImage();

                const QRectF paAfter = s.w.plotArea();
                // 边框带：plotArea 之外、窗口之内的左带与下带；尺寸变化时取重叠区
                const QRect overlap = before.rect().intersected(after.rect());
                const QRect bandL(overlap.left(), overlap.top(),
                                  qMax(0, int(qMin(paBefore.left(), paAfter.left())) - overlap.left()),
                                  overlap.height());
                const QRect bandB(overlap.left(),
                                  qMax(overlap.top(), int(qMax(paBefore.bottom(), paAfter.bottom())) + 1),
                                  overlap.width(),
                                  qMax(0, overlap.bottom() - qMax(overlap.top(), int(qMax(paBefore.bottom(), paAfter.bottom())) + 1) + 1));
                const int dL = diffIn(before, after, bandL);
                const int dB = diffIn(before, after, bandB);
                const int dIn = diffIn(before, after, paAfter.toRect().adjusted(2, 2, -2, -2).intersected(overlap));

                qInfo().noquote()
                    << QString("t74 [%1] %2：外层 Paint=%3 宿主 Paint=%4 | 左边框带 diff=%5 下边框带 diff=%6 plotArea 内 diff=%7 | plotArea %8x%9 → %10x%11")
                           .arg(be).arg(changes[ci].tag).arg(outerPaints).arg(hostPaints)
                           .arg(dL).arg(dB).arg(dIn)
                           .arg(paBefore.width()).arg(paBefore.height())
                           .arg(paAfter.width()).arg(paAfter.height());

                QVERIFY2(outerPaints >= 1,
                         qPrintable(QString("[%1] %2 后外层 widget 必须重绘（≥1）——GL 下边框轴不跟随的根因；实测 %3")
                                        .arg(be).arg(changes[ci].tag).arg(outerPaints)));
                QVERIFY2(dL > 0 || dB > 0,
                         qPrintable(QString("[%1] %2 后边框轴带像素必须变化（左 %3 / 下 %4）")
                                        .arg(be).arg(changes[ci].tag).arg(dL).arg(dB)));
                QVERIFY2(dIn > 0,
                         qPrintable(QString("[%1] %2 后 plotArea 内像素必须变化（实测 %3）")
                                        .arg(be).arg(changes[ci].tag).arg(dIn)));
            }

            // 变化序列结束后再次校验空闲：不得留下"每次绘制再触发绘制"的隐性循环
            cnt.reset();
            for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QTest::qWait(10); }
            qInfo().noquote() << QString("t74 [%1] 四次变化后再空闲 20 帧：外层 Paint=%2 宿主 Paint=%3")
                                 .arg(be).arg(cnt.outerCount).arg(cnt.hostCount);
            QVERIFY2(cnt.outerCount == 0 && cnt.hostCount == 0,
                     qPrintable(QString("[%1] 变化序列后仍不得有空闲重绘（外层 %2 / 宿主 %3）")
                                .arg(be).arg(cnt.outerCount).arg(cnt.hostCount)));
        }
    }
}
