// QChartAxes3D.cpp
#include "QChartAxes3D.h"
#include "QChartAxis.h"   // 4b：组装数据盒需读取轴范围

// ===== 4b：数据盒按需从三根轴范围组装（不长期持有）=====
QCube QChartAxes3D::dataBounds() const
{
    QVector3D mn(0, 0, 0), mx(10, 10, 10);   // 默认（axis 缺失维度的回退，与迁移前默认盒一致）
    for (int d = 0; d < 3; ++d) {
        const QChartAxis* a = m_cfg[d].axis;
        if (!a) continue;
        const qreal lo = a->min(), hi = a->max();
        switch (d) {
        case 0: mn.setX(lo); mx.setX(hi); break;
        case 1: mn.setY(lo); mx.setY(hi); break;
        default: mn.setZ(lo); mx.setZ(hi); break;
        }
    }
    return QCube(mn, mx);
}

QVector<QVector3D> QChartAxes3D::boxCorners(const QVector3D& dataMin, const QVector3D& dataMax) {
    QVector<QVector3D> corners;
    corners.reserve(8);
    for (int i = 0; i < 8; ++i) {
        const qreal u = (i & 1) ? dataMax.x() : dataMin.x();
        const qreal v = (i & 2) ? dataMax.y() : dataMin.y();
        const qreal w = (i & 4) ? dataMax.z() : dataMin.z();
        corners.append(QVector3D(u, v, w));
    }
    return corners;
}

QVector<QPair<int, int>> QChartAxes3D::boxEdges() {
    return {
        {0,1}, {2,3}, {4,5}, {6,7},   // u∥
        {0,2}, {1,3}, {4,6}, {5,7},   // v∥
        {0,4}, {1,5}, {2,6}, {3,7}    // w∥
    };
}

QVector<int> QChartAxes3D::spineEdgeIndices() {
    return {0, 4, 8};   // 从角0出发的三条边
}

// ===== FaceLine 模式几何（批次2 B）=====
qreal QChartAxes3D::safeFixedValue(qreal lo, qreal hi) {
    if (lo > hi) { const qreal t = lo; lo = hi; hi = t; }
    if (lo <= 0.0 && 0.0 <= hi) return 0.0;   // 0 在范围内 → 取 0（球坐标 θ/φ 的安全位置）
    return 0.5 * (lo + hi);                   // 否则取中点（仍远离极点/奇点）
}

QPair<QVector3D, QVector3D> QChartAxes3D::faceLineSegment(const QVector3D& dataMin,
                                                          const QVector3D& dataMax) {
    const qreal fixed1 = safeFixedValue(dataMin.y(), dataMax.y());
    const qreal fixed2 = safeFixedValue(dataMin.z(), dataMax.z());
    return { QVector3D(dataMin.x(), fixed1, fixed2),
             QVector3D(dataMax.x(), fixed1, fixed2) };
}