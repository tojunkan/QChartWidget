// QChartScatterSeries3D.cpp —— 3D 散点系列实现
#include "QChartScatterSeries3D.h"
#include "QChartScene.h"
#include "QChartPrimitive.h"
#include "QChartAxis.h"
#include <cmath>

QChartScatterSeries3D::QChartScatterSeries3D(const QString& name, QObject* parent)
    : QChartSeries3D(name, parent) {}

void QChartScatterSeries3D::setMarkerSize(qreal s) {
    if (s <= 0.0 || m_markerSize == s) return;
    m_markerSize = s;
    emit markerSizeChanged();
}

// ===== 图元收集：每点一个 Point 图元（Numeric 三元组），非法点跳过 =====
// count()/at()：双存储统一访问（numeric-only → float3 单点物化；混合 → QDataPoint3D 权威）
void QChartScatterSeries3D::collectPrimitives(QChartSegment& seg,
                                              const QChartAxis* ax, const QChartAxis* ay,
                                              const QChartAxis* az) const {
    const int n = count();
    for (int i = 0; i < n; ++i) {
        const QDataPoint3D d = at(i);
        const qreal nx = ax ? ax->toNumeric(d.x()) : d.x().toDouble();
        const qreal ny = ay ? ay->toNumeric(d.y()) : d.y().toDouble();
        const qreal nz = az ? az->toNumeric(d.z()) : d.z().toDouble();
        if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) continue;

        QChartPrimitive p;
        p.type = QChartPrimitive::Type::Point;
        p.numA = QVector3D(static_cast<float>(nx), static_cast<float>(ny),
                           static_cast<float>(nz));
        // 契约 C3：一个点一个图元 → 数据下标就是它在系列里的下标
        p.color = color();
        p.markerSize = static_cast<float>(m_markerSize);
        seg.prims.append(p);
    }
}
