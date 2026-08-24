// QChartAxes3D.cpp —— 3D 轴参照系编排器实现
// 纯 Numeric 空间几何 + 2D Axis 刻度/标签委托；无 toWorld/投影/绘制（三层分离红线）。
#include "QChartAxes3D.h"
#include "QChartAxis.h"

QChartAxes3D::QChartAxes3D() = default;

// ===== 盒几何 =====
QVector<QVector3D> QChartAxes3D::boxCorners(const QVector3D& dataMin, const QVector3D& dataMax) {
    // index = u | (v<<1) | (w<<2)：bit0=u、bit1=v、bit2=w，置位取 dataMax 分量
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
    // u∥：bit0 翻转；v∥：bit1 翻转；w∥：bit2 翻转
    return {
        { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },   // u∥（4 条）
        { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },   // v∥（4 条）
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },   // w∥（4 条）
    };
}

QVector<int> QChartAxes3D::spineEdgeIndices() {
    return { 0, 4, 8 };   // min 角（角 0）出发：u∥边0、v∥边4、w∥边8
}

// ===== 刻度/标签委托（组合复用 2D Axis）=====
QVector<qreal> QChartAxes3D::ticks(int dim, qreal dimMin, qreal dimMax) const {
    if (dim < 0 || dim > 2) return {};
    const QChartAxis* a = m_cfg[dim].axis;
    if (!a) return {};
    return a->tickValues(dimMin, dimMax);
}

QStringList QChartAxes3D::tickLabelTexts(int dim, qreal dimMin, qreal dimMax) const {
    if (dim < 0 || dim > 2) return {};
    const QChartAxis* a = m_cfg[dim].axis;
    if (!a) return {};
    return a->tickLabels(a->tickValues(dimMin, dimMax));
}

// ===== 刻度锚点（Numeric）：dataMin 的 dim 分量替换为 tickValue =====
QVector3D QChartAxes3D::tickAnchor(int dim, qreal tickValue, const QVector3D& dataMin) {
    QVector3D anchor = dataMin;
    if (dim == 0) anchor.setX(tickValue);
    else if (dim == 1) anchor.setY(tickValue);
    else if (dim == 2) anchor.setZ(tickValue);
    return anchor;
}
