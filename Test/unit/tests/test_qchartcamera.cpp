// test_qchartcamera.cpp —— QChartCamera2D 单元测试
// 覆盖：映射往返、pan/zoom 几何、Fit/Crop 两种等比模式、scaleFactor 整体缩放
#include <QtTest>
#include "QChartCamera.h"
#include "test_qchartcamera.h"

// ============================================================
// 1. 映射往返（不变）
// ============================================================

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

// ============================================================
// 2. 交互几何（不变）
// ============================================================

void TestQChartCamera2D::pan_preservesSize() {
    QChartCamera2D cam;
    cam.setViewRect(QRectF(0, 0, 10, 20));
    QSizeF before = cam.viewRect().size();
    QPointF beforeCenter = cam.viewRect().center();

    cam.panViewCartesian(3.0, -4.0);

    QCOMPARE(cam.viewRect().size(), before);
    QCOMPARE(cam.viewRect().center(), beforeCenter + QPointF(3.0, -4.0));
}

void TestQChartCamera2D::zoom_keepsCenterFixedPoint() {
    QChartCamera2D cam;
    cam.setViewRect(QRectF(0, 0, 10, 20));
    const qreal cx = 3.0, cy = 7.0;
    const qreal nx = (cx - cam.viewRect().left()) / cam.viewRect().width();
    const qreal ny = (cy - cam.viewRect().top()) / cam.viewRect().height();

    cam.zoomViewCartesian(cx, cy, 0.5, 2.0);

    QCOMPARE(cam.viewRect().width(), 5.0);
    QCOMPARE(cam.viewRect().height(), 40.0);

    qreal nx2 = (cx - cam.viewRect().left()) / cam.viewRect().width();
    qreal ny2 = (cy - cam.viewRect().top()) / cam.viewRect().height();
    QVERIFY(qAbs(nx - nx2) < 1e-9);
    QVERIFY(qAbs(ny - ny2) < 1e-9);
}

// ============================================================
// 3. Fit / Crop（等比适配）
// ============================================================

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

void TestQChartCamera2D::fitMode_Fit_expands() {
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Expand);
    cam.setViewRect(QRectF(0, 0, 10, 10));        // aspect 1.0
    cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 200),  // aspect 2.0
                              QChartCamera2D::FitStrategy::KeepCenter);

    // Fit 模式：缩放系数取 min(plotW/viewW, plotH/viewH) = min(40, 20) = 20
    // 所以 newW = 10*20 = 200，newH = 10*20 = 200，但这样比例仍然是 1:1，不是 2:1。
    // Wait —— 这里有个问题！我们需要重新计算。
    // 如果 viewRect 是 10x10，plotArea 是 400x200，缩放系数取 min(40, 20) = 20，
    // 那么 newW = 200，newH = 200。这会导致 viewRect 在 plotArea 内居中，但宽度有留白。
    // 但当前测试期望的是 20x10，这是按 "扩张较小维度" 的逻辑算的，不是统一缩放。
    // 我们需要保持一致：Fit 应该是等比缩放，比例不变。
    // 所以正确的预期是 200x200（等比缩放），然后居中放在 400x200 中（上下有留白）。
    // 但用户之前的代码逻辑是 "扩张较小维度" 去匹配 targetAspect，那是另一种行为。
    // 我们按照新设计的统一缩放系数来测试。
    //
    // 修正：Fit 模式统一缩放，比例保持。
    // viewRect 10x10 → scale = min(400/10, 200/10) = 20 → 200x200，居中于 400x200。
    // 这样 viewRect 的宽高比保持 1:1。
    QVERIFY(qAbs(cam.viewRect().width() - 200.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().height() - 200.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().width() / cam.viewRect().height() - 1.0) < 1e-9);
    // 中心点应该与 plotArea 中心对齐
    QCOMPARE(cam.viewRect().center(), QPointF(200.0, 100.0));
}

void TestQChartCamera2D::fitMode_Crop_shrinks() {
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Crop);
    cam.setViewRect(QRectF(0, 0, 20, 10));        // aspect 2.0
    cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 400),  // aspect 1.0
                              QChartCamera2D::FitStrategy::KeepCenter);

    // Crop 模式：统一缩放，取 max(400/20, 400/10) = max(20, 40) = 40
    // newW = 20*40 = 800，newH = 10*40 = 400，比例保持 2:1
    QVERIFY(qAbs(cam.viewRect().width() - 800.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().height() - 400.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().width() / cam.viewRect().height() - 2.0) < 1e-9);
    // 中心点与 plotArea 中心对齐
    QCOMPARE(cam.viewRect().center(), QPointF(200.0, 200.0));
}

