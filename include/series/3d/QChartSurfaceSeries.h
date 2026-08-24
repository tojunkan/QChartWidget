// QChartSurfaceSeries.h —— 曲面线框系列（行主序网格）
// Data 层：网格 QVector<QDataPoint3D>（rows×cols 行主序）；setParametricGrid 便捷生成 (u,v) 格点。
// World 层缓存：m_worldCache（基类 QChartSeries3D，Phase 3 上移）行主序 rows*cols，由 QChartLayer3D
//   渲染时经自身 axis toNumeric + projection3D toWorld 直算填充（不走系列闭包；参数曲面莫比乌斯/球面的
//   toWorld 在此发生），供 Phase 3 VBO 直接消费；collectPrimitives 仍走全链闭包 ProjectFn3D（队长裁决 b）。
// collectPrimitives：线框 rows·(cols-1) + cols·(rows-1) 条 LineSegment，任一端投影 screen 非有限 → 跳过；
//   深度 = 两端点 depth 均值（裁决 a）；dataIndex = 线段起点数据索引（裁决 c）。
#pragma once
#include "QChartSeries3D.h"

class QChartSurfaceSeries : public QChartSeries3D {
    Q_OBJECT
public:
    explicit QChartSurfaceSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== Data 层：网格（行主序，rows×cols）=====
    /// pts.size() 必须 == rows*cols，否则忽略并 qWarning（不改变状态）
    void setGrid(int rows, int cols, const QVector<QDataPoint3D>& pts);
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    QDataPoint3D gridAt(int row, int col) const;
    /// 便捷：按参数域生成 (u,v) 格点（x=u, y=v, z 未用），u∈[u0,u1]、v∈[v0,v1]
    void setParametricGrid(int rows, int cols,
                           qreal u0, qreal u1, qreal v0, qreal v1);

    // ===== World 层缓存：基类 QChartSeries3D::worldCache（t51 上移，VBO 源）=====

    void collectPrimitives(const ProjectFn3D& projectFn,
                           QVector<QChartPrimitive>& out) const override;
    void draw(QPainter* painter, const ProjectFn3D& projectFn,
              const DrawContext3D* ctx = nullptr) const override;

signals:
    void gridChanged();

private:
    int m_rows = 0, m_cols = 0;
};
