// QChartCartesianProjection3D.h —— 3D 笛卡尔坐标投影（恒等）
// toWorld/fromWorld: 恒等映射，Numeric 空间 ≡ World 空间
// 2D QCartesianProjection 的 3D 对应
#pragma once
#include "QChartProjection3D.h"

class QChartCartesianProjection3D : public QChartProjection3D {
public:
    QChartCartesianProjection3D() : QChartProjection3D("x", "y", "z") {}

    // ── Numeric ↔ World：恒等 ──
    QVector3D toWorld(qreal n0, qreal n1, qreal n2 = 0.0) const override {
        return QVector3D(n0, n1, n2);
    }

    QVector3D fromWorld(const QVector3D& w) const override {
        return w;
    }

    // ── 恒等快速通道（design_3d_axes.md §5.4）：直线两点即可、反算免采样 ──
    int samplingSegmentsHint() const override { return 2; }
    bool isIdentityMapping() const override { return true; }
};
