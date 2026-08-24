// test_qchartsurface3d.cpp —— 3D 系列单元测试
// 覆盖（design_3d.md §11.1 TestQChartSurface3D 全部 7 组）。
// 注：QVector3D/QPointF 经 float 存储 → 数值断言容差 1e-4。
#include <QtTest>
#include <QDateTime>
#include <cmath>
#include "QDataPoint3D.h"
#include "QChartSeries3D.h"
#include "QChartScatterSeries3D.h"
#include "QChartLineSeries3D.h"
#include "QChartSurfaceSeries.h"
#include "QChartCylindricalProjection3D.h"
#include "test_qchartsurface3d.h"

// ===== 1. QDataPoint3D：QVariant 三元组（qreal/QDateTime）存取一致 =====
void TestQChartSurface3D::data_variantTriple() {
    QChartScatterSeries3D s("pts");
    const QDateTime dt(QDate(2024, 1, 1), QTime(12, 0));

    s.append(QDataPoint3D(QVariant(1.0), QVariant(2.5), QVariant(3.0)));
    s.append(QDataPoint3D(QVariant(dt), QVariant(10), QVariant(20)));   // 任意 Axis 类型
    s.append(4.0, 5.0, 6.0);                                            // 便捷 qreal
    QCOMPARE(s.count(), 3);
    QCOMPARE(s.at(0).x(), QVariant(1.0));
    QCOMPARE(s.at(0).y(), QVariant(2.5));
    QCOMPARE(s.at(1).x(), QVariant(dt));
    QCOMPARE(s.at(2).z(), QVariant(6.0));

    // replace / insert / remove / clear / setPoints
    s.replace(1, QDataPoint3D(7.0, 8.0, 9.0));
    QCOMPARE(s.at(1).x(), QVariant(7.0));
    s.insert(0, QDataPoint3D(0, 0, 0));
    QCOMPARE(s.count(), 4);
    QCOMPARE(s.at(0).x(), QVariant(0));
    s.remove(0);
    QCOMPARE(s.count(), 3);
    s.clear();
    QCOMPARE(s.count(), 0);

    QVector<QDataPoint3D> pts;
    pts.append(QDataPoint3D(1, 2, 3));
    pts.append(QDataPoint3D(4, 5, 6));
    s.setPoints(pts);
    QCOMPARE(s.count(), 2);
    QCOMPARE(s.at(1).y(), QVariant(5));
    QCOMPARE(s.points().size(), 2);

    // dataChanged 信号
    int signalCount = 0;
    QObject::connect(&s, &QChartSeries3D::dataChanged, [&signalCount]() { ++signalCount; });
    s.append(1, 2, 3);
    QCOMPARE(signalCount, 1);
}

// ===== 2. setGrid：行主序存取 + 大小校验 =====
void TestQChartSurface3D::grid_layout() {
    QChartSurfaceSeries s;
    QVector<QDataPoint3D> pts;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            pts.append(QDataPoint3D(r * 10.0 + c, r, c));
    s.setGrid(3, 4, pts);

    QCOMPARE(s.rows(), 3);
    QCOMPARE(s.cols(), 4);
    QCOMPARE(s.count(), 12);

    // 行主序：gridAt(row, col) = pts[row*cols+col]
    QDataPoint3D g = s.gridAt(1, 2);
    QCOMPARE(g.x(), QVariant(12.0));
    QCOMPARE(g.y(), QVariant(1.0));
    QCOMPARE(g.z(), QVariant(2.0));
    QCOMPARE(s.gridAt(0, 0).x(), QVariant(0.0));
    QCOMPARE(s.gridAt(2, 3).x(), QVariant(23.0));

    // 大小校验：pts.size() != rows*cols → 拒绝（状态不变）
    QVector<QDataPoint3D> bad;
    bad.append(QDataPoint3D(0, 0, 0));
    s.setGrid(3, 4, bad);
    QCOMPARE(s.rows(), 3);
    QCOMPARE(s.cols(), 4);
    QCOMPARE(s.count(), 12);

    // 越界 gridAt → 空点
    QDataPoint3D oob = s.gridAt(9, 9);
    QVERIFY(!oob.x().isValid());
}

