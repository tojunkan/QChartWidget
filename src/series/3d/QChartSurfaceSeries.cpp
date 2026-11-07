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
void QChartSurfaceSeries::collectPrimitives(QChartSegment& seg,
                                            const QChartAxis* ax, const QChartAxis* ay,
                                            const QChartAxis* az) const {
    // ★ 吐 Grid（不是 TriangleMesh）：顶点表是 rows×cols 的**共享**控制点（1681 个，替代旧版
    //   9600 个展开顶点），索引由渲染端按"每格 6 个"派生 —— 这样曲面控制点数与格点数一致，
    //   才能喂 tessellation 的 4 控制点/格 patch。
    if (m_rows < 2 || m_cols < 2) return;

    const int n = m_rows * m_cols;
    QVector<QVector3D> verts;
    verts.reserve(n);

    for (int i = 0; i < n; ++i) {
        // count()/at()：双存储统一访问（setGrid 走 QVariant 路径）
        const QDataPoint3D d = at(i);
        const qreal nx = ax ? ax->toNumeric(d.x()) : d.x().toDouble();
        const qreal ny = ay ? ay->toNumeric(d.y()) : d.y().toDouble();
        const qreal nz = az ? az->toNumeric(d.z()) : d.z().toDouble();
        verts.append(QVector3D(static_cast<float>(nx), static_cast<float>(ny),
                               static_cast<float>(nz)));
    }

    // ★ 第 2 步（Numeric → 图元）走共用入口 generatePrimitive：surface 与坐标系面共用同一份
    //   "Grid 图元怎么造"；网格连线也走共用的 generateWireframe（契约 C3 的注释见 QChartPrimitive.h）。
    const QChartPrimitive grid =
        generatePrimitive(QChartPrimitive::Type::Grid, verts, color(), m_rows, m_cols);

    if (m_renderMode != RenderMode::GridOnly) seg.prims.append(grid);

    if (m_renderMode != RenderMode::Surface) {
        // 线只是辅助：①颜色要**比面深一档**，否则同色叠在同色面上等于看不见；
        // ②半透明才能落进 3D 的"半透明趟"，从而画在面之后、压在面上。
        QColor lineColor = color().darker(170);
        lineColor.setAlpha(200);
        for (const QChartPrimitive& p : generateWireframe(grid, lineColor))
            seg.prims.append(p);
    }
}
