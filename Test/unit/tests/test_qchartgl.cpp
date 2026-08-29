// test_qchartgl.cpp —— QChartGL 资源池 + QOpenGLChartRenderer 骨架单元测试
// 运行环境（design_phase3.md §13.2）：真实 GL（xcb/桌面）跑探测；offscreen 平台 QSKIP。
#include <QtTest>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include "QChartGL.h"
#include "QOpenGLChartRenderer.h"
#include "test_qchartgl.h"

// ===== 1. QChartRenderer 接口契约（全平台，无 GL 依赖）=====
void TestQChartGL::renderer_interface_contract() {
    QOpenGLChartRenderer r;

    // 缓存语义（§2.1）：恒 true / setCachingEnabled no-op
    QVERIFY(r.isCachingEnabled());
    r.setCachingEnabled(false);
    QVERIFY(r.isCachingEnabled());          // no-op（GL 无 QPixmap 缓存）

    // 脏标记（invalidate* 置位 → 无崩溃；§2.1：数据/样式变化 → VBO 重建）
    r.invalidateBackground();
    r.invalidateForeground();

    // ⚠ device 非宿主 → qWarning 返回（不崩溃；A2 组合：GL 渲染无通用 QPaintDevice 目标）
    QChartScene scene;
    r.render(scene, nullptr);
    // ⚠ GL 不参与导出（A4）：renderUncached → qWarning 拒绝
    r.renderUncached(scene, nullptr);

    // 宿主未设置 / 无 GL 上下文：生命周期入口降级不崩溃（A9 QPainter 共存兜底）
    r.initializeGL();          // host==nullptr → qWarning
    r.resizeGL(100, 100);      // 无上下文 → no-op
    r.paintGL(scene);          // 无 GL → 空绘制返回
    const QRgb sentinel = r.pickIdAt(QPoint(5, 5), scene);   // 骨架：哨兵（t46 落地）
    QCOMPARE(sentinel, qRgb(255, 255, 255));                 // 0xFFFFFF = 无命中（§8.1）
}

// ===== 2. 共享根 / 实例计数生命周期（§7.3；成对注册/注销安全）=====
void TestQChartGL::sharedContext_refcount() {
    // ⚠ 进程级单例：不假设初始计数为 0（其他测试类的 QChartWidget3D 可能注册过且已注销）；
    // 只验证成对安全与「无实例 → nullptr / 有实例 → 有效或降级 nullptr」不变量。
    QVERIFY(QChartGL::sharedContext() == nullptr);   // 测试开始前应无存活实例（各测试类已析构）

    QChartGL::registerHost();
    QChartGL::registerHost();                        // 引用计数 2
    QOpenGLContext* sc = QChartGL::sharedContext();
    QVERIFY2(sc == nullptr || sc->isValid(),
             "共享根：无 GL 环境（offscreen）→ nullptr 降级；有 GL → 有效上下文");
    if (sc) {
        const QPair<int, int> v = sc->format().version();
        QVERIFY2(v.first > 3 || (v.first == 3 && v.second >= 3),
                 "共享根格式应 ≥ 3.3（§7.3：3.3 Core）");
        QVERIFY(sc->format().profile() == QSurfaceFormat::CoreProfile);
        QVERIFY(sc->format().depthBufferSize() >= 24);   // depth 24（A3）
    }
    QChartGL::unregisterHost();
    QVERIFY(QChartGL::sharedContext() == sc);        // 仍有 1 个存活实例 → 共享根保留
    QChartGL::unregisterHost();
    QVERIFY(QChartGL::sharedContext() == nullptr);   // 归零 → 释放（§7.3：无实例 → nullptr）

    // 释放后重新注册可重建（惰性，A3）
    QChartGL::registerHost();
    QChartGL::unregisterHost();
    QVERIFY(QChartGL::sharedContext() == nullptr);
}

// ===== 3. GL 环境探测（§10.1 硬件基线表初值；offscreen QSKIP，§13.2）=====
void TestQChartGL::gl_probe_context() {
    if (QGuiApplication::platformName() == "offscreen") {
        QSKIP("offscreen 平台无真实 GL：环境探测跳过（A8 llvmpipe 冒烟属 t48）");
    }

    // 共享根：xcb 平台应能创建
    QChartGL::registerHost();
    QOpenGLContext* sc = QChartGL::sharedContext();
    QVERIFY2(sc, "xcb 平台应能创建共享根上下文（记录 GPU/驱动/版本到 §10.1）");
    QVERIFY(sc->isValid());
    const QPair<int, int> v = sc->format().version();
    QVERIFY(v.first >= 3 && v.second >= 3);
    QVERIFY(sc->format().profile() == QSurfaceFormat::CoreProfile);
    QVERIFY(sc->format().depthBufferSize() >= 24);

    // QOpenGLWidget 上下文可建（show 后初始化）
    QOpenGLWidget w;
    w.setFormat(QChartGL::surfaceFormat());
    w.resize(64, 64);
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "QOpenGLWidget 窗口应能显示");
    QVERIFY2(w.isValid() && w.context() && w.context()->isValid(),
             "QOpenGLWidget GL 上下文应有效（无头/无 GL 驱动才会失败）");

    // 记录基线（§10.1）：vendor/renderer/version
    w.makeCurrent();
    if (QOpenGLFunctions* f = w.context()->functions()) {
        const auto str = [](const GLubyte* p) {
            return p ? QString::fromLatin1(reinterpret_cast<const char*>(p)) : QStringLiteral("?");
        };
        const QString vendor   = str(f->glGetString(GL_VENDOR));
        const QString renderer = str(f->glGetString(GL_RENDERER));
        const QString version  = str(f->glGetString(GL_VERSION));
        qInfo().noquote() << "GL 基线（§10.1 初值）: vendor=" << vendor
                          << "renderer=" << renderer << "version=" << version;
        QVERIFY2(renderer != QStringLiteral("?"), "GL_RENDERER 应可读（记录硬件基线）");
        QVERIFY2(version != QStringLiteral("?"), "GL_VERSION 应可读（记录驱动）");
    }
    w.doneCurrent();
    w.hide();

    QChartGL::unregisterHost();
    QVERIFY(QChartGL::sharedContext() == nullptr);
}