// ===== 3. setParametricGrid：(u0,u1)×(v0,v1) 采样值正确 =====
void TestQChartSurface3D::parametricGrid_lattice() {
    QChartSurfaceSeries s;
    s.setParametricGrid(3, 5, 0.0, 100.0, -10.0, 10.0);

    QCOMPARE(s.rows(), 3);
    QCOMPARE(s.cols(), 5);
    QCOMPARE(s.count(), 15);

    // u = u0 + (u1-u0)*c/(cols-1)；v = v0 + (v1-v0)*r/(rows-1)
    QDataPoint3D p00 = s.gridAt(0, 0);   // u=0, v=-10
    QCOMPARE(p00.x(), QVariant(0.0));
    QCOMPARE(p00.y(), QVariant(-10.0));
    QDataPoint3D p22 = s.gridAt(2, 2);   // u=0+100*2/4=50, v=-10+20*2/2=10
    QCOMPARE(p22.x(), QVariant(50.0));
    QCOMPARE(p22.y(), QVariant(10.0));
    QDataPoint3D p14 = s.gridAt(1, 4);   // u=100, v=-10+20*1/2=0
    QCOMPARE(p14.x(), QVariant(100.0));
    QCOMPARE(p14.y(), QVariant(0.0));

    // z 未用（空 QVariant）
    QVERIFY(!p22.z().isValid());
}

// ===== 4. worldCache：尺寸 = rows*cols、值 = toWorld(u,v)（模拟 Layer3D 渲染时填充）=====
void TestQChartSurface3D::worldCache_filled() {
    QChartSurfaceSeries s;
    s.setParametricGrid(2, 3, 0.0, 90.0, 0.0, 180.0);   // u∈[0,90]、v∈[0,180]

    // 模拟 QChartLayer3D（t11）渲染时填充：worldCache = 柱坐标 toWorld(u,v,0)
    QChartCylindricalProjection3D cyl;
    QVector<QVector3D>& cache = s.worldCache();
    cache.resize(s.rows() * s.cols());
    for (int r = 0; r < s.rows(); ++r) {
        for (int c = 0; c < s.cols(); ++c) {
            QDataPoint3D d = s.gridAt(r, c);
            cache[r * s.cols() + c] =
                cyl.toWorld(d.x().toDouble(), d.y().toDouble(), 0.0);
        }
    }

    // 尺寸 = rows*cols
    QCOMPARE(s.worldCache().size(), s.rows() * s.cols());
    QCOMPARE(s.worldCache().size(), 6);

    // 值 = toWorld(u,v)：gridAt(0,1): u=45, v=0 → (45·cos0, 45·sin0, 0)
    QVector3D v01 = s.worldCache().at(0 * 3 + 1);
    QVERIFY(qAbs(v01.x() - 45.0) < 1e-4 && qAbs(v01.y()) < 1e-4 && qAbs(v01.z()) < 1e-4);
    // gridAt(1,1): u=45, v=180 → (-45, 0, 0)
    QVector3D v11 = s.worldCache().at(1 * 3 + 1);
    QVERIFY(qAbs(v11.x() + 45.0) < 1e-4 && qAbs(v11.y()) < 1e-4 && qAbs(v11.z()) < 1e-4);
}

// ===== 5. QChartScatterSeries3D：markerSize 属性存在/生效 =====
void TestQChartSurface3D::scatter_markerSize() {
    QChartScatterSeries3D s;
    QVERIFY(s.metaObject()->indexOfProperty("markerSize") >= 0);
    QCOMPARE(s.markerSize(), 4.0);   // 默认

    int signalCount = 0;
    QObject::connect(&s, &QChartScatterSeries3D::markerSizeChanged, [&signalCount]() { ++signalCount; });
    s.setMarkerSize(7.5);
    QCOMPARE(s.markerSize(), 7.5);
    QCOMPARE(signalCount, 1);

    // 无效值忽略
    s.setMarkerSize(-1.0);
    QCOMPARE(s.markerSize(), 7.5);

    // 便捷数据 + 全链闭包（Data→{screen,depth}，恒等）→ Point 图元携带 markerSize/dataIndex/depth
    s.append(1, 2, 3);
    QVector<QChartPrimitive> out;
    ProjectFn3D fn = [](const QDataPoint3D& d) {
        return QChartProjectedPoint{ QPointF(d.x().toDouble(), d.y().toDouble()), d.z().toDouble() };
    };
    s.collectPrimitives(fn, out);
    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0].type, QChartPrimitive::Type::Point);
    QCOMPARE(out[0].markerSize, 7.5);
    QCOMPARE(out[0].dataIndex, 0);
    QCOMPARE(out[0].depth, 3.0);
}

