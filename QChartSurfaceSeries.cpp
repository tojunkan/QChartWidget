// QChartSurfaceSeries.cpp —— 曲面线框系列实现
#include "QChartSurfaceSeries.h"
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
    m_points = pts;
    m_worldCache.clear();          // 网格变更 → 旧 World 缓存失效（Layer3D 下次渲染重建）
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

// ===== 图元收集：线框（走全链闭包 ProjectFn3D，裁决 b）=====
void QChartSurfaceSeries::collectPrimitives(const ProjectFn3D& projectFn,
                                            QVector<QChartPrimitive>& out) const {
    if (!projectFn || m_rows <= 0 || m_cols <= 0) return;

    // 全链闭包预投影全部网格点（Data→{screen,depth}）
    const int n = m_points.size();
    QVector<QChartProjectedPoint> proj(n);
    QVector<bool> valid(n, false);
    for (int i = 0; i < n; ++i) {
        proj[i] = projectFn(m_points.at(i));
        valid[i] = std::isfinite(proj[i].screen.x()) && std::isfinite(proj[i].screen.y());
    }

    auto emitSegment = [&](int i0, int i1) {
        if (!valid[i0] || !valid[i1]) return;   // 任一端投影 NaN → 跳过
        QChartPrimitive prim;
        prim.type = QChartPrimitive::Type::LineSegment;
        prim.a = proj[i0].screen;
        prim.b = proj[i1].screen;
        prim.depth = (proj[i0].depth + proj[i1].depth) * 0.5;   // 裁决 a：两端点深度均值
        prim.dataIndex = i0;                                     // 裁决 c：线段起点数据索引
        prim.penWidth = 1.0;
        prim.color = color();
        out.append(prim);
    };

    // u 方向：rows·(cols-1) 条（同一行内相邻列）
    for (int r = 0; r < m_rows; ++r)
        for (int c = 0; c + 1 < m_cols; ++c)
            emitSegment(r * m_cols + c, r * m_cols + c + 1);
    // v 方向：cols·(rows-1) 条（同一列内相邻行）
    for (int c = 0; c < m_cols; ++c)
        for (int r = 0; r + 1 < m_rows; ++r)
            emitSegment(r * m_cols + c, (r + 1) * m_cols + c);
}

// ===== 直接绘制：无排序直绘 =====
void QChartSurfaceSeries::draw(QPainter* painter,
                               const ProjectFn3D& projectFn,
                               const DrawContext3D* ctx) const {
    Q_UNUSED(ctx);
    if (!painter || !projectFn) return;
    QVector<QChartPrimitive> items;
    collectPrimitives(projectFn, items);
    painter->save();
    painter->setOpacity(opacity());
    painter->setPen(QPen(color(), 1.0));
    for (const QChartPrimitive& prim : items)
        painter->drawLine(prim.a, prim.b);
    painter->restore();
}
