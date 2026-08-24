// test_qchartmath.cpp —— QChartMath + Projection3D 家族单元测试
// 覆盖（design_3d.md §11.1 TestQChartMath 全部 8 组）：
//   clipToNdc/ndcToScreen/clipToScreen、透视/正交矩阵性质、viewDepth、projectBatch 对齐
//   + Projection3D 家族：圆柱/球/笛卡尔/函数式映射、莫比乌斯采样、computeWorldBounds 兜底
#include <QtTest>
#include <memory>
#include "QChartMath.h"
#include "QChartProjection3D.h"
#include "QChartCartesianProjection3D.h"
#include "QChartCylindricalProjection3D.h"
#include "QChartSphericalProjection3D.h"
#include "QChartFunctionalProjection3D.h"
#include "QChartCamera.h"
#include "QChartSeries3D.h"
#include "QChartScatterSeries3D.h"
#include "QChartLineSeries3D.h"
#include "QChartLayer3D.h"
#include "QChartCamera3D.h"
#include "test_qchartmath.h"

namespace {
    // 注：QMatrix4x4/QVector3D 内部为 float 存储，矩阵元与映射结果带 ~1e-7 相对误差，
    // 故容差不能用 1e-9；屏幕坐标 1e-6、QVector3D 往返 1e-5、角度 1e-4。
    bool nearPoint(const QPointF& p, qreal x, qreal y, qreal tol = 1e-6) {
        return qAbs(p.x() - x) < tol && qAbs(p.y() - y) < tol;
    }
    bool nearVec3(const QVector3D& v, qreal x, qreal y, qreal z, qreal tol = 1e-6) {
        return qAbs(v.x() - x) < tol && qAbs(v.y() - y) < tol && qAbs(v.z() - z) < tol;
    }
}

// ===== 1. Clip → NDC：÷w；w<=0 → NaN 哨兵 =====
void TestQChartMath::clipToNdc_divideByW() {
    QVector3D ndc = QChartMath::clipToNdc(QVector4D(4.0f, 6.0f, 8.0f, 2.0f));
    QVERIFY(nearVec3(ndc, 2.0, 3.0, 4.0));

    // w=0 → NaN（相机背后/近平面外）
    QVector3D zero = QChartMath::clipToNdc(QVector4D(1.0f, 2.0f, 3.0f, 0.0f));
    QVERIFY(qIsNaN(zero.x()) && qIsNaN(zero.y()) && qIsNaN(zero.z()));

    // w<0 → NaN
    QVector3D neg = QChartMath::clipToNdc(QVector4D(1.0f, 2.0f, 3.0f, -5.0f));
    QVERIFY(qIsNaN(neg.x()) && qIsNaN(neg.y()) && qIsNaN(neg.z()));
}

// ===== 2. NDC → Screen：四角 + y 翻转（View 上 → 像素上）=====
void TestQChartMath::ndcToScreen_viewport() {
    const QRectF plot(0, 0, 400, 300);

    // (±1,±1) 四角 → plotArea 四角
    QVERIFY(nearPoint(QChartMath::ndcToScreen(QVector3D(-1, -1, 0), plot), 0, 300));   // 左下
    QVERIFY(nearPoint(QChartMath::ndcToScreen(QVector3D( 1, -1, 0), plot), 400, 300));  // 右下
    QVERIFY(nearPoint(QChartMath::ndcToScreen(QVector3D(-1,  1, 0), plot), 0, 0));      // 左上
    QVERIFY(nearPoint(QChartMath::ndcToScreen(QVector3D( 1,  1, 0), plot), 400, 0));    // 右上

    // 中心
    QVERIFY(nearPoint(QChartMath::ndcToScreen(QVector3D(0, 0, 0), plot), 200, 150));

    // y 翻转：ndc.y=+1（NDC 顶部）→ 像素 y=0（屏幕顶部），与 2D cartesianToPixel 一致
    QVERIFY(QChartMath::ndcToScreen(QVector3D(0, 1, 0), plot).y() <
            QChartMath::ndcToScreen(QVector3D(0, -1, 0), plot).y());
}

// ===== 3. Clip → Screen：正交矩阵下与手算一致（含 2D 对照）=====
void TestQChartMath::clipToScreen_roundtrip() {
    QMatrix4x4 ortho = QChartMath::orthographicMatrix(0, 10, 0, 10, -100, 100);
    const QRectF plot(0, 0, 10, 10);

    // world (2,3,0)：clip = (-0.6, -0.4, 0, 1) → screen (2, 7)
    QPointF s1 = QChartMath::clipToScreen(ortho * QVector4D(2.0f, 3.0f, 0.0f, 1.0f), plot);
    QVERIFY(nearPoint(s1, 2.0, 7.0));

    // world (7,1,0)：clip = (0.4, -0.8, 0, 1) → screen (7, 9)
    QPointF s2 = QChartMath::clipToScreen(ortho * QVector4D(7.0f, 1.0f, 0.0f, 1.0f), plot);
    QVERIFY(nearPoint(s2, 7.0, 9.0));

    // y 翻转与 2D cartesianToPixel 一致（正交俯视 ≡ 2D 线性映射，§2.3 精神）
    const QRectF viewRect(0, 0, 10, 10);
    QVERIFY(nearPoint(s1, QChartCamera2D::cartesianToPixel(viewRect, plot, 2, 3).x(),
                          QChartCamera2D::cartesianToPixel(viewRect, plot, 2, 3).y(), 1e-6));
    QVERIFY(nearPoint(s2, QChartCamera2D::cartesianToPixel(viewRect, plot, 7, 1).x(),
                          QChartCamera2D::cartesianToPixel(viewRect, plot, 7, 1).y(), 1e-6));

    // w<=0 → NaN 哨兵（clipToScreen 组合层也检查）
    QPointF nan1 = QChartMath::clipToScreen(QVector4D(0.0f, 0.0f, 0.0f, 0.0f), plot);
    QPointF nan2 = QChartMath::clipToScreen(QVector4D(1.0f, 1.0f, 1.0f, -1.0f), plot);
    QVERIFY(qIsNaN(nan1.x()) && qIsNaN(nan1.y()));
    QVERIFY(qIsNaN(nan2.x()) && qIsNaN(nan2.y()));
}

