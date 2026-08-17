// test_qchartcamera3d.cpp —— QChartCamera3D 单元测试
// 覆盖（design_3d.md §11.1 TestQChartCamera3D 全部 8 组），含 D-3D-2 退化一致性硬验收。
// 注：QMatrix4x4/QVector3D 为 float 存储 → 容差按 1e-4（屏幕 1e-3）取。
#include <QtTest>
#include <QVariantAnimation>
#include <QPropertyAnimation>
#include <cmath>
#include "../../QChartCamera3D.h"
#include "../../QChartCamera.h"   // QChartCamera2D::cartesianToPixel（硬验收对照）
#include "test_qchartcamera3d.h"

namespace {
    qreal elevationDeg(const QChartCamera3D& cam) {
        QVector3D d = (cam.position() - cam.lookAt()).normalized();
        const qreal sinP = qBound<qreal>(-1.0, QVector3D::dotProduct(d, QVector3D(0, 1, 0)), 1.0);
        return qRadiansToDegrees(qAsin(sinP));
    }
}

// ===== 1. lookAt：行向量正交 + 单位；平移项 = -R·eye =====
void TestQChartCamera3D::lookAt_orthonormal() {
    QChartCamera3D cam;
    cam.setPosition(QVector3D(3, 4, 5));
    cam.setLookAt(QVector3D(1, 1, 0));
    cam.setUp(QVector3D(0, 1, 0));

    QMatrix4x4 v = cam.viewMatrix();
    QVector3D r0(v(0, 0), v(0, 1), v(0, 2));   // side
    QVector3D r1(v(1, 0), v(1, 1), v(1, 2));   // upv
    QVector3D r2(v(2, 0), v(2, 1), v(2, 2));   // -forward

    // 正交
    QVERIFY(qAbs(QVector3D::dotProduct(r0, r1)) < 1e-4);
    QVERIFY(qAbs(QVector3D::dotProduct(r0, r2)) < 1e-4);
    QVERIFY(qAbs(QVector3D::dotProduct(r1, r2)) < 1e-4);
    // 单位
    QVERIFY(qAbs(r0.length() - 1.0) < 1e-4);
    QVERIFY(qAbs(r1.length() - 1.0) < 1e-4);
    QVERIFY(qAbs(r2.length() - 1.0) < 1e-4);

    // 平移项 = -R·eye：eye → 视图原点
    QVector4D eyeView = v * QVector4D(3, 4, 5, 1);
    QVERIFY(qAbs(eyeView.x()) < 1e-4 && qAbs(eyeView.y()) < 1e-4 && qAbs(eyeView.z()) < 1e-4);

    // 目标 → (0, 0, -dist)
    const qreal dist = (QVector3D(3, 4, 5) - QVector3D(1, 1, 0)).length();
    QVector4D tgtView = v * QVector4D(1, 1, 0, 1);
    QVERIFY(qAbs(tgtView.x()) < 1e-4 && qAbs(tgtView.y()) < 1e-4
            && qAbs(tgtView.z() + dist) < 1e-3);

    // 直接核对 m(0,3) = -side·eye、m(1,3) = -upv·eye、m(2,3) = forward·eye
    const QVector3D eye(3, 4, 5);
    QVERIFY(qAbs(v(0, 3) + QVector3D::dotProduct(r0, eye)) < 1e-4);
    QVERIFY(qAbs(v(1, 3) + QVector3D::dotProduct(r1, eye)) < 1e-4);
    QVERIFY(qAbs(v(2, 3) - QVector3D::dotProduct(-r2, eye)) < 1e-3);
}

