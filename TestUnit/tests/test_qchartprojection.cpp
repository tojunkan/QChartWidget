// test_qchartprojection.cpp —— QChartProjection 单元测试
// 覆盖：Cartesian/Polar 映射往返、包络转换、createPath 断路径
#include <QtTest>
#include <QPainterPath>
#include "../../QCartesianProjection.h"
#include "../../QPolarProjection.h"
#include "test_qchartprojection.h"

// ===== Cartesian：恒等映射往返 =====
void TestQChartProjection::cartesian_roundtrip() {
    QCartesianProjection proj;
    const qreal xs[] = { 0.0, 1.0, -2.5, 100.0, -0.001 };
    const qreal ys[] = { 0.0, -1.0, 3.14, 42.0, 7.5 };
    for (qreal x : xs) {
        for (qreal y : ys) {
            QPointF c = proj.toCartesian(x, y);
            QCOMPARE(c, QPointF(x, y));
            QPointF back = proj.fromCartesian(c.x(), c.y());
            QCOMPARE(back, QPointF(x, y));
        }
    }
}

// ===== Polar：toCartesian∘fromCartesian 往返（θ 归一化到 [0,360)）=====
void TestQChartProjection::polar_roundtrip() {
    QPolarProjection proj;
    const qreal radii[] = { 1.0, 2.5, 10.0 };
    const qreal angles[] = { 0.0, 45.0, 90.0, 180.0, 270.0, 359.0 };
    for (qreal r : radii) {
        for (qreal th : angles) {
            QPointF c = proj.toCartesian(th, r);
            QPointF back = proj.fromCartesian(c.x(), c.y());
            // r 应精确恢复
            QVERIFY2(qAbs(back.y() - r) < 1e-9, "Polar 半径往返应一致");
            // θ 差值应为 360° 的整数倍（含 0）
            qreal dtheta = back.x() - th;
            qreal wrapped = dtheta - qRound(dtheta / 360.0) * 360.0;
            QVERIFY2(qAbs(wrapped) < 1e-6, "Polar 角度往返应一致（模 360°）");
        }
    }
}

// ===== Polar 极点：fromCartesian(0,0) 返回 (NaN, 0) =====
void TestQChartProjection::polar_pole_returnsNaN() {
    QPolarProjection proj;
    QPointF back = proj.fromCartesian(0.0, 0.0);
    QVERIFY(qIsNaN(back.x()));
    QCOMPARE(back.y(), 0.0);
}

// ===== Cartesian：dataBounds 与 viewRect 恒等 =====
void TestQChartProjection::cartesian_bounds_identity() {
    QCartesianProjection proj;
    QRectF db(1.0, 2.0, 3.0, 4.0);
    QCOMPARE(proj.computeViewRect(db), db);
    QCOMPARE(proj.computeDataBounds(db), db);
}

// ===== Polar：computeDataBounds(computeViewRect(db)) 应包含原 db（采样取包络）=====
void TestQChartProjection::polar_bounds_contain() {
    QPolarProjection proj;
    QRectF db(0.0, 0.0, 360.0, 10.0);   // 完整圆盘

    QRectF vr = proj.computeViewRect(db);
    // 圆盘半径 10 的轴对齐包围盒
    QVERIFY(qAbs(vr.left() + 10.0) < 1e-9);
    QVERIFY(qAbs(vr.top() + 10.0) < 1e-9);
    QVERIFY(qAbs(vr.width() - 20.0) < 1e-9);
    QVERIFY(qAbs(vr.height() - 20.0) < 1e-9);

    QRectF db2 = proj.computeDataBounds(vr);
    // 采样得到的范围应覆盖原 dataBounds（θ 全圆、r 至少 [0,10]）
    QVERIFY2(db2.left() <= db.left() + 1e-9 && db2.right() >= db.right() - 1e-9,
             "θ 范围应包含原 θ 范围");
    QVERIFY2(db2.top() <= db.top() + 1e-9, "rMin 应 <= 原 rMin（含原点→0）");
    QVERIFY2(db2.bottom() >= db.bottom() - 1e-9, "rMax 应 >= 原 rMax");
}

// ===== createPath：采样点 NaN 处断路径（moveTo 重开子路径）=====
void TestQChartProjection::createPath_breaksOnNaN() {
    QCartesianProjection proj;

    // dataCurve 在 t∈(0.4, 0.6) 返回 NaN → 应断开为两条子路径
    auto nanCurve = [](qreal t) -> QPointF {
        if (t > 0.4 && t < 0.6) return QPointF(qQNaN(), qQNaN());
        return QPointF(t * 10.0, t * 10.0);
    };
    QPainterPath broken = proj.createPath(nanCurve, 100);
    int moveToCount = 0;
    for (int i = 0; i < broken.elementCount(); ++i)
        if (broken.elementAt(i).isMoveTo()) ++moveToCount;
    QCOMPARE(moveToCount, 2);   // NaN 前后各一段

    // 对照：无 NaN 的曲线应只有一条子路径
    auto okCurve = [](qreal t) -> QPointF { return QPointF(t * 10.0, t * 10.0); };
    QPainterPath whole = proj.createPath(okCurve, 100);
    int wholeMoveTo = 0;
    for (int i = 0; i < whole.elementCount(); ++i)
        if (whole.elementAt(i).isMoveTo()) ++wholeMoveTo;
    QCOMPARE(wholeMoveTo, 1);
}
