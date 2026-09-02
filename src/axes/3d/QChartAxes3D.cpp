// QChartAxes3D.cpp
#include "QChartAxes3D.h"

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