// ===== 2. 【D-3D-2 硬验收】正交俯视 ≡ 2D cartesianToPixel（含 y 翻转）=====
void TestQChartCamera3D::orthographicTopDown_equals2D() {
    QChartCamera3D cam;
    cam.setPosition(QVector3D(0, 0, 10));   // 俯视
    cam.setLookAt(QVector3D(0, 0, 0));
    cam.setUp(QVector3D(0, 1, 0));
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
    const QRectF viewRect(0, 0, 10, 10);
    cam.setOrthographicBox(viewRect);        // 正交盒 = viewRect
    cam.setNearPlane(0.1);
    cam.setFarPlane(100.0);

    const QRectF plot(0, 0, 400, 300);
    const qreal xs[] = { -2.0, 0.0, 2.5, 5.0, 7.5, 10.0, 12.0 };
    const qreal ys[] = { -3.0, 0.0, 1.5, 5.0, 8.5, 10.0, 13.0 };
    for (qreal x : xs) {
        for (qreal y : ys) {
            QChartProjectedPoint p = cam.project(QVector3D(x, y, 0), plot);
            QPointF expected = QChartCamera2D::cartesianToPixel(viewRect, plot, x, y);
            QVERIFY2(qAbs(p.screen.x() - expected.x()) < 1e-3 &&
                     qAbs(p.screen.y() - expected.y()) < 1e-3,
                     qPrintable(QString("正交俯视 != 2D at (%1,%2): 3D=(%3,%4) 2D=(%5,%6)")
                                .arg(x).arg(y).arg(p.screen.x()).arg(p.screen.y())
                                .arg(expected.x()).arg(expected.y())));
            // 深度：z=0 在相机前方 10 单位
            QVERIFY(qAbs(p.depth - 10.0) < 1e-3);
        }
    }

    // y 翻转显式断言：view 上（大 y）→ 像素上（小 py）
    QChartProjectedPoint low  = cam.project(QVector3D(5, 0, 0), plot);
    QChartProjectedPoint high = cam.project(QVector3D(5, 10, 0), plot);
    QVERIFY2(high.screen.y() < low.screen.y(), "y 翻转：View 上 → 像素上");
}

// ===== 3. orbit：距离不变、lookAt 不变、yaw 几何、pitch clamp ±89° =====
void TestQChartCamera3D::orbit_yawPitch_geometry() {
    // 默认相机：position(0,0,10)、lookAt(0,0,0)、距离 10
    QChartCamera3D cam;
    const qreal dist0 = (cam.position() - cam.lookAt()).length();
    QCOMPARE(dist0, 10.0);

    // yaw 30°（绕 up 轴）：(0,0,10) → (5, 0, 8.660)
    cam.orbit(30, 0);
    QVERIFY(qAbs((cam.position() - cam.lookAt()).length() - dist0) < 1e-4);
    QCOMPARE(cam.lookAt(), QVector3D(0, 0, 0));
    QVERIFY(qAbs(cam.position().x() - 5.0) < 1e-3);
    QVERIFY(qAbs(cam.position().y()) < 1e-6);
    QVERIFY(qAbs(cam.position().z() - 8.660254) < 1e-3);

    // pitch clamp：orbit(0, -90) → 仰角压到 +89°（恰达极点 → 水平方向兜底）
    QChartCamera3D c2;
    c2.orbit(0, -90);
    QVERIFY(elevationDeg(c2) > 88.0 && elevationDeg(c2) <= 89.0 + 1e-3);
    QVERIFY(qAbs((c2.position() - c2.lookAt()).length() - 10.0) < 1e-4);
    QCOMPARE(c2.lookAt(), QVector3D(0, 0, 0));

    // orbit(0, +90) → -89°
    QChartCamera3D c3;
    c3.orbit(0, 90);
    QVERIFY(elevationDeg(c3) >= -89.0 - 1e-3 && elevationDeg(c3) < -88.0);
    QVERIFY(qAbs((c3.position() - c3.lookAt()).length() - 10.0) < 1e-4);

    // 反复大幅 orbit 不越界、距离/目标不变
    QChartCamera3D c4;
    for (int i = 0; i < 12; ++i)
        c4.orbit(17, -43);
    QVERIFY(qAbs(elevationDeg(c4)) <= 89.0 + 1e-3);
    QVERIFY(qAbs((c4.position() - c4.lookAt()).length() - 10.0) < 1e-3);
    QCOMPARE(c4.lookAt(), QVector3D(0, 0, 0));
}

