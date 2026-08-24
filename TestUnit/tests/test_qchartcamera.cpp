// test_qchartcamera.cpp —— QChartCamera2D 单元测试
// 覆盖：映射往返、pan/zoom 几何、四种 fit 模式、center/zoom 属性一致性
#include <QtTest>
#include "QChartCamera.h"
#include "test_qchartcamera.h"

// ===== View Cartesian → Pixel → View Cartesian 往返恒等 =====
void TestQChartCamera2D::cartesianToPixel_roundtrip() {
    const QRectF viewRects[] = {
        QRectF(0, 0, 10, 10),
        QRectF(-5, -5, 10, 10),
        QRectF(2, 3, 8, 4),
        QRectF(-100, 50, 200, 40),
    };
    const QRectF plotAreas[] = {
        QRectF(0, 0, 400, 300),
        QRectF(20, 20, 360, 260),
        QRectF(-10, -10, 500, 500),
    };

    for (const QRectF& vr : viewRects) {
        for (const QRectF& pa : plotAreas) {
            for (int i = 0; i <= 10; ++i) {
                for (int j = 0; j <= 10; ++j) {
                    qreal cx = vr.left() + vr.width() * i / 10.0;
                    qreal cy = vr.top() + vr.height() * j / 10.0;
                    QPointF px = QChartCamera2D::cartesianToPixel(vr, pa, cx, cy);
                    QPointF back = QChartCamera2D::pixelToCartesian(vr, pa, px);
                    QVERIFY2(qAbs(back.x() - cx) < 1e-9 && qAbs(back.y() - cy) < 1e-9,
                             "cartesianToPixel∘pixelToCartesian 应为恒等");
                }
            }
        }
    }
}

// ===== Pixel → View Cartesian → Pixel 往返恒等 =====
void TestQChartCamera2D::pixelToCartesian_roundtrip() {
    const QRectF viewRects[] = { QRectF(0, 0, 10, 10), QRectF(-5, -5, 10, 10) };
    const QRectF plotAreas[] = { QRectF(0, 0, 400, 300), QRectF(20, 20, 360, 260) };

    for (const QRectF& vr : viewRects) {
        for (const QRectF& pa : plotAreas) {
            for (int i = 0; i <= 8; ++i) {
                for (int j = 0; j <= 8; ++j) {
                    QPointF px(pa.left() + pa.width() * i / 8.0,
                               pa.top() + pa.height() * j / 8.0);
                    QPointF c = QChartCamera2D::pixelToCartesian(vr, pa, px);
                    QPointF back = QChartCamera2D::cartesianToPixel(vr, pa, c.x(), c.y());
                    QVERIFY2(qAbs(back.x() - px.x()) < 1e-9 && qAbs(back.y() - px.y()) < 1e-9,
                             "pixelToCartesian∘cartesianToPixel 应为恒等");
                }
            }
        }
    }
}

// ===== pan 只平移、不改尺寸 =====
void TestQChartCamera2D::pan_preservesSize() {
    QChartCamera2D cam;
    cam.setViewRect(QRectF(0, 0, 10, 20));
    QSizeF before = cam.viewRect().size();
    QPointF beforeCenter = cam.viewRect().center();

    cam.panViewCartesian(3.0, -4.0);

    QCOMPARE(cam.viewRect().size(), before);
    QCOMPARE(cam.viewRect().center(), beforeCenter + QPointF(3.0, -4.0));
}

// ===== zoom 以 (cx,cy) 为中心：该点的归一化位置不变（不动点）=====
void TestQChartCamera2D::zoom_keepsCenterFixedPoint() {
    QChartCamera2D cam;
    cam.setViewRect(QRectF(0, 0, 10, 20));
    const qreal cx = 3.0, cy = 7.0;
    const qreal nx = (cx - cam.viewRect().left()) / cam.viewRect().width();
    const qreal ny = (cy - cam.viewRect().top()) / cam.viewRect().height();

    cam.zoomViewCartesian(cx, cy, 0.5, 2.0);

    // 尺寸按 factor 缩放
    QCOMPARE(cam.viewRect().width(), 5.0);
    QCOMPARE(cam.viewRect().height(), 40.0);

    // (cx,cy) 在新 viewRect 中的归一化位置与缩放前一致
    qreal nx2 = (cx - cam.viewRect().left()) / cam.viewRect().width();
    qreal ny2 = (cy - cam.viewRect().top()) / cam.viewRect().height();
    QVERIFY(qAbs(nx - nx2) < 1e-9);
    QVERIFY(qAbs(ny - ny2) < 1e-9);
}

