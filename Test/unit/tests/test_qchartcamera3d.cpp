// test_qchartcamera3d.cpp —— QChartCamera3D 单元测试（R5 重写：派生不变量）
// 覆盖（design_3d.md §11.1 R5 8 例），含 D-3D-2 退化一致性硬验收（新形态论证）。
// 注：QMatrix4x4/QVector3D 为 float 存储 → 屏幕 1e-3、距离/角度 1e-3 容差。
#include <QtTest>
#include <QVariantAnimation>
#include <QPropertyAnimation>
#include <cmath>
#include "QChartCamera3D.h"
#include "QChartCamera.h"
#include "test_qchartcamera3d.h"

namespace {
    // 保守拟合距离 d = radius / tan(fovY/2)，radius = 半对角线
    qreal fitDistance(const QChartCamera3D& cam) {
        const qreal radius = cam.viewCubeSize().length() * 0.5;
        return radius / qTan(qDegreesToRadians(cam.fovY()) * 0.5);
    }
    qreal elevationDeg(const QChartCamera3D& cam) {
        const QVector3D d = (cam.position() - cam.lookAt()).normalized();
        const qreal sinP = qBound<qreal>(-1.0, QVector3D::dotProduct(d, QVector3D(0, 1, 0)), 1.0);
        return qRadiansToDegrees(qAsin(sinP));
    }
}

// ===== 1. 派生：position = lookAt − forward·d、|position−lookAt| == d、lookAt == 盒中心 =====
void TestQChartCamera3D::derivedPosition_lookAt() {
    QChartCamera3D cam;   // 默认 viewCube {0,0,0}-{10,10,10}、yaw45/pitch30、fov45

    QCOMPARE(cam.lookAt(), QVector3D(5, 5, 5));          // = 盒中心
    QCOMPARE(cam.viewCubeCenter(), QVector3D(5, 5, 5));

    const qreal d = fitDistance(cam);
    QVERIFY(qAbs((cam.position() - cam.lookAt()).length() - d) < 1e-3);
    // near = max(0.01, d − 1.5r)；far = d + 1.5r
    const qreal r = cam.viewCubeSize().length() * 0.5;
    QVERIFY(qAbs(cam.nearPlane() - qMax<qreal>(0.01, d - 1.5 * r)) < 1e-3);
    QVERIFY(qAbs(cam.farPlane() - (d + 1.5 * r)) < 1e-3);

    // 换盒：半径/d 随盒尺寸重派生
    cam.setViewCube(QChartWorldBox{ QVector3D(0, 0, 0), QVector3D(20, 4, 8) });
    QCOMPARE(cam.lookAt(), QVector3D(10, 2, 4));
    const qreal d2 = fitDistance(cam);
    QVERIFY(qAbs((cam.position() - cam.lookAt()).length() - d2) < 1e-3);
    QVERIFY(qAbs(cam.viewCubeSize().length() * 0.5 - 10.9544) < 1e-3);
}

// ===== 2. 【D-3D-2 硬验收】正交 + viewCube=viewRect 范围 + 俯视 ≡ 2D cartesianToPixel =====
void TestQChartCamera3D::orthographicTopDown_equals2D() {
    QChartCamera3D cam;
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
    cam.setViewCube(QChartWorldBox{ QVector3D(0, 0, -1), QVector3D(10, 10, 1) });   // x/y = viewRect 范围，z 覆盖数据平面
    cam.setYaw(0.0);     // 俯视：forward = (0,0,−1)
    cam.setPitch(0.0);

    const QRectF viewRect(0, 0, 10, 10);
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
            // 深度有限且 = d（数据平面在相机前方 d）
            QVERIFY(std::isfinite(p.depth));
            QVERIFY(qAbs(p.depth - fitDistance(cam)) < 1e-3);
        }
    }

    // y 翻转显式断言：view 上（大 y）→ 像素上（小 py）
    QChartProjectedPoint low  = cam.project(QVector3D(5, 0, 0), plot);
    QChartProjectedPoint high = cam.project(QVector3D(5, 10, 0), plot);
    QVERIFY2(high.screen.y() < low.screen.y(), "y 翻转：View 上 → 像素上");
}