// ===== 4. 透视矩阵性质：near 平面缩放 = 1/tan(fov/2)，far 比例正确 =====
void TestQChartMath::perspectiveMatrix_properties() {
    const qreal fovY = 90.0, aspect = 2.0, nearP = 0.1, farP = 100.0;
    QMatrix4x4 m = QChartMath::perspectiveMatrix(fovY, aspect, nearP, farP);

    // 矩阵元：m00 = cot(fov/2)/aspect = 0.5；m11 = cot(45°) = 1（float 存储 → 1e-6 容差）
    QVERIFY(qAbs(m(0, 0) - 0.5) < 1e-6);
    QVERIFY(qAbs(m(1, 1) - 1.0) < 1e-6);
    QVERIFY(qAbs(m(2, 2) - (farP + nearP) / (nearP - farP)) < 1e-6);
    QVERIFY(qAbs(m(3, 2) + 1.0) < 1e-6);   // z 对 w 的贡献 -1

    // near 平面：y = tan(fov/2)·near → ndc.y = 1（近平面顶边）
    QVector4D clipNear = m * QVector4D(0.0f, 0.1f, -0.1f, 1.0f); // tan45°·0.1 = 0.1
    QVERIFY(qAbs(clipNear.w() - 0.1f) < 1e-6);
    QVERIFY(qAbs(clipNear.y() / clipNear.w() - 1.0) < 1e-6);

    // near 平面右缘：x = tan(fov/2)·near·aspect → ndc.x = 1
    QVector4D clipNearR = m * QVector4D(0.2f, 0.0f, -0.1f, 1.0f);
    QVERIFY(qAbs(clipNearR.x() / clipNearR.w() - 1.0) < 1e-6);

    // far/near 平面 z → ndc.z = +1 / -1
    QVector4D clipFar = m * QVector4D(0.0f, 0.0f, -100.0f, 1.0f);
    QVERIFY(qAbs(clipFar.z() / clipFar.w() - 1.0) < 1e-6);
    QVector4D clipN = m * QVector4D(0.0f, 0.0f, -0.1f, 1.0f);
    QVERIFY(qAbs(clipN.z() / clipN.w() + 1.0) < 1e-6);
}

// ===== 5. 正交矩阵性质：盒角点映射正确 =====
void TestQChartMath::orthographicMatrix_properties() {
    QMatrix4x4 m = QChartMath::orthographicMatrix(0, 10, 0, 10, -100, 100);

    // 矩阵元（float 存储 → 1e-6 容差）
    QVERIFY(qAbs(m(0, 0) - 0.2) < 1e-6);   // 2/width
    QVERIFY(qAbs(m(1, 1) - 0.2) < 1e-6);   // 2/height
    QVERIFY(qAbs(m(2, 2) + 0.01) < 1e-6);  // -2/(far-near)
    QVERIFY(qAbs(m(0, 3) + 1.0) < 1e-6);   // -(l+r)/(r-l)
    QVERIFY(qAbs(m(1, 3) + 1.0) < 1e-6);   // -(t+b)/(t-b)
    QVERIFY(qAbs(m(2, 3)) < 1e-6);         // -(n+f)/(f-n) = 0（对称 near/far）

    // 角点：world (0,0) → clip (-1,-1)；world (10,10) → clip (1,1)；中心 (5,5) → (0,0)
    QVector4D c00 = m * QVector4D(0.0f, 0.0f, 0.0f, 1.0f);
    QVERIFY(nearVec3(QVector3D(c00), -1.0, -1.0, 0.0));
    QVector4D c11 = m * QVector4D(10.0f, 10.0f, 0.0f, 1.0f);
    QVERIFY(nearVec3(QVector3D(c11), 1.0, 1.0, 0.0));
    QVector4D cc = m * QVector4D(5.0f, 5.0f, 0.0f, 1.0f);
    QVERIFY(nearVec3(QVector3D(cc), 0.0, 0.0, 0.0));

    // 屏幕：world (0,0) → 左下 (0,10)；world (10,10) → 右上 (10,0)
    const QRectF plot(0, 0, 10, 10);
    QVERIFY(nearPoint(QChartMath::clipToScreen(c00, plot), 0.0, 10.0));
    QVERIFY(nearPoint(QChartMath::clipToScreen(c11, plot), 10.0, 0.0));
}

