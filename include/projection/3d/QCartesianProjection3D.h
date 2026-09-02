// QCartesianProjection3D.h —— 3D 笛卡尔坐标投影（恒等）
// toWorld/fromWorld: 恒等映射，Numeric 空间 ≡ World 空间
// 2D QCartesianProjection 的 3D 对应
#pragma once
#include "QChartProjection3D.h"

class QCartesianProjection3D : public QChartProjection3D {
public:
    QCartesianProjection3D() : QChartProjection3D("x", "y", "z") {}

    // ── Numeric ↔ World：恒等 ──
    QVector3D toCartesian(qreal n0, qreal n1, qreal n2 = 0.0) const override {
        return QVector3D(n0, n1, n2);
    }

    QVector3D fromCartesian(const QVector3D& cart) const override {
        return cart;
    }

    // QCartesianProjection3D.h 中添加
    QString glslToCartesian() const override {
        return "num";
    }
    QString glslFromCartesian() const override {
        return "cart";
    }

    // ── 恒等快速通道（design_3d_axes.md §5.4）：直线两点即可、反算免采样 ──
    int samplingSegmentsHint() const override { return 2; }
    bool isIdentityMapping() const override { return true; }
};