// ===== 3. orbit：只转 orientation，viewCube 不动（R6）；距离不变、lookAt 不变、pitch clamp =====
void TestQChartCamera3D::orbit_geometry() {
    QChartCamera3D cam;
    const qreal d0 = fitDistance(cam);
    const QChartWorldBox box0 = cam.viewCube();

    cam.orbit(30, 20);
    QVERIFY(qAbs((cam.position() - cam.lookAt()).length() - d0) < 1e-3);  // 距离不变
    QCOMPARE(cam.lookAt(), QVector3D(5, 5, 5));                            // 盒中心不变
    QVERIFY(qAbs(cam.yaw() - 75.0) < 1e-6);
    QVERIFY(qAbs(cam.pitch() - 50.0) < 1e-6);
    QVERIFY(cam.viewCube().min == box0.min && cam.viewCube().max == box0.max);  // R6：viewCube 不动

    // pitch clamp ±89°
    QChartCamera3D c2;
    c2.setPitch(88.0);
    c2.orbit(0, 5);
    QVERIFY(qAbs(c2.pitch() - 89.0) < 1e-6);
    c2.orbit(0, -200);
    QVERIFY(qAbs(c2.pitch() + 89.0) < 1e-6);
    // 仰角（派生）不越界
    QVERIFY(qAbs(elevationDeg(c2)) <= 89.0 + 1e-3);
}

// ===== 4. dolly：viewCube 缩放 f 倍 → d' == f·d、lookAt 不变、同 world 点屏幕外扩 =====
void TestQChartCamera3D::dolly_scale() {
    QChartCamera3D cam;
    const qreal d0 = fitDistance(cam);
    const QRectF plot(0, 0, 400, 300);
    const QPointF before = cam.project(QVector3D(4, 4, 4), plot).screen;
    const QPointF center(200, 150);

    cam.dolly(0.5);
    // 盒尺寸减半 → 距离减半
    QVERIFY(qAbs(cam.viewCubeSize().length() - 10.0 * qSqrt(3.0) * 0.5) < 1e-3);
    QVERIFY(qAbs(fitDistance(cam) - 0.5 * d0) < 1e-3);
    QCOMPARE(cam.lookAt(), QVector3D(5, 5, 5));   // 绕中心缩放 → 中心不变
    QVERIFY(qAbs((cam.position() - cam.lookAt()).length() - 0.5 * d0) < 1e-3);

    // 同 world 点屏幕坐标外扩（内容放大）
    const QPointF after = cam.project(QVector3D(4, 4, 4), plot).screen;
    const qreal dBefore = std::sqrt(QPointF::dotProduct(before - center, before - center));
    const qreal dAfter  = std::sqrt(QPointF::dotProduct(after - center, after - center));
    QVERIFY2(dAfter > dBefore, "dolly(0.5) 内容放大 → 同点屏幕外扩");

    // factor<=0 no-op
    const QVector3D sizeKeep = cam.viewCubeSize();
    cam.dolly(0);
    cam.dolly(-1.0);
    QVERIFY(cam.viewCubeSize() == sizeKeep);
}

// ===== 5. panViewCube：盒中心/position 同位移、viewMatrix 平移项正确 =====
void TestQChartCamera3D::pan_translates() {
    QChartCamera3D cam;
    const QChartWorldBox box0 = cam.viewCube();
    const QVector3D pos0 = cam.position();

    cam.panViewCube(3.0, -4.0);
    // 盒整体平移 (3,−4,0)
    QVERIFY(cam.viewCube().min == box0.min + QVector3D(3, -4, 0));
    QVERIFY(cam.viewCube().max == box0.max + QVector3D(3, -4, 0));
    QCOMPARE(cam.lookAt(), QVector3D(8, 1, 5));       // 中心同位移
    QVERIFY(cam.position() == pos0 + QVector3D(3, -4, 0));   // position 同位移

    // viewMatrix 平移项：新 lookAt 映射到 (0,0,−d)
    QMatrix4x4 v = cam.viewMatrix();
    QVector4D t = v * QVector4D(cam.lookAt(), 1.0f);
    QVERIFY(qAbs(t.x()) < 1e-3 && qAbs(t.y()) < 1e-3
            && qAbs(t.z() + fitDistance(cam)) < 1e-3);
}