// ===== 6. viewDepth：-viewZ，前方点 depth>0、越远越大（§3 公式）=====
void TestQChartMath::viewDepth_viewSpaceZ() {
    QMatrix4x4 view;
    view.lookAt(QVector3D(0, 0, 10), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    // 相机在 z=10 看向原点：前方点 viewZ<0 → depth = -viewZ > 0
    QVERIFY(QChartMath::viewDepth(view, QVector3D(0, 0, 0)) > 0.0);
    QVERIFY(QChartMath::viewDepth(view, QVector3D(0, 0, 9)) > 0.0);

    // 具体值：viewZ = z-10 → depth = 10-z
    QVERIFY(qAbs(QChartMath::viewDepth(view, QVector3D(0, 0, 0)) - 10.0) < 1e-6);
    QVERIFY(qAbs(QChartMath::viewDepth(view, QVector3D(0, 0, 9)) - 1.0) < 1e-6);
    QVERIFY(qAbs(QChartMath::viewDepth(view, QVector3D(0, 0, -5)) - 15.0) < 1e-6);

    // 排序键语义：越远（离相机越远）depth 越大 → 远→近排序 = depth 降序
    QVERIFY(QChartMath::viewDepth(view, QVector3D(0, 0, -5)) >
            QChartMath::viewDepth(view, QVector3D(0, 0, 0)));
    QVERIFY(QChartMath::viewDepth(view, QVector3D(0, 0, 0)) >
            QChartMath::viewDepth(view, QVector3D(0, 0, 9)));

    // 相机位置本身 → depth 0
    QVERIFY(qAbs(QChartMath::viewDepth(view, QVector3D(0, 0, 10))) < 1e-6);
}

// ===== 7. projectBatch：批量结果与逐点一致，两数组对齐 =====
void TestQChartMath::projectBatch_alignment() {
    QMatrix4x4 proj = QChartMath::orthographicMatrix(0, 10, 0, 10, -100, 100);
    QMatrix4x4 view; // 恒等
    const QRectF plot(0, 0, 100, 100);

    const QVector<QVector3D> world = {
        QVector3D(1, 1, 0), QVector3D(9, 7, 0), QVector3D(5, 5, 2), QVector3D(0, 0, 0)
    };

    QVector<QPointF> screen;
    QVector<qreal> depth;
    QChartMath::projectBatch(proj, view, plot, world, &screen, &depth);

    QCOMPARE(screen.size(), world.size());
    QCOMPARE(depth.size(), world.size());

    for (int i = 0; i < world.size(); ++i) {
        // 与逐点 clipToScreen 一致
        QPointF expected = QChartMath::clipToScreen(proj * QVector4D(world.at(i), 1.0f), plot);
        QVERIFY2(qAbs(screen.at(i).x() - expected.x()) < 1e-6 &&
                 qAbs(screen.at(i).y() - expected.y()) < 1e-6,
                 "批量屏幕点应与逐点投影一致");
        // 与逐点 viewDepth 一致（深度与屏幕点逐元素对齐）
        qreal expectedDepth = QChartMath::viewDepth(view, world.at(i));
        QVERIFY2(qAbs(depth.at(i) - expectedDepth) < 1e-6,
                 "批量深度应与逐点 viewDepth 一致");
    }
}

// ===== 8a. 柱坐标：(r,θ,z) 往返；r=0 → θ NaN =====
void TestQChartMath::cylindrical_roundtrip() {
    QChartCylindricalProjection3D proj;

    // 正向特殊点
    QVERIFY(nearVec3(proj.toWorld(2, 90, 5), 0.0, 2.0, 5.0));
    QVERIFY(nearVec3(proj.toWorld(3, 0, -1), 3.0, 0.0, -1.0));
    QVERIFY(nearVec3(proj.toWorld(1, 180, 2), -1.0, 0.0, 2.0));

    // 往返：r/z 精确恢复，θ 模 360°
    const qreal rs[] = { 1.0, 2.5, 10.0 };
    const qreal ths[] = { 0.0, 45.0, 90.0, 180.0, 270.0, 359.0 };
    const qreal zs[] = { -2.0, 0.0, 3.0 };
    for (qreal r : rs) {
        for (qreal th : ths) {
            for (qreal z : zs) {
                QVector3D w = proj.toWorld(r, th, z);
                QVector3D back = proj.fromWorld(w);
                QVERIFY(qAbs(back.x() - r) < 1e-5);            // r 恢复（float 存储）
                QVERIFY(qAbs(back.z() - z) < 1e-5);            // z 恢复
                qreal dtheta = back.y() - th;                  // θ 模 360°
                qreal wrapped = dtheta - qRound(dtheta / 360.0) * 360.0;
                QVERIFY(qAbs(wrapped) < 1e-4);
            }
        }
    }

    // 奇点：r=0 → θ NaN（z 保留）
    QVector3D pole = proj.fromWorld(QVector3D(0, 0, 7));
    QVERIFY(qIsNaN(pole.x()));
    QVERIFY(qAbs(pole.y()) < 1e-6);
    QVERIFY(qAbs(pole.z() - 7.0) < 1e-6);
}

// ===== 8b. 球坐标：(r,θ,φ) 往返；r=0 → θ/φ NaN =====
void TestQChartMath::spherical_roundtrip() {
    QChartSphericalProjection3D proj;

    // 正向特殊点
    QVERIFY(nearVec3(proj.toWorld(2, 0, 90), 0.0, 0.0, 2.0));
    QVERIFY(nearVec3(proj.toWorld(1, 90, 0), 0.0, 1.0, 0.0));
    QVERIFY(nearVec3(proj.toWorld(1, 0, -90), 0.0, 0.0, -1.0));

    // 往返：r/θ/φ 恢复
    const qreal rs[] = { 1.0, 3.0 };
    const qreal ths[] = { 0.0, 90.0, 180.0, 270.0, 359.0 };
    const qreal phis[] = { -60.0, 0.0, 45.0 };
    for (qreal r : rs) {
        for (qreal th : ths) {
            for (qreal ph : phis) {
                QVector3D w = proj.toWorld(r, th, ph);
                QVector3D back = proj.fromWorld(w);
                QVERIFY(qAbs(back.x() - r) < 1e-5);            // r 恢复（float 存储）
                qreal dtheta = back.y() - th;
                qreal wrapped = dtheta - qRound(dtheta / 360.0) * 360.0;
                QVERIFY(qAbs(wrapped) < 1e-4);                 // θ 恢复（模 360°）
                QVERIFY(qAbs(back.z() - ph) < 1e-4);           // φ 恢复
            }
        }
    }

    // 负 z 半球：θ=0, φ=-45
    QVector3D wneg = proj.toWorld(1, 0, -45);
    QVERIFY(qAbs(wneg.z() + 0.70710678) < 1e-6);
    QVector3D bneg = proj.fromWorld(wneg);
    QVERIFY(qAbs(bneg.x() - 1.0) < 1e-5);
    QVERIFY(qAbs(bneg.y()) < 1e-4);
    QVERIFY(qAbs(bneg.z() + 45.0) < 1e-4);

    // 奇点：r=0 → θ/φ NaN
    QVector3D pole = proj.fromWorld(QVector3D(0, 0, 0));
    QVERIFY(qIsNaN(pole.x()));
    QVERIFY(qIsNaN(pole.y()));
    QVERIFY(qAbs(pole.z()) < 1e-6);
}

// ===== 8c. 3D 笛卡尔：恒等 =====
void TestQChartMath::cartesian3d_identity() {
    QChartCartesianProjection3D proj;
    QVERIFY(nearVec3(proj.toWorld(1, 2, 3), 1, 2, 3));
    QVERIFY(nearVec3(proj.toWorld(-2.5, 0.5, 0), -2.5, 0.5, 0));
    QVERIFY(nearVec3(proj.toWorld(7, -8, 9), 7, -8, 9));
    QVERIFY(nearVec3(proj.toWorld(2.5, -1.5), 2.5, -1.5, 0.0));  // n2 缺省 → 0（2→3 嵌入）
    QVector3D w(4.0f, 5.0f, 6.0f);
    QVector3D back = proj.fromWorld(w);
    QVERIFY(nearVec3(back, 4, 5, 6));
    QCOMPARE(proj.dimensionName(0), QString("x"));
    QCOMPARE(proj.dimensionName(1), QString("y"));
    QCOMPARE(proj.dimensionName(2), QString("z"));
    QVERIFY(proj.dimensionName(3).isEmpty());
}

// ===== 8d. 函数式投影：莫比乌斯环（§5.3 可落地版本）=====
void TestQChartMath::functional3d_mobiusSamples() {
    auto mobius = std::make_unique<QChartFunctionalProjection3D>(
        [](qreal u, qreal v, qreal /*n2*/) -> QVector3D {
            const qreal R = 1.0;
            const qreal uRad = qDegreesToRadians(u);
            const qreal half = uRad * 0.5;                       // 半扭转
            return QVector3D((R + v * qCos(half)) * qCos(uRad),
                             (R + v * qCos(half)) * qSin(uRad),
                             v * qSin(half));
        },
        nullptr,                                       // 反向不提供
        QVector3D(0.0, -0.5, 0.0), QVector3D(360.0, 0.5, 0.0),
        nullptr, "u", "v", "w");

    // 精确采样点（v 轴对齐处 half=0）
    QVERIFY(nearVec3(mobius->toWorld(0, 0), 1.0, 0.0, 0.0));
    QVERIFY(nearVec3(mobius->toWorld(180, 0), -1.0, 0.0, 0.0));
    QVERIFY(nearVec3(mobius->toWorld(90, 0), 0.0, 1.0, 0.0));
    QVERIFY(nearVec3(mobius->toWorld(0, 0.5), 1.5, 0.0, 0.0));
    QVERIFY(nearVec3(mobius->toWorld(0, -0.5), 0.5, 0.0, 0.0));

    // 网格采样：|dist − R| ≤ 带宽容差（dist = 到 z 轴距离，|v·cos(u/2)| ≤ 0.5）
    const qreal tol = 0.5 + 1e-6;
    const qreal us[] = { 0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0 };
    const qreal vs[] = { -0.5, -0.25, 0.0, 0.25, 0.5 };
    for (qreal u : us) {
        for (qreal v : vs) {
            QVector3D w = mobius->toWorld(u, v);
            qreal dist = qSqrt(w.x() * w.x() + w.y() * w.y());
            QVERIFY2(qAbs(dist - 1.0) <= tol,
                     qPrintable(QString("莫比乌斯采样 |dist-R|=%1 超出容差 u=%2 v=%3")
                                .arg(qAbs(dist - 1.0)).arg(u).arg(v)));
        }
    }

    // 默认数据范围与维度名
    auto bounds = mobius->defaultDataBounds();
    QVERIFY(nearVec3(bounds.first, 0.0, -0.5, 0.0));
    QVERIFY(nearVec3(bounds.second, 360.0, 0.5, 0.0));
    QCOMPARE(mobius->dimensionName(0), QString("u"));
    QCOMPARE(mobius->dimensionName(1), QString("v"));
    QCOMPARE(mobius->dimensionName(2), QString("w"));

    // 反向未提供 → fromWorld 返回 NaN
    QVector3D nan = mobius->fromWorld(QVector3D(1, 0, 0));
    QVERIFY(qIsNaN(nan.x()) && qIsNaN(nan.y()) && qIsNaN(nan.z()));
}

// ===== 8e. computeWorldBounds：16×16×16 采样；全 NaN 兜底 =====
void TestQChartMath::computeWorldBounds_sampling() {
    // 笛卡尔：精确命中盒角点
    QChartCartesianProjection3D cart;
    QChartWorldBox cb = cart.computeWorldBounds(QVector3D(0, 0, 0), QVector3D(1, 2, 3));
    QVERIFY(nearVec3(cb.min, 0, 0, 0));
    QVERIFY(nearVec3(cb.max, 1, 2, 3));

    // 全 NaN 兜底：返回 dataMin/dataMax 原值
    auto allNaN = std::make_unique<QChartFunctionalProjection3D>(
        [](qreal, qreal, qreal) -> QVector3D {
            return QVector3D(qQNaN(), qQNaN(), qQNaN());
        });
    QChartWorldBox fb = allNaN->computeWorldBounds(QVector3D(-1, -2, -3), QVector3D(4, 5, 6));
    QVERIFY(nearVec3(fb.min, -1, -2, -3));
    QVERIFY(nearVec3(fb.max, 4, 5, 6));

    // 部分 NaN：n0<0 → NaN，采样后 x 范围应收缩为 [0, 1]
    auto halfNaN = std::make_unique<QChartFunctionalProjection3D>(
        [](qreal n0, qreal n1, qreal n2) -> QVector3D {
            if (n0 < 0.0) return QVector3D(qQNaN(), qQNaN(), qQNaN());
            return QVector3D(n0, n1, n2);
        });
    QChartWorldBox hb = halfNaN->computeWorldBounds(QVector3D(-1, -1, -1), QVector3D(1, 1, 1));
    QVERIFY(nearVec3(hb.min, 0.0, -1.0, -1.0));
    QVERIFY(nearVec3(hb.max, 1.0, 1.0, 1.0));

    // 圆柱整圆盘：r∈[0,1], θ∈[0,360] → x/y ∈ [-1,1]（16 网格含 0/90/180/270°），z ∈ [-1,1]
    QChartCylindricalProjection3D cyl;
    QChartWorldBox wb = cyl.computeWorldBounds(QVector3D(0, 0, -1), QVector3D(1, 360, 1));
    QVERIFY2(wb.min.x() <= -0.99 && wb.max.x() >= 0.99, "柱坐标 x 范围应覆盖 ±1");
    QVERIFY2(wb.min.y() <= -0.99 && wb.max.y() >= 0.99, "柱坐标 y 范围应覆盖 ±1");
    QVERIFY(qAbs(wb.min.z() + 1.0) < 1e-6 && qAbs(wb.max.z() - 1.0) < 1e-6);
    QVERIFY(wb.min.x() <= 0.0 && wb.max.x() >= 0.0);  // 含原点
}

// ===== 补项：samplingSegmentsHint（design_3d_axes.md §5.4）=====
void TestQChartMath::samplingHint() {
    QChartCartesianProjection3D cart;
    QCOMPARE(cart.samplingSegmentsHint(), 2);          // 恒等 → 两点直线

    QChartCylindricalProjection3D cyl;
    QChartSphericalProjection3D sph;
    QCOMPARE(cyl.samplingSegmentsHint(), 32);          // 弯曲投影默认 32
    QCOMPARE(sph.samplingSegmentsHint(), 32);

    QChartFunctionalProjection3D func(
        [](qreal u, qreal v, qreal) -> QVector3D {
            return QVector3D(u, v, 0);
        });
    QCOMPARE(func.samplingSegmentsHint(), 32);         // 函数式默认 32（不可断言直线）
}

// ===== 补项：isIdentityMapping（design_3d_axes.md §5.4 快速通道）=====
void TestQChartMath::identityFastPath() {
    QChartCartesianProjection3D cart;
    QVERIFY(cart.isIdentityMapping());                 // 恒等 → 快速通道

    QChartCylindricalProjection3D cyl;
    QChartSphericalProjection3D sph;
    QVERIFY(!cyl.isIdentityMapping());
    QVERIFY(!sph.isIdentityMapping());

    QChartFunctionalProjection3D func(
        [](qreal u, qreal v, qreal) -> QVector3D {
            return QVector3D(u, v, 0);
        });
    QVERIFY(!func.isIdentityMapping());                // 用户 lambda 不可假定恒等
}

// ===== 补项：unproject（design_3d_axes.md §2.3，Phase 3 预留；本任务实现并单测）=====
void TestQChartMath::unproject_roundtrip() {
    // 相机：lookAt 视图 + 透视/正交投影（与 §2.3 同构）
    QMatrix4x4 view;
    view.lookAt(QVector3D(0, 0, 10), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    // 透视：前方点 project → clip → unproject 还原
    {
        const QMatrix4x4 vp = QChartMath::perspectiveMatrix(45.0, 4.0 / 3.0, 0.1, 100.0) * view;
        const QVector3D worlds[] = {
            QVector3D(0, 0, 0), QVector3D(1, 2, 0), QVector3D(-0.5f, 1.5f, -3.0f),
            QVector3D(2, -1, 5), QVector3D(-3, 0.5f, 2),
        };
        for (const QVector3D& w : worlds) {
            const QVector4D clip = vp * QVector4D(w, 1.0f);
            QVERIFY2(clip.w() > 0.0f, "前方点 clip.w 应 > 0");
            const QVector3D back = QChartMath::unproject(vp, clip);
            // float 存储 → 1e-3 容差
            QVERIFY2(nearVec3(back, w.x(), w.y(), w.z(), 1e-3),
                     qPrintable(QString("透视 unproject 往返应还原 world (%1,%2,%3)→(%4,%5,%6)")
                                .arg(w.x()).arg(w.y()).arg(w.z())
                                .arg(back.x()).arg(back.y()).arg(back.z())));
        }
    }

    // 正交：同上
    {
        const QMatrix4x4 vp = QChartMath::orthographicMatrix(-10, 10, -10, 10, 0.1, 100.0) * view;
        const QVector3D worlds[] = {
            QVector3D(0, 0, 0), QVector3D(4, -3, 2), QVector3D(-6, 7, -4),
        };
        for (const QVector3D& w : worlds) {
            const QVector4D clip = vp * QVector4D(w, 1.0f);
            const QVector3D back = QChartMath::unproject(vp, clip);
            QVERIFY2(nearVec3(back, w.x(), w.y(), w.z(), 1e-3),
                     qPrintable(QString("正交 unproject 往返应还原 world (%1,%2,%3)")
                                .arg(w.x()).arg(w.y()).arg(w.z())));
        }
    }

    // w<=0 → NaN 哨兵（相机背后/近平面外 + 输入齐次 w<=0）
    {
        const QMatrix4x4 vp = QChartMath::perspectiveMatrix(45.0, 4.0 / 3.0, 0.1, 100.0) * view;
        QVector3D nan1 = QChartMath::unproject(vp, QVector4D(0, 0, 0, 0));
        QVERIFY(qIsNaN(nan1.x()) && qIsNaN(nan1.y()) && qIsNaN(nan1.z()));
        QVector3D nan2 = QChartMath::unproject(vp, QVector4D(1, 1, 1, -1));
        QVERIFY(qIsNaN(nan2.x()) && qIsNaN(nan2.y()) && qIsNaN(nan2.z()));
        // 相机背后点：clip.w<0 → NaN
        QVector4D behind = vp * QVector4D(0, 0, 20, 1);   // 相机在 z=10 后方
        QVERIFY(behind.w() < 0.0f);
        QVector3D nan3 = QChartMath::unproject(vp, behind);
        QVERIFY(qIsNaN(nan3.x()) && qIsNaN(nan3.y()) && qIsNaN(nan3.z()));
    }

    // 不可逆矩阵 → NaN
    {
        QMatrix4x4 singular;
        singular.fill(0.0f);
        QVector3D nan = QChartMath::unproject(singular, QVector4D(1, 1, 1, 1));
        QVERIFY(qIsNaN(nan.x()) && qIsNaN(nan.y()) && qIsNaN(nan.z()));
    }
}

// ============================================================
// Phase 3：数值预转换缓存 + worldCache（design_phase3.md §9 / §13.2，t51）
// ============================================================
// ===== 1. 数值型 append → float3 权威存储（12B/点）激活 =====
void TestQChartMath::numericCache_numericAppends() {
    // A3 结构性前提：float3 = 12B/点，远小于 QVariant 三元组（QDataPoint3D 3×QVariant）
    QCOMPARE(int(sizeof(QVector3D)), 12);
    QVERIFY2(sizeof(QVector3D) < sizeof(QDataPoint3D),
             "float3 权威存储必须小于 QVariant 三元组（A3 内存预算前提）");

    QChartScatterSeries3D s;
    s.append(1.0, 2.0, 3.0);
    s.append(-4.5, 0.0, 7.25);
    s.append(100.0, -200.0, 0.5);

    QVERIFY(s.numericCacheActive());                        // 全部数值型 append → true
    QCOMPARE(s.numericCache().size(), 3);                   // float3 权威存储
    QCOMPARE(s.numericCache().size() * int(sizeof(QVector3D)), 36);   // 12B/点 内存核算
    QCOMPARE(s.count(), 3);

    // 值 = append 的 (x,y,z)
    QVERIFY(nearVec3(s.numericCache().at(0), 1.0, 2.0, 3.0));
    QVERIFY(nearVec3(s.numericCache().at(1), -4.5, 0.0, 7.25));
    QVERIFY(nearVec3(s.numericCache().at(2), 100.0, -200.0, 0.5));
}

// ===== 2. QVariant 路径不激活；混合回退保持顺序 =====
void TestQChartMath::numericCache_qvariantPath() {
    QChartScatterSeries3D s;
    s.append(QVariant(1.0), QVariant(2.0), QVariant(3.0));   // QVariant 路径（Phase 2 边界）
    QVERIFY(!s.numericCacheActive());                        // 不激活
    QCOMPARE(s.numericCache().size(), 0);
    QCOMPARE(s.count(), 1);

    // 混合：数值型 append 在 QVariant 之后 → 仍不激活（回退 QDataPoint3D 列表，顺序语义保持）
    s.append(4.0, 5.0, 6.0);
    QVERIFY(!s.numericCacheActive());
    QCOMPARE(s.count(), 2);
    QCOMPARE(s.at(0).x(), QVariant(1.0));
    QCOMPARE(s.at(1).x(), QVariant(4.0));
    QCOMPARE(s.points().size(), 2);

    // setPoints 批量：QVariant 路径 → 不激活
    QVector<QDataPoint3D> pts;
    pts.append(QDataPoint3D(7.0, 8.0, 9.0));
    s.setPoints(pts);
    QVERIFY(!s.numericCacheActive());
    QCOMPARE(s.count(), 1);
    QCOMPARE(s.at(0).z(), QVariant(9.0));

    // append(QDataPoint3D) 同属 QVariant 路径 → 不激活
    QChartLineSeries3D s2;
    s2.append(1, 2, 3);                                     // 数值型先行
    QVERIFY(s2.numericCacheActive());
    s2.append(QDataPoint3D(QVariant(4.0), QVariant(5.0), QVariant(6.0)));
    QVERIFY(!s2.numericCacheActive());                      // QDataPoint3D 注入 → 回退
    QCOMPARE(s2.count(), 2);
    QCOMPARE(s2.at(1).x(), QVariant(4.0));
}

// ===== 3. clear 复位 / replace·insert 失效 / remove 增量维护 =====
void TestQChartMath::numericCache_clearReplaceInvalidate() {
    // 数值型 → replace 失效回退（任务定：clear/replace 失效）：active=false，数据完整
    QChartLineSeries3D s;
    s.append(1, 2, 3);
    s.append(4, 5, 6);
    QVERIFY(s.numericCacheActive());
    s.replace(1, QDataPoint3D(QVariant(40.0), QVariant(50.0), QVariant(60.0)));
    QVERIFY(!s.numericCacheActive());                       // replace 失效
    QCOMPARE(s.count(), 2);
    QCOMPARE(s.at(1).x(), QVariant(40.0));
    QCOMPARE(s.points().size(), 2);                         // 物化完整（含被替换点）

    // 数值型 → insert 注入 QVariant 点 → 回退（顺序语义保持）
    QChartScatterSeries3D s2;
    s2.append(1, 2, 3);
    s2.insert(0, QDataPoint3D(QVariant(0.0), QVariant(0.0), QVariant(0.0)));
    QVERIFY(!s2.numericCacheActive());
    QCOMPARE(s2.count(), 2);
    QCOMPARE(s2.at(0).x(), QVariant(0.0));
    QCOMPARE(s2.at(1).x(), QVariant(1.0));

    // 数值型 → remove 增量维护（免回退）
    QChartScatterSeries3D s3;
    s3.append(1, 2, 3);
    s3.append(4, 5, 6);
    s3.append(7, 8, 9);
    s3.remove(1);
    QVERIFY(s3.numericCacheActive());                       // remove 保持数值型
    QCOMPARE(s3.count(), 2);
    QVERIFY(nearVec3(s3.numericCache().at(1), 7.0, 8.0, 9.0));
    QCOMPARE(s3.at(1).x(), QVariant(7.0));

    // clear → 失效复位：空白、恢复数值型容量（下次 append 决定模式）
    s3.clear();
    QCOMPARE(s3.count(), 0);
    QVERIFY(s3.numericCacheActive());
    QCOMPARE(s3.numericCache().size(), 0);
    s3.append(1.5, 2.5, 3.5);
    QVERIFY(s3.numericCacheActive());
    QVERIFY(nearVec3(s3.numericCache().at(0), 1.5, 2.5, 3.5));
}

// ===== 4. points()/at()/count() API 语义与 Phase 2 一致（QVariant 按需物化）=====
void TestQChartMath::numericCache_apiSemantics() {
    QChartScatterSeries3D s;
    s.append(1.0, 2.0, 3.0);
    s.append(4.0, 5.0, 6.0);

    const QVector<QDataPoint3D>& pts = s.points();          // 按需物化视图
    QCOMPARE(pts.size(), 2);
    QCOMPARE(pts.at(0).x(), QVariant(1.0));
    QCOMPARE(pts.at(1).y(), QVariant(5.0));
    QCOMPARE(s.count(), 2);
    QCOMPARE(s.at(1).z(), QVariant(6.0));

    // 物化后 append → 视图失效；下次 points() 重新物化反映新数据
    s.append(7.0, 8.0, 9.0);
    QCOMPARE(s.count(), 3);
    QCOMPARE(s.points().size(), 3);
    QCOMPARE(s.points().at(2).x(), QVariant(7.0));
    QVERIFY(s.numericCacheActive());                         // 仍数值型
    QCOMPARE(s.numericCache().size(), 3);

    // OOB at() → 空点（Phase 2 语义）
    QDataPoint3D oob = s.at(99);
    QVERIFY(!oob.x().isValid());

    // collectPrimitives 在数值型模式下产生与 Phase 2 一致的图元（dataIndex/depth/screen）
    ProjectFn3D fn = [](const QDataPoint3D& d) {
        return QChartProjectedPoint{ QPointF(d.x().toDouble(), d.y().toDouble()), d.z().toDouble() };
    };
    QVector<QChartPrimitive> out;
    s.collectPrimitives(fn, out);
    QCOMPARE(out.size(), 3);
    QCOMPARE(out[2].dataIndex, 2);
    QCOMPARE(out[2].depth, 9.0);
    QVERIFY(qAbs(out[2].a.x() - 7.0) < 1e-6);
}

// ===== 5. worldCache：Layer3D 渲染时填充，= toWorld(numericCache)（VBO 源）=====
void TestQChartMath::worldCache_filledToWorld() {
    QChartCamera3D cam;
    QChartLayer3D layer;
    QChartCylindricalProjection3D cyl;
    layer.setProjection3D(&cyl);

    auto* s = new QChartScatterSeries3D("pts");   // 堆分配：所有权归 layer（dtor qDeleteAll）
    s->append(2.0, 0.0, 5.0);        // 柱坐标 (2,0°,5) → (2, 0, 5)
    s->append(1.0, 90.0, -1.0);      // (1,90°,-1) → (0, 1, -1)
    s->append(3.0, 180.0, 0.0);      // (3,180°,0) → (-3, 0, 0)
    layer.addSeries3D(s);

    QVector<QChartPrimitive> out;
    const QRectF plot(0, 0, 400, 300);
    layer.collectPrimitives(&cam, plot, out);   // 渲染时填充

    QVERIFY(s->numericCacheActive());
    QCOMPARE(s->worldCache().size(), 3);
    for (int i = 0; i < 3; ++i) {
        const QVector3D& n = s->numericCache().at(i);
        const QVector3D expected = cyl.toWorld(n.x(), n.y(), n.z());
        QVERIFY2(qAbs(s->worldCache().at(i).x() - expected.x()) < 1e-5 &&
                 qAbs(s->worldCache().at(i).y() - expected.y()) < 1e-5 &&
                 qAbs(s->worldCache().at(i).z() - expected.z()) < 1e-5,
                 "worldCache 应等于 toWorld(numericCache)（逐点对照）");
    }
    // 具体值抽查：柱坐标 (r,θ,z) → (r·cosθ, r·sinθ, z)
    QVERIFY(qAbs(s->worldCache().at(0).x() - 2.0) < 1e-5);
    QVERIFY(qAbs(s->worldCache().at(1).y() - 1.0) < 1e-5);
    QVERIFY(qAbs(s->worldCache().at(2).x() + 3.0) < 1e-5);

    // QVariant 路径（混合）系列 → 无 worldCache（Phase 2 边界）
    auto* m = new QChartLineSeries3D("mixed");
    m->append(QVariant(1.0), QVariant(2.0), QVariant(3.0));
    m->append(4.0, 5.0, 6.0);
    layer.addSeries3D(m);
    layer.collectPrimitives(&cam, plot, out);   // addSeries3D 置脏 → 重建
    QVERIFY(!m->numericCacheActive());
    QCOMPARE(m->worldCache().size(), 0);        // 混合路径不填充
    QCOMPARE(s->worldCache().size(), 3);        // 数值型系列重建保持
}

// ===== 6. worldCache：数据/投影变化 → 失效并重建（§9 失效策略）=====
void TestQChartMath::worldCache_invalidatedOnChange() {
    QChartCamera3D cam;
    QChartLayer3D layer;
    QChartCartesianProjection3D cart;
    layer.setProjection3D(&cart);

    auto* s = new QChartScatterSeries3D("pts");
    s->append(1, 2, 3);
    layer.addSeries3D(s);

    QVector<QChartPrimitive> out;
    const QRectF plot(0, 0, 400, 300);
    layer.collectPrimitives(&cam, plot, out);
    QCOMPARE(s->worldCache().size(), 1);

    // 数据变化 → 本类自清 + Layer3D 置脏；下次渲染重建（投影/数据变化才重建）
    s->append(4, 5, 6);
    QCOMPARE(s->worldCache().size(), 0);        // 数据变化 → 立即失效
    layer.collectPrimitives(&cam, plot, out);
    QCOMPARE(s->worldCache().size(), 2);        // 重建（尺寸跟随）
    QVERIFY(nearVec3(s->worldCache().at(1), 4, 5, 6));

    // 投影变化 → 置脏重建（换柱坐标）
    QChartCylindricalProjection3D cyl;
    layer.setProjection3D(&cyl);
    layer.collectPrimitives(&cam, plot, out);
    QCOMPARE(s->worldCache().size(), 2);
    const QVector3D expected = cyl.toWorld(4.0f, 5.0f, 6.0f);
    QVERIFY(qAbs(s->worldCache().at(1).x() - expected.x()) < 1e-5 &&
            qAbs(s->worldCache().at(1).y() - expected.y()) < 1e-5 &&
            qAbs(s->worldCache().at(1).z() - expected.z()) < 1e-5);
}
