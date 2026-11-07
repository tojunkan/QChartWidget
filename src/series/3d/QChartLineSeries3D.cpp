// QChartLineSeries3D.cpp —— 3D 折线系列实现（收集 Numeric 直线段，绘制统一由 renderer 完成）
#include "QChartLineSeries3D.h"
#include "QChartScene.h"
#include "QChartPrimitive.h"
#include "QChartAxis.h"
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

// ===== 图元收集 =====
// 连续的**有效**点组成一段折线，一律是 Path（折线只有一种表示，没有 Line 特例）。NaN/Inf 断段。
void QChartLineSeries3D::collectPrimitives(QChartSegment& seg,
                                           const QChartAxis* ax, const QChartAxis* ay,
                                           const QChartAxis* az) const {
    const int n = count();
    QVector<QVector3D> run;
    int runStart = 0;               // 本段折线的**首点数据下标**（契约 C3：一个图元跨多个数据项 → 写起点）
    auto flush = [&]() {
        if (run.isEmpty()) return;
        QChartPrimitive p;
        p.type = QChartPrimitive::Type::Path;
        p.numVerts = run;
        p.color = color();
        p.penWidth = static_cast<float>(m_lineWidth);
        seg.prims.append(p);
        run.clear();
    };

    for (int i = 0; i < n; ++i) {
        // count()/at()：双存储统一访问（numeric-only → float3 单点物化；混合 → QDataPoint3D 权威）
        const QDataPoint3D d = at(i);
        const qreal nx = ax ? ax->toNumeric(d.x()) : d.x().toDouble();
        const qreal ny = ay ? ay->toNumeric(d.y()) : d.y().toDouble();
        const qreal nz = az ? az->toNumeric(d.z()) : d.z().toDouble();
        if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) {
            flush();
            continue;
        }
        if (run.isEmpty()) runStart = i;   // 本段的首点
        run.append(QVector3D(static_cast<float>(nx), static_cast<float>(ny),
                             static_cast<float>(nz)));
    }
    flush();
}
