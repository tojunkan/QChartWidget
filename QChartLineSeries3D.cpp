// QChartLineSeries3D.cpp —— 3D 折线系列实现
#include "QChartLineSeries3D.h"
#include <cmath>

QChartLineSeries3D::QChartLineSeries3D(const QString& name, QObject* parent)
    : QChartSeries3D(name, parent) {}

void QChartLineSeries3D::setLineWidth(qreal w) {
    if (w <= 0.0 || m_lineWidth == w) return;
    m_lineWidth = w;
    emit lineWidthChanged();
}

void QChartLineSeries3D::setCullingEnabled(bool v) {
    if (m_cullingEnabled == v) return;
    m_cullingEnabled = v;
    emit cullingChanged();
}

void QChartLineSeries3D::collectPrimitives(const ProjectFn3D& projectFn,
                                           QVector<QChartPrimitive>& out) const {
    if (!projectFn || m_points.size() < 2) return;

    // 全链闭包预投影所有点（{screen,depth}），screen 非有限 → 断段
    struct Proj { QChartProjectedPoint p; bool valid; };
    QVector<Proj> proj;
    proj.reserve(m_points.size());
    for (const QDataPoint3D& d : m_points) {
        const QChartProjectedPoint p = projectFn(d);
        proj.append({ p, std::isfinite(p.screen.x()) && std::isfinite(p.screen.y()) });
    }

    for (int i = 0; i + 1 < proj.size(); ++i) {
        const Proj& p0 = proj.at(i);
        const Proj& p1 = proj.at(i + 1);
        if (!p0.valid || !p1.valid) continue;   // 任一端 NaN → 断段

        QChartPrimitive prim;
        prim.type = QChartPrimitive::Type::LineSegment;
        prim.a = p0.p.screen;
        prim.b = p1.p.screen;
        prim.depth = (p0.p.depth + p1.p.depth) * 0.5;   // 裁决 a：两端点深度均值
        prim.dataIndex = i;                              // 裁决 c：线段起点数据索引
        prim.penWidth = m_lineWidth;
        prim.color = color();
        out.append(prim);
    }
}

void QChartLineSeries3D::draw(QPainter* painter,
                              const ProjectFn3D& projectFn,
                              const DrawContext3D* ctx) const {
    Q_UNUSED(ctx);
    if (!painter || !projectFn) return;
    QVector<QChartPrimitive> items;
    collectPrimitives(projectFn, items);
    painter->save();
    painter->setOpacity(opacity());
    painter->setPen(QPen(color(), m_lineWidth));
    for (const QChartPrimitive& prim : items)
        painter->drawLine(prim.a, prim.b);   // 无排序直绘
    painter->restore();
}