// ===== 6. 透视 vs 正交：同 world 点两模式屏幕坐标差异符合预期 =====
void TestQChartCamera3D::perspectiveVsOrthographic() {
    QChartCamera3D cam;
    cam.setViewCube(QChartWorldBox{ QVector3D(0, 0, -1), QVector3D(10, 10, 1) });
    cam.setYaw(0.0);
    cam.setPitch(0.0);
    const QRectF plot(0, 0, 400, 300);

    // 正交：盒中心 (5,5) → 视口中心；盒角 (0,0) → 左下角
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Orthographic);
    QChartProjectedPoint o1 = cam.project(QVector3D(5, 5, 0), plot);
    QVERIFY(qAbs(o1.screen.x() - 200.0) < 1e-3 && qAbs(o1.screen.y() - 150.0) < 1e-3);
    QChartProjectedPoint o2 = cam.project(QVector3D(0, 0, 0), plot);
    QVERIFY(qAbs(o2.screen.x()) < 1e-3 && qAbs(o2.screen.y() - 300.0) < 1e-3);

    // 透视：盒中心仍在视轴 → 视口中心；角点 (0,0) 明显偏离（透视收缩）
    cam.setProjectionMode(QChartCamera3D::ProjectionMode::Perspective);
    QChartProjectedPoint p1 = cam.project(QVector3D(5, 5, 0), plot);
    QVERIFY(std::isfinite(p1.screen.x()) && std::isfinite(p1.screen.y()));
    QVERIFY(qAbs(p1.screen.x() - 200.0) < 1e-3 && qAbs(p1.screen.y() - 150.0) < 1e-3);
    QChartProjectedPoint p2 = cam.project(QVector3D(0, 0, 0), plot);
    QVERIFY2(qAbs(p2.screen.x() - 0.0) > 30.0 || qAbs(p2.screen.y() - 300.0) > 30.0,
             "透视下角点应偏离正交投影位置");
}

// ===== 7. 退化：viewCube 零尺寸 → orbit/dolly no-op、无 NaN =====
void TestQChartCamera3D::degenerate_zeroSize() {
    QChartCamera3D cam;
    cam.setViewCube(QChartWorldBox{ QVector3D(1, 2, 3), QVector3D(1, 2, 3) });
    const qreal yaw0 = cam.yaw(), pitch0 = cam.pitch();
    const QChartWorldBox box0 = cam.viewCube();

    cam.orbit(30, 45);
    cam.dolly(0.5);
    cam.dolly(2.0);
    QCOMPARE(cam.yaw(), yaw0);       // no-op
    QCOMPARE(cam.pitch(), pitch0);
    QVERIFY(cam.viewCube().min == box0.min && cam.viewCube().max == box0.max);

    // 派生无 NaN
    QVERIFY(!qIsNaN(cam.position().x()) && !qIsNaN(cam.position().y()) && !qIsNaN(cam.position().z()));
    QVERIFY(!qIsNaN(cam.nearPlane()) && !qIsNaN(cam.farPlane()));
}

// ===== 8. 属性可动画：五 Q_PROPERTY、setter 发 viewChanged、QVector3D 插值器 =====
void TestQChartCamera3D::properties_animatable() {
    QChartCamera3D cam;
    const QMetaObject* mo = cam.metaObject();
    QVERIFY(mo->indexOfProperty("viewCubeCenter") >= 0);
    QVERIFY(mo->indexOfProperty("viewCubeSize") >= 0);
    QVERIFY(mo->indexOfProperty("yaw") >= 0);
    QVERIFY(mo->indexOfProperty("pitch") >= 0);
    QVERIFY(mo->indexOfProperty("fovY") >= 0);

    // setter 发 viewChanged（值变化才发）
    int signalCount = 0;
    QObject::connect(&cam, &QChartCamera::viewChanged, [&signalCount]() { ++signalCount; });
    cam.setViewCubeCenter(QVector3D(6, 5, 5));
    cam.setViewCubeSize(QVector3D(12, 10, 10));
    cam.setYaw(60.0);
    cam.setPitch(40.0);
    cam.setFovY(60.0);
    QCOMPARE(signalCount, 5);

    // QVector3D 插值器：QVariantAnimation 往返（Qt 内建或库内注册，线性插值）
    QVariantAnimation anim;
    anim.setStartValue(QVariant::fromValue(QVector3D(0, 0, 0)));
    anim.setEndValue(QVariant::fromValue(QVector3D(10, 0, 0)));
    anim.setDuration(1000);
    anim.setCurrentTime(500);
    QVector3D mid = anim.currentValue().value<QVector3D>();
    QVERIFY(qAbs(mid.x() - 5.0) < 1e-4 && qAbs(mid.y()) < 1e-6 && qAbs(mid.z()) < 1e-6);

    // QPropertyAnimation 驱动 viewCubeCenter（R5：3D 相机动画走 QPropertyAnimation）
    QPropertyAnimation prop(&cam, "viewCubeCenter");
    prop.setStartValue(QVariant::fromValue(QVector3D(5, 5, 5)));
    prop.setEndValue(QVariant::fromValue(QVector3D(5, 10, 5)));
    prop.setDuration(100);
    prop.start();
    prop.setCurrentTime(50);
    QVERIFY(qAbs(cam.viewCubeCenter().y() - 7.5) < 1e-3);
    prop.stop();
}