// ===== 4. dolly：距离缩放、lookAt 不变、factor<=0 防除零 =====
void TestQChartCamera3D::dolly_factor() {
    QChartCamera3D cam;
    cam.dolly(0.5);                          // 10 → 5
    QVERIFY(qAbs((cam.position() - cam.lookAt()).length() - 5.0) < 1e-4);
    QVERIFY(qAbs(cam.position().x()) < 1e-6 && qAbs(cam.position().y()) < 1e-6
            && qAbs(cam.position().z() - 5.0) < 1e-6);
    QCOMPARE(cam.lookAt(), QVector3D(0, 0, 0));

    cam.dolly(2.0);                          // 5 → 10
    QVERIFY(qAbs((cam.position() - cam.lookAt()).length() - 10.0) < 1e-4);
    QCOMPARE(cam.lookAt(), QVector3D(0, 0, 0));

    // factor<=0 忽略（无 NaN、位置不变）
    const QVector3D before = cam.position();
    cam.dolly(0);
    cam.dolly(-1.0);
    QCOMPARE(cam.position(), before);
    QVERIFY(!qIsNaN(cam.position().x()) && !qIsNaN(cam.position().y()) && !qIsNaN(cam.position().z()));
}

// ===== 5. panTarget：position/lookAt 同步平移、viewMatrix 平移项正确 =====
void TestQChartCamera3D::panTarget_translatesBoth() {
    QChartCamera3D cam;                      // 默认前方 -z → 相机平面 = 世界 X/Y
    cam.panTarget(3.0, -4.0);

    // 同移 delta = (3, -4, 0)
    QVERIFY(qAbs(cam.position().x() - 3.0) < 1e-4 && qAbs(cam.position().y() + 4.0) < 1e-4
            && qAbs(cam.position().z() - 10.0) < 1e-4);
    QVERIFY(qAbs(cam.lookAt().x() - 3.0) < 1e-4 && qAbs(cam.lookAt().y() + 4.0) < 1e-4
            && qAbs(cam.lookAt().z()) < 1e-6);

    // viewMatrix 平移项：新 lookAt 映射到 (0, 0, -dist)
    QMatrix4x4 v = cam.viewMatrix();
    QVector4D t = v * QVector4D(cam.lookAt(), 1.0f);
    QVERIFY(qAbs(t.x()) < 1e-4 && qAbs(t.y()) < 1e-4 && qAbs(t.z() + 10.0) < 1e-3);

    // 相对距离与朝向不变
    QVERIFY(qAbs((cam.position() - cam.lookAt()).length() - 10.0) < 1e-4);
}

// ===== 6. 透视 vs 正交：同 world 点两模式屏幕坐标差异符合预期 =====
void TestQChartCamera3D::perspectiveVsOrthographic() {
    QChartCamera3D cam;                      // 默认透视：pos(0,0,10)、fov 45°
    const QRectF plot(0, 0, 400, 300);       // aspect 4/3

    // 正交模式：盒 (0,0,10,10) → 世界 (5,5) 为盒中心
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
    cam.setOrthographicBox(QRectF(0, 0, 10, 10));
    QChartProjectedPoint o1 = cam.project(QVector3D(5, 5, 0), plot);
    QVERIFY(qAbs(o1.screen.x() - 200.0) < 1e-3 && qAbs(o1.screen.y() - 150.0) < 1e-3);
    // 目标点 (0,0,0) → 正交盒左下角
    QChartProjectedPoint o2 = cam.project(QVector3D(0, 0, 0), plot);
    QVERIFY(qAbs(o2.screen.x()) < 1e-3 && qAbs(o2.screen.y() - 300.0) < 1e-3);

    // 透视模式：同点 (5,5,0) 明显偏离中心（透视角差）；目标点 → 视口中心
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Perspective);
    QChartProjectedPoint p1 = cam.project(QVector3D(5, 5, 0), plot);
    QVERIFY(std::isfinite(p1.screen.x()) && std::isfinite(p1.screen.y()));
    QVERIFY2(qAbs(p1.screen.x() - 200.0) > 100.0 || qAbs(p1.screen.y() - 150.0) > 100.0,
             "透视与正交屏幕坐标应有明显差异");
    QChartProjectedPoint p2 = cam.project(QVector3D(0, 0, 0), plot);
    QVERIFY(qAbs(p2.screen.x() - 200.0) < 1e-3 && qAbs(p2.screen.y() - 150.0) < 1e-3);
}