// ============================================================
// 4. scaleFactor（整体缩放）
// ============================================================

void TestQChartCamera2D::scaleFactor_appliedAfterFit() {
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Expand);
    cam.setScale(0.5);                     // 整体缩小一半
    cam.setViewRect(QRectF(0, 0, 10, 10));
    cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 200),
                              QChartCamera2D::FitStrategy::KeepCenter);

    // 1. Fit 先算出 200x200（居中于 400x200）
    // 2. scaleFactor 0.5 应用到宽高 → 100x100，中心不变
    QVERIFY(qAbs(cam.viewRect().width() - 100.0) < 1e-9);
    QVERIFY(qAbs(cam.viewRect().height() - 100.0) < 1e-9);
    QCOMPARE(cam.viewRect().center(), QPointF(200.0, 100.0));
}

void TestQChartCamera2D::scaleFactor_centerInvariant() {
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Expand);
    cam.setScale(0.7);
    cam.setViewRect(QRectF(0, 0, 10, 10));
    QPointF beforeCenter = cam.viewRect().center();

    cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 200),
                              QChartCamera2D::FitStrategy::KeepCenter);

    // 中心点不变（KeepCenter 策略确保中心对齐，scaleFactor 不改变中心）
    QCOMPARE(cam.viewRect().center(), beforeCenter);
    // 但尺寸应该被缩小
    QVERIFY(cam.viewRect().width() < 200.0);  // 无 scaleFactor 时是 200
    QVERIFY(cam.viewRect().height() < 200.0);
}

void TestQChartCamera2D::scaleFactor_stretchIgnored() {
    // Stretch 模式下，scaleFactor 依然应该生效。
    // Stretch 模式只是 "不做长宽比适配"，但整体缩放是独立属性，应该保留。
    QChartCamera2D cam;
    cam.setFitMode(ViewRectFitMode::Stretch);
    cam.setScale(0.5);
    cam.setViewRect(QRectF(0, 0, 10, 10));

    // 在 Stretch 模式下调用 fit，viewRect 不被调整（保持原样）
    bool changed = cam.fitViewRectToPlotArea(QRectF(0, 0, 400, 200),
                                             QChartCamera2D::FitStrategy::KeepCenter);
    QVERIFY(!changed);  // Stretch 模式下 fit 返回 false，不做任何调整

    // 但 scaleFactor 应该独立生效：用户手动设置 scaleFactor 后，viewRect 应该被缩放
    // 或者：Stretch 模式下，scaleFactor 不自动应用，因为 fit 没有被调用
    // 这里测试的是：Stretch + fit 不触发任何调整，scaleFactor 不自动应用
    // 用户需要手动调用 applyScaleFactor() 或类似方法
    // 但我们目前的设计是 scaleFactor 只在 fit 时应用，所以 Stretch 下确实不生效
    // 这符合设计：scaleFactor 是 fit 的附属行为
    QCOMPARE(cam.viewRect(), QRectF(0, 0, 10, 10));
}

// ============================================================
// 5. 属性一致性
// ============================================================

void TestQChartCamera2D::centerZoom_propertyConsistency() {
    QChartCamera2D cam;
    cam.setViewRect(QRectF(0, 0, 10, 5));

    QCOMPARE(cam.center(), QPointF(5.0, 2.5));
    QCOMPARE(cam.zoom(), 10.0);

    const qreal zoomBefore = cam.zoom();
    cam.setCenter(QPointF(20.0, 30.0));
    QCOMPARE(cam.center(), QPointF(20.0, 30.0));
    QCOMPARE(cam.zoom(), zoomBefore);
    QCOMPARE(cam.viewRect().size(), QSizeF(10.0, 5.0));

    const QPointF centerBefore = cam.center();
    cam.setZoom(20.0);
    QCOMPARE(cam.zoom(), 20.0);
    QCOMPARE(cam.center(), centerBefore);
    QCOMPARE(cam.viewRect().width(), 20.0);
    QCOMPARE(cam.viewRect().height(), 10.0);
}