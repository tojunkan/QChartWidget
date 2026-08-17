// test_qchartmath.cpp —— QChartMath + Projection3D 家族单元测试
// 覆盖（design_3d.md §11.1 TestQChartMath 全部 8 组）：
//   clipToNdc/ndcToScreen/clipToScreen、透视/正交矩阵性质、viewDepth、projectBatch 对齐
//   + Projection3D 家族：圆柱/球/笛卡尔/函数式映射、莫比乌斯采样、computeWorldBounds 兜底
#include <QtTest>
#include <memory>
#include "../../QChartMath.h"
#include "../../QChartProjection3D.h"
#include "../../QChartCartesianProjection3D.h"
#include "../../QChartCylindricalProjection3D.h"
#include "../../QChartSphericalProjection3D.h"
#include "../../QChartFunctionalProjection3D.h"
#include "../../QChartCamera.h"   // 仅用 QChartCamera2D::cartesianToPixel 做 y 翻转一致性对照
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
