// test_widget_gl.cpp —— 批次 A widget GL 宿主冒烟（实窗验证）
// 覆盖：QChartWidget 切 OpenGL 后端 → plotArea 对齐 QOpenGLWidget 子控件（GL 只画 plotArea 内）→
//       host FBO readback 取证网格/轴出墨 + 宿主几何==plotArea（外部边框轴由外层画，区域互不重叠）。
#include "test_widget_gl.h"

#include <QtTest>
#include <QtMath>
#include <cmath>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

#include "QChartWidget.h"
#include "QChartLayer.h"
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
}