// ===== Stretch：不做拟合，viewRect 不变 =====
void TestQChartCamera2D::fitMode_Stretch_noChange() {
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Stretch);
    cam.setViewRect(QRectF(0, 0, 10, 10));
    QRectF before = cam.viewRect();

    bool changed = cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 200),
                                             QChartCamera2D::FitStrategy::KeepCenter);
    QVERIFY(!changed);
    QCOMPARE(cam.viewRect(), before);
}

// ===== Fit：扩张较小维度匹配 plotArea 长宽比，数据完整 =====
void TestQChartCamera2D::fitMode_Fit_expands() {
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Fit);
    cam.setViewRect(QRectF(0, 0, 10, 10));        // aspect 1.0
    cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 200),  // aspect 2.0
                              QChartCamera2D::FitStrategy::KeepCenter);
    // 目标 2.0 > 当前 1.0 → 扩张宽度到 20（高度不变）
    QVERIFY(qAbs(cam.viewRect().width() - 20.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().height() - 10.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().width() / cam.viewRect().height() - 2.0) < 1e-9);
}

// ===== Crop：收缩较大维度匹配 plotArea 长宽比 =====
void TestQChartCamera2D::fitMode_Crop_shrinks() {
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Crop);
    cam.setViewRect(QRectF(0, 0, 20, 10));        // aspect 2.0
    cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 400),  // aspect 1.0
                              QChartCamera2D::FitStrategy::KeepCenter);
    // 太宽 → 收缩宽度到 10
    QVERIFY(qAbs(cam.viewRect().width() - 10.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().height() - 10.0) < 1e-9);
}

// ===== Fixed：匹配 fixedAspectRatio，忽略 plotArea =====
void TestQChartCamera2D::fitMode_Fixed_aspectRatio() {
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Fixed);
    cam.setFixedAspectRatio(0.5);
    cam.setViewRect(QRectF(0, 0, 10, 10));        // aspect 1.0
    cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 200),  // aspect 2.0（被忽略）
                              QChartCamera2D::FitStrategy::KeepCenter);
    // 目标 0.5 < 当前 1.0 → 扩张高度到 20（宽度不变）
    QVERIFY(qAbs(cam.viewRect().width() - 10.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().height() - 20.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().width() / cam.viewRect().height() - 0.5) < 1e-9);
}

// ===== center/zoom 属性与 viewRect 一致性 =====
void TestQChartCamera2D::centerZoom_propertyConsistency() {
    QChartCamera2D cam;
    cam.setViewRect(QRectF(0, 0, 10, 5));  // aspect 2.0, center (5, 2.5)

    QCOMPARE(cam.center(), QPointF(5.0, 2.5));
    QCOMPARE(cam.zoom(), 10.0);

    // setCenter：center 变、zoom 与尺寸不变
    const qreal zoomBefore = cam.zoom();
    cam.setCenter(QPointF(20.0, 30.0));
    QCOMPARE(cam.center(), QPointF(20.0, 30.0));
    QCOMPARE(cam.zoom(), zoomBefore);
    QCOMPARE(cam.viewRect().size(), QSizeF(10.0, 5.0));

    // setZoom：zoom 变、center 不变、长宽比保持
    const QPointF centerBefore = cam.center();
    cam.setZoom(20.0);
    QCOMPARE(cam.zoom(), 20.0);
    QCOMPARE(cam.center(), centerBefore);
    QCOMPARE(cam.viewRect().width(), 20.0);
    QCOMPARE(cam.viewRect().height(), 10.0);  // aspect 2.0 保持
}
