// test.cpp —— 批次 B4：轴阶段 demo 主程序（分发表 + 参数选择 + shot 冒烟）
// 用法：
//   QChartDemo                     → 跑全部新 demo（axis、axis3d，CPU 后端）
//   QChartDemo axis [cpu|gl]       → 只跑指定 demo，可选后端（缺省 cpu）
//   QChartDemo axis3d gl
// 环境变量 QCHART_DEMO_SHOT=1：show 后 ~400ms 对每个已显示窗口 grab 存 demo_<name>.png
//   并自动退出（GL 后端 offscreen 无窗口暴露 → 跳过该窗口并记录，保证自动化不挂死）。
#include "demos.h"
#include "QChartAbstractWidget.h"
#include "QChartWidget.h"
#include "QChartWidget3D.h"
#include <QApplication>
#include <QTimer>
#include <QPixmap>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>

namespace {
DemoBackend backendFromKey(const QString& k)
{
    return (k.compare(QLatin1String("gl"), Qt::CaseInsensitive) == 0)
        ? DemoBackend::Gl : DemoBackend::Cpu;
}
QString backendKey(DemoBackend b)
{
    return b == DemoBackend::Gl ? QStringLiteral("gl") : QStringLiteral("cpu");
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QStringList args = app.arguments();

    // 演示注册表（批次 B4：旧条目随旧 demo 一并移出，见 LEGACY_DEMOS_OLD.txt）
    const DemoEntry entries[] = {
        { "axis",    [](DemoBackend b) { return buildDemoAxis(b); } },
        { "axis3d",  [](DemoBackend b) { return buildDemoAxis3D(b); } },
        { "scatter", [](DemoBackend b) { return buildDemoScatter(b); } },
        { "scatter3d", [](DemoBackend b) { return buildDemoScatter3D(b); } },
        { "line", [](DemoBackend b) { return buildDemoLine(b); } },
        { "line3d", [](DemoBackend b) { return buildDemoLine3D(b); } },
        { "polygon", [](DemoBackend b) { return buildDemoPolygon(b); } },
        { "bar", [](DemoBackend b) { return buildDemoBar(b); } },
        { "surface3d", [](DemoBackend b) { return buildDemoSurface3D(b); } },
    };

    // 参数解析：可选的 demo 名 + 可选后端 cpu/gl（缺省后端 CPU）
    DemoBackend backend = DemoBackend::Cpu;
    QString wantName = args.value(1);
    QString wantBackend = args.value(2);
    if (!wantName.isEmpty() && (wantName.compare(QLatin1String("cpu"), Qt::CaseInsensitive) == 0
                                || wantName.compare(QLatin1String("gl"), Qt::CaseInsensitive) == 0)) {
        backend = backendFromKey(wantName);
        wantName.clear();
        wantBackend.clear();
    } else if (!wantBackend.isEmpty()) {
        backend = backendFromKey(wantBackend);
    }

    struct WinRec { QString name; DemoBackend backend; QWidget* w; };
    QList<WinRec> wins;
    const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");

    for (const DemoEntry& e : entries) {
        if (!wantName.isEmpty() && wantName != QLatin1String(e.name))
            continue;
        // offscreen + GL：无窗口暴露，跳过该窗口并记录（自动化不挂死）
        if (offscreen && backend == DemoBackend::Gl) {
            qInfo().noquote() << QString("shot-skip %1 gl (offscreen 无 GL 窗口暴露)")
                                 .arg(QLatin1String(e.name));
            continue;
        }
        QWidget* w = e.build(backend);
        w->show();
        wins.append({ QLatin1String(e.name), backend, w });
    }

    // QCHART_DEMO_SHOT=1：延时 400ms 截图存盘并退出
    if (qEnvironmentVariableIsSet("QCHART_DEMO_SHOT")) {
        QTimer::singleShot(400, &app, [&]() {
            int saved = 0;
            for (const WinRec& rec : wins) {
                if (!rec.w->isVisible()) {
                    qInfo().noquote() << QString("shot-skip %1（窗口不可见）").arg(rec.name);
                    continue;
                }
                const QString file = QStringLiteral("demo_%1.png").arg(rec.name);
                const QPixmap pm = rec.w->grab();
                const bool ok = pm.save(file);
                qInfo().noquote() << QString("shot %1 %2 saved=%3")
                                     .arg(rec.name).arg(file).arg(ok);
                if (ok) ++saved;

                // GL 后端额外存宿主 FBO（axis3d gl 出图硬证据；axis 无 GL 宿主则跳过）
                if (rec.backend == DemoBackend::Gl) {
                    if (auto* cw = qobject_cast<QChartWidget*>(rec.w)) {
                        if (auto* host = qobject_cast<QOpenGLWidget*>(cw->glHostWidget())) {
                            const QString fboFile =
                                QStringLiteral("demo_%1_glhost.png").arg(rec.name);
                            const bool ok2 = host->grabFramebuffer().save(fboFile);
                            qInfo().noquote() << QString("shot-glhost %1 saved=%2")
                                                 .arg(fboFile).arg(ok2);
                        }
                    }
                }
            }
            qInfo().noquote() << QString("shot total saved=%1").arg(saved);
            app.quit();
        });
    }

    // QCHART_DEMO_SIM=1：脚本化交互观测（滚轮 1 格 + 水平拖 100px + 竖直拖 100px），逐条打印 viewRect
    if (qEnvironmentVariableIsSet("QCHART_DEMO_SIM")) {
        QTimer::singleShot(800, &app, [&]() {
            for (const WinRec& rec : wins) {
                auto* w = qobject_cast<QChartWidget*>(rec.w);
                if (!w) continue;
                auto logState = [&](const char* tag) {
                    if (auto* w3 = qobject_cast<QChartWidget3D*>(rec.w)) {
                        const QCube vc = w3->viewCube();
                        const QCube db = w3->dataBounds3D();
                        qInfo().noquote() << QString("SIM3D %1 vc=(%2,%3,%4)-(%5,%6,%7) db=(%8,%9,%10)-(%11,%12,%13)")
                            .arg(tag)
                            .arg(vc.min.x()).arg(vc.min.y()).arg(vc.min.z())
                            .arg(vc.max.x()).arg(vc.max.y()).arg(vc.max.z())
                            .arg(db.min.x()).arg(db.min.y()).arg(db.min.z())
                            .arg(db.max.x()).arg(db.max.y()).arg(db.max.z());
                        return;
                    }
                    const QRectF vr = w->viewRect();
                    const QRectF pa = w->plotArea();
                    qInfo().noquote() << QString("SIM %1 vr=(%2,%3,%4,%5) pa=(%6,%7,%8,%9)")
                        .arg(tag)
                        .arg(vr.x()).arg(vr.y()).arg(vr.width()).arg(vr.height())
                        .arg(pa.x()).arg(pa.y()).arg(pa.width()).arg(pa.height());
                    const QString f = QStringLiteral("sim_%1_%2.png").arg(rec.name, QLatin1String(tag));
                    const bool ok = w->grab().save(f);
                    qInfo().noquote() << QString("SIM snap %1 saved=%2").arg(f).arg(ok);
                };
                const QPointF c = QRectF(w->rect()).center();
                logState("init");

                // 1) 滚轮：窗口中心上滚 1 格（120）
                QWheelEvent wheel(c, w->mapToGlobal(c.toPoint()), QPoint(0, 0), QPoint(0, 120),
                                  Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
                QCoreApplication::sendEvent(w, &wheel);
                logState("wheel+1");

                // 2) 水平拖动 100px（press → 10 次 move → release）
                {
                    const QPointF p0 = c;
                    const QPointF p1 = c + QPointF(100, 0);
                    QMouseEvent press(QEvent::MouseButtonPress, p0, w->mapToGlobal(p0.toPoint()),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(w, &press);
                    for (int i = 1; i <= 10; ++i) {
                        const QPointF p = p0 + (p1 - p0) * (i / 10.0);
                        QMouseEvent mv(QEvent::MouseMove, p, w->mapToGlobal(p.toPoint()),
                                       Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                        QCoreApplication::sendEvent(w, &mv);
                    }
                    QMouseEvent rel(QEvent::MouseButtonRelease, p1, w->mapToGlobal(p1.toPoint()),
                                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(w, &rel);
                    logState("drag+100x");
                }

                // 3) 竖直拖动 100px
                {
                    const QPointF p0 = c;
                    const QPointF p1 = c + QPointF(0, -100);
                    QMouseEvent press(QEvent::MouseButtonPress, p0, w->mapToGlobal(p0.toPoint()),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(w, &press);
                    for (int i = 1; i <= 10; ++i) {
                        const QPointF p = p0 + (p1 - p0) * (i / 10.0);
                        QMouseEvent mv(QEvent::MouseMove, p, w->mapToGlobal(p.toPoint()),
                                       Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                        QCoreApplication::sendEvent(w, &mv);
                    }
                    QMouseEvent rel(QEvent::MouseButtonRelease, p1, w->mapToGlobal(p1.toPoint()),
                                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(w, &rel);
                    logState("drag-100y");
                }
            }
            app.quit();
        });
    }

    const int rc = app.exec();
    for (WinRec& r : wins)
        delete r.w;
    return rc;
}
