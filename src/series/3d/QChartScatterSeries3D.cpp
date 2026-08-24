// QChartScatterSeries3D.cpp —— 3D 散点系列实现
#include "QChartScatterSeries3D.h"
#include <cmath>

QChartScatterSeries3D::QChartScatterSeries3D(const QString& name, QObject* parent)
    : QChartSeries3D(name, parent) {}

void QChartScatterSeries3D::setMarkerSize(qreal s) {
    if (s <= 0.0 || m_markerSize == s) return;
    m_markerSize = s;
    emit markerSizeChanged();
}

void QChartScatterSeries3D::collectPrimitives(const ProjectFn3D& projectFn,
                                              QVector<QChartPrimitive>& out) const {
    if (!projectFn) return;
    // count()/at()：双存储统一访问（numeric-only → float3 单点物化；混合 → QDataPoint3D 权威）
    const int n = count();
    for (int i = 0; i < n; ++i) {
        const QChartProjectedPoint p = projectFn(at(i));           // 全链闭包：Data→{screen,depth}
        if (!std::isfinite(p.screen.x()) || !std::isfinite(p.screen.y()))
            continue;   // 投影 NaN（w<=0 等）→ 跳过

        QChartPrimitive prim;
        prim.type = QChartPrimitive::Type::Point;
        prim.a = p.screen;
        prim.depth = p.depth;
        prim.dataIndex = i;
        prim.markerSize = m_markerSize;
        prim.color = color();
        prim.worldA = p.world;   // GL 顶点源（t42，§3.2）
        out.append(prim);
    }
}

void QChartScatterSeries3D::draw(QPainter* painter,
                                 const ProjectFn3D& projectFn,
                                 const DrawContext3D* ctx) const {
    Q_UNUSED(ctx);
    if (!painter || !projectFn) return;
    QVector<QChartPrimitive> items;
    collectPrimitives(projectFn, items);
    painter->save();
    painter->setOpacity(opacity());
    painter->setPen(Qt::NoPen);
    painter->setBrush(color());
    const qreal r = m_markerSize * 0.5;
    for (const QChartPrimitive& prim : items)
        painter->drawEllipse(prim.a, r, r);   // 无排序直绘
    painter->restore();
}
