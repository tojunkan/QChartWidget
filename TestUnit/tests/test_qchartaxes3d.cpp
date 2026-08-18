// test_qchartaxes3d.cpp —— QChartAxes3D 编排器单元测试
// 覆盖（design_3d_axes.md §10.1 全部 8 例）；三层分离红线：本类无 QPainter/相机/投影引用。
#include <QtTest>
#include "../../QChartAxes3D.h"
#include "../../QChartAxis.h"
#include "../../QValueAxis.h"
#include "test_qchartaxes3d.h"

// ===== 1. boxCorners：8 角约定 index = u | (v<<1) | (w<<2) =====
void TestQChartAxes3D::boxCorners_convention() {
    const QVector3D minV(0, 0, 0), maxV(10, 20, 30);
    const QVector<QVector3D> c = QChartAxes3D::boxCorners(minV, maxV);
    QCOMPARE(c.size(), 8);

    // 手算对照（bit0=u、bit1=v、bit2=w；置位取 max 分量）
    const QVector3D expected[] = {
        QVector3D( 0,  0,  0),   // 0: 000
        QVector3D(10,  0,  0),   // 1: 001 (u)
        QVector3D( 0, 20,  0),   // 2: 010 (v)
        QVector3D(10, 20,  0),   // 3: 011
        QVector3D( 0,  0, 30),   // 4: 100 (w)
        QVector3D(10,  0, 30),   // 5: 101
        QVector3D( 0, 20, 30),   // 6: 110
        QVector3D(10, 20, 30),   // 7: 111
    };
    for (int i = 0; i < 8; ++i)
        QCOMPARE(c.at(i), expected[i]);
}

// ===== 2. boxEdges：12 条边、u/v/w 各 4 平行、端点恰差一个分量 =====
void TestQChartAxes3D::boxEdges_12() {
    const QVector<QPair<int, int>> edges = QChartAxes3D::boxEdges();
    QCOMPARE(edges.size(), 12);

    const QVector<QVector3D> c = QChartAxes3D::boxCorners(QVector3D(0, 0, 0), QVector3D(1, 2, 3));
    int uCount = 0, vCount = 0, wCount = 0;
    for (const auto& e : edges) {
        const QVector3D a = c.at(e.first), b = c.at(e.second);
        // 恰差一个分量
        const int diff = (a.x() != b.x()) + (a.y() != b.y()) + (a.z() != b.z());
        QCOMPARE(diff, 1);
        // 方向分类（§8.2 约定：0-3=u∥、4-7=v∥、8-11=w∥）
        const int idx = edges.indexOf(e);
        if (idx < 4) { QCOMPARE(a.x() != b.x(), true); ++uCount; }
        else if (idx < 8) { QCOMPARE(a.y() != b.y(), true); ++vCount; }
        else { QCOMPARE(a.z() != b.z(), true); ++wCount; }
    }
    QCOMPARE(uCount, 4);
    QCOMPARE(vCount, 4);
    QCOMPARE(wCount, 4);
}

// ===== 3. spineEdges：3 条 spine 均含角 0（min 角）、分别沿 u/v/w =====
void TestQChartAxes3D::spineEdges_fromMinCorner() {
    const QVector<int> spine = QChartAxes3D::spineEdgeIndices();
    QCOMPARE(spine.size(), 3);
    const QVector<QPair<int, int>> edges = QChartAxes3D::boxEdges();

    // {u∥边0, v∥边4, w∥边8}
    QCOMPARE(spine.at(0), 0);
    QCOMPARE(spine.at(1), 4);
    QCOMPARE(spine.at(2), 8);

    const QVector<QVector3D> c = QChartAxes3D::boxCorners(QVector3D(0, 0, 0), QVector3D(1, 2, 3));
    for (int i = 0; i < 3; ++i) {
        const auto& e = edges.at(spine.at(i));
        // 均含角 0（min 角）
        QVERIFY2(e.first == 0 || e.second == 0, "spine 必须从 min 角出发");
        // 方向：spine[0] 沿 u、spine[1] 沿 v、spine[2] 沿 w
        const QVector3D a = c.at(e.first), b = c.at(e.second);
        if (i == 0) QVERIFY(a.x() != b.x() && a.y() == b.y() && a.z() == b.z());
        if (i == 1) QVERIFY(a.y() != b.y() && a.x() == b.x() && a.z() == b.z());
        if (i == 2) QVERIFY(a.z() != b.z() && a.x() == b.x() && a.y() == b.y());
    }
}

