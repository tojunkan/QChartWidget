// QChartSurfaceSeries.cpp —— 3D 曲面实现（收集一个 TriangleMesh，绘制统一由 renderer 完成）
#include "QChartSurfaceSeries.h"
#include "QChartScene.h"
#include "QChartPrimitive.h"
#include "QChartAxis.h"
#include <QDebug>
#include <cmath>

QChartSurfaceSeries::QChartSurfaceSeries(const QString& name, QObject* parent)
    : QChartSeries3D(name, parent) {}

// ===== Data 层：网格 =====
void QChartSurfaceSeries::setGrid(int rows, int cols, const QVector<QDataPoint3D>& pts) {
    if (rows < 0 || cols < 0 || pts.size() != rows * cols) {
        qWarning() << "QChartSurfaceSeries::setGrid: pts.size() 必须等于 rows*cols，忽略"
                   << "rows=" << rows << "cols=" << cols << "pts.size=" << pts.size();
        return;
    }
    m_rows = rows;
    m_cols = cols;
    setPointsInternal(pts);    // QVariant 路径：m_points 权威、numericCache 失效、worldCache 失效（不发信号）
    emit gridChanged();
    emit dataChanged();
}

QDataPoint3D QChartSurfaceSeries::gridAt(int row, int col) const {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return QDataPoint3D();
    return m_points.at(row * m_cols + col);
}

void QChartSurfaceSeries::setParametricGrid(int rows, int cols,
                                            qreal u0, qreal u1, qreal v0, qreal v1) {
    QVector<QDataPoint3D> pts;
    pts.reserve(rows * cols);
    for (int r = 0; r < rows; ++r) {
        const qreal v = (rows > 1) ? v0 + (v1 - v0) * r / (rows - 1) : v0;
        for (int c = 0; c < cols; ++c) {
            const qreal u = (cols > 1) ? u0 + (u1 - u0) * c / (cols - 1) : u0;
            pts.append(QDataPoint3D(QVariant(u), QVariant(v), QVariant()));   // z 未用
        }
    }
    setGrid(rows, cols, pts);
}

// ===== 图元收集：一个 TriangleMesh（Numeric 顶点 + 三角索引）=====
// 每格 2 个三角形：左上→左下→右上、右上→左下→右下（逆时针，与顶点顺序约定一致）；
// 四角任一非有限（NaN/Inf）→ 整个格跳过。
void QChartSurfaceSeries::collectPrimitives(QChartScene& scene,
                                            const QChartAxis* ax, const QChartAxis* ay,
                                            const QChartAxis* az) const {
    if (m_rows < 2 || m_cols < 2) return;

    const int n = m_rows * m_cols;
    QVector<QVector3D> verts;
    QVector<bool> valid;
    verts.reserve(n);
    valid.reserve(n);

    for (int i = 0; i < n; ++i) {
        // count()/at()：双存储统一访问（setGrid 走 QVariant 路径）
        const QDataPoint3D d = at(i);
        const qreal nx = ax ? ax->toNumeric(d.x()) : d.x().toDouble();
        const qreal ny = ay ? ay->toNumeric(d.y()) : d.y().toDouble();
        const qreal nz = az ? az->toNumeric(d.z()) : d.z().toDouble();
        const bool ok = std::isfinite(nx) && std::isfinite(ny) && std::isfinite(nz);
        verts.append(QVector3D(static_cast<float>(nx), static_cast<float>(ny),
                               static_cast<float>(nz)));
        valid.append(ok);
    }

    QVector<int> indices;
    indices.reserve(6 * (m_rows - 1) * (m_cols - 1));
    for (int r = 0; r + 1 < m_rows; ++r) {
        for (int c = 0; c + 1 < m_cols; ++c) {
            const int i00 = r * m_cols + c;          // 左上
            const int i01 = i00 + 1;                 // 右上
            const int i10 = i00 + m_cols;            // 左下
            const int i11 = i10 + 1;                 // 右下
            if (!valid.at(i00) || !valid.at(i01) || !valid.at(i10) || !valid.at(i11))
                continue;
            indices << i00 << i10 << i01;
            indices << i01 << i10 << i11;
        }
    }
    if (indices.isEmpty()) return;

    QChartPrimitive p;
    p.type = QChartPrimitive::Type::TriangleMesh;
    p.numVerts = verts;
    p.numIndices = indices;
    p.fillColor = color();
    p.color = color();
    scene.primitives.append(p);
}