// ===== 6. QChartLineSeries3D：任一端 NaN → 断段；深度=端点均值；dataIndex=起点 =====
void TestQChartSurface3D::line_breakOnNaN() {
    QChartLineSeries3D s;
    s.append(0, 0, 0);
    s.append(1, 0, 2);
    s.append(qQNaN(), 0, 0);   // NaN 点 → 相邻两段都断
    s.append(2, 0, 4);
    s.append(3, 0, 6);

    ProjectFn3D fn = [](const QDataPoint3D& d) {
        return QChartProjectedPoint{ QPointF(d.x().toDouble(), d.y().toDouble()), d.z().toDouble() };
    };
    QVector<QChartPrimitive> out;
    s.collectPrimitives(fn, out);

    // 段：(0-1) 有效；(1-NaN) 断；(NaN-2) 断；(2-3) 有效 → 2 条
    QCOMPARE(out.size(), 2);
    QVERIFY(out[0].type == QChartPrimitive::Type::LineSegment);
    QVERIFY(qAbs(out[0].a.x() - 0.0) < 1e-6 && qAbs(out[0].b.x() - 1.0) < 1e-6);
    QVERIFY(qAbs(out[1].a.x() - 2.0) < 1e-6 && qAbs(out[1].b.x() - 3.0) < 1e-6);
    // 裁决 a：深度 = 两端点 depth 均值（(0+2)/2=1；(4+6)/2=5）
    QVERIFY(qAbs(out[0].depth - 1.0) < 1e-6);
    QVERIFY(qAbs(out[1].depth - 5.0) < 1e-6);
    // 裁决 c：dataIndex = 线段起点数据索引
    QCOMPARE(out[0].dataIndex, 0);
    QCOMPARE(out[1].dataIndex, 3);

    // 无 NaN → n-1 条；默认属性
    QChartLineSeries3D s2;
    for (int i = 0; i < 4; ++i) s2.append(i, 0, 0);
    QVector<QChartPrimitive> out2;
    s2.collectPrimitives(fn, out2);
    QCOMPARE(out2.size(), 3);
    QCOMPARE(s2.lineWidth(), 2.0);
    QVERIFY(s2.isCullingEnabled());
    QVERIFY(s2.metaObject()->indexOfProperty("lineWidth") >= 0);
    QVERIFY(s2.metaObject()->indexOfProperty("cullingEnabled") >= 0);
}

// ===== 7. collect：投影 NaN 图元被跳过（散点 + 曲面线框）=====
void TestQChartSurface3D::collect_nanSkip() {
    // 散点：闭包对 x>=2 返回 NaN screen → 该点跳过
    QChartScatterSeries3D s;
    s.append(0, 0, 0);
    s.append(1, 1, 1);
    s.append(2, 2, 2);
    auto projectFn = [](const QDataPoint3D& d) -> QChartProjectedPoint {
        const qreal x = d.x().toDouble();
        if (x >= 2.0) return QChartProjectedPoint{ QPointF(qQNaN(), qQNaN()), 0.0 };
        return QChartProjectedPoint{ QPointF(x, d.y().toDouble()), d.z().toDouble() };
    };
    QVector<QChartPrimitive> out;
    s.collectPrimitives(projectFn, out);
    QCOMPARE(out.size(), 2);
    QVERIFY(out[0].type == QChartPrimitive::Type::Point);
    QCOMPARE(out[0].markerSize, 4.0);

    // 曲面 2×2：线框 2·1 + 2·1 = 4 条；含 NaN 点（x>=2）时触及段跳过
    QChartSurfaceSeries surf;
    QVector<QDataPoint3D> pts;
    pts.append(QDataPoint3D(0, 0, 0));
    pts.append(QDataPoint3D(1, 0, 0));
    pts.append(QDataPoint3D(0, 1, 0));
    pts.append(QDataPoint3D(1, 1, 0));
    surf.setGrid(2, 2, pts);
    QVector<QChartPrimitive> sout;
    surf.collectPrimitives(projectFn, sout);
    QCOMPARE(sout.size(), 4);   // 无 x>=2 → 全有效

    // 把 (1,1) 的投影打成 NaN → 触及它的段（v 方向 1 条 + u 方向 1 条）跳过 → 2 条
    auto projectFn2 = [](const QDataPoint3D& d) -> QChartProjectedPoint {
        const qreal x = d.x().toDouble(), y = d.y().toDouble();
        if (x >= 1.0 && y >= 1.0) return QChartProjectedPoint{ QPointF(qQNaN(), qQNaN()), 0.0 };
        return QChartProjectedPoint{ QPointF(x, y), d.z().toDouble() };
    };
    QVector<QChartPrimitive> sout2;
    surf.collectPrimitives(projectFn2, sout2);
    QCOMPARE(sout2.size(), 2);
}