// ===== 4. ticks：复用 2D Axis（tickValues 委托）=====
void TestQChartAxes3D::ticks_reuseAxis() {
    QChartAxes3D axes;
    QValueAxis va;
    va.setRange(-10, 10);
    va.setTickCount(5);
    axes.axis(0).axis = &va;

    const QVector<qreal> fromAxes = axes.ticks(0, -10, 10);
    const QVector<qreal> fromAxis = va.tickValues(-10, 10);
    QCOMPARE(fromAxes, fromAxis);   // 复用回归
    QVERIFY(fromAxes.size() >= 2);

    // axis 为 null → 空
    QVERIFY(axes.ticks(1, -10, 10).isEmpty());
    // 越界 dim → 空
    QVERIFY(axes.ticks(3, -10, 10).isEmpty());
}

// ===== 5. tickAnchor：anchor.dim == tickValue、其余分量 == dataMin =====
void TestQChartAxes3D::tickAnchor_onSpine() {
    const QVector3D dataMin(1, 2, 3);
    QCOMPARE(QChartAxes3D::tickAnchor(0, 7.5, dataMin), QVector3D(7.5, 2, 3));
    QCOMPARE(QChartAxes3D::tickAnchor(1, -4.0, dataMin), QVector3D(1, -4.0, 3));
    QCOMPARE(QChartAxes3D::tickAnchor(2, 0.0, dataMin), QVector3D(1, 2, 0.0));
}

// ===== 6. markerSizePx：默认 4.0、可配、per-dim 独立 =====
void TestQChartAxes3D::markerSizePx_default() {
    QChartAxes3D axes;
    QCOMPARE(axes.axis(0).markerSizePx, 4.0);
    QCOMPARE(axes.axis(1).markerSizePx, 4.0);
    QCOMPARE(axes.axis(2).markerSizePx, 4.0);

    axes.axis(0).markerSizePx = 7.5;
    QCOMPARE(axes.axis(0).markerSizePx, 7.5);
    QCOMPARE(axes.axis(1).markerSizePx, 4.0);   // dim1 不受影响
    QCOMPARE(axes.axis(2).markerSizePx, 4.0);
}

// ===== 7. tickLabelTexts：复用 2D Axis（tickLabels 委托）=====
void TestQChartAxes3D::labels_reuseAxis() {
    QChartAxes3D axes;
    QValueAxis va;
    va.setRange(0, 100);
    va.setTickCount(6);
    axes.axis(2).axis = &va;

    const QStringList fromAxes = axes.tickLabelTexts(2, 0, 100);
    const QVector<qreal> ticks = va.tickValues(0, 100);
    QCOMPARE(fromAxes, va.tickLabels(ticks));
    QCOMPARE(fromAxes.size(), ticks.size());

    // axis 为 null → 空
    QVERIFY(axes.tickLabelTexts(0, 0, 100).isEmpty());
}

// ===== 8. axisConfig：每维 visible/markerSize/labelOffset 独立生效 =====
void TestQChartAxes3D::axisConfig_perDim() {
    QChartAxes3D axes;
    axes.axis(0).visible = false;
    axes.axis(0).markerSizePx = 9.0;
    axes.axis(0).labelOffsetPx = QPointF(3, -2);
    axes.axis(0).axisTitleVisible = false;
    axes.axis(0).axisTitle = "dim0 标题";

    // dim1/dim2 保持默认
    QVERIFY(axes.axis(1).visible);
    QCOMPARE(axes.axis(1).markerSizePx, 4.0);
    QCOMPARE(axes.axis(1).labelOffsetPx, QPointF(0, 0));
    QVERIFY(axes.axis(1).axisTitleVisible);
    QVERIFY(axes.axis(2).axisTitle.isEmpty());
    QVERIFY(axes.axis(2).visible);

    // 总开关
    QVERIFY(axes.visible());
    axes.setVisible(false);
    QVERIFY(!axes.visible());
    axes.setVisible(true);
    QVERIFY(axes.visible());
}