// ===== 7. 退化：position == lookAt → orbit/dolly no-op、无 NaN =====
void TestQChartCamera3D::degenerate_positionEqualsLookAt() {
    QChartCamera3D cam;
    cam.setPosition(QVector3D(1, 2, 3));
    cam.setLookAt(QVector3D(1, 2, 3));
    const QVector3D pos = cam.position();
    const QVector3D tgt = cam.lookAt();

    cam.orbit(30, 45);
    cam.dolly(0.5);
    cam.dolly(2.0);

    // 设计只保证 orbit/dolly 为 no-op
    QCOMPARE(cam.position(), pos);
    QCOMPARE(cam.lookAt(), tgt);

    // panTarget 不在 no-op 列表：应正常同步平移且无 NaN（position==lookAt 保持）
    cam.panTarget(1, 1);
    QVERIFY(!qIsNaN(cam.position().x()) && !qIsNaN(cam.position().y()) && !qIsNaN(cam.position().z()));
    QCOMPARE(cam.position(), cam.lookAt());
}

// ===== 8. 属性可动画：三 Q_PROPERTY、setter 发 viewChanged、QVector3D 插值器 =====
void TestQChartCamera3D::properties_animatable() {
    QChartCamera3D cam;
    const QMetaObject* mo = cam.metaObject();
    QVERIFY(mo->indexOfProperty("position") >= 0);
    QVERIFY(mo->indexOfProperty("lookAt") >= 0);
    QVERIFY(mo->indexOfProperty("fovY") >= 0);

    // setter 发 viewChanged（值变化才发）
    int signalCount = 0;
    QObject::connect(&cam, &QChartCamera::viewChanged, [&signalCount]() { ++signalCount; });
    cam.setPosition(QVector3D(1, 0, 10));
    cam.setLookAt(QVector3D(1, 0, 0));
    cam.setFovY(60.0);
    QCOMPARE(signalCount, 3);

    // QVector3D 插值器：QVariantAnimation 往返（Qt 内建或库内注册，线性插值）
    QVariantAnimation anim;
    anim.setStartValue(QVariant::fromValue(QVector3D(0, 0, 0)));
    anim.setEndValue(QVariant::fromValue(QVector3D(10, 0, 0)));
    anim.setDuration(1000);
    anim.setCurrentTime(500);
    QVector3D mid = anim.currentValue().value<QVector3D>();
    QVERIFY(qAbs(mid.x() - 5.0) < 1e-4 && qAbs(mid.y()) < 1e-6 && qAbs(mid.z()) < 1e-6);

    // QPropertyAnimation 驱动 position 属性（D-3D-3：3D 相机动画走 QPropertyAnimation）
    QPropertyAnimation prop(&cam, "position");
    prop.setStartValue(QVariant::fromValue(QVector3D(1, 0, 10)));
    prop.setEndValue(QVariant::fromValue(QVector3D(1, 5, 10)));
    prop.setDuration(100);
    prop.start();
    prop.setCurrentTime(50);
    QVector3D midPos = cam.position();
    QVERIFY(qAbs(midPos.y() - 2.5) < 1e-3);
    prop.stop();
}
