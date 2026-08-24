// QCartesianProjection.h —— 笛卡尔坐标投影
// toCartesian/fromCartesian: 恒等映射，Numeric 空间 ≡ View Cartesian 空间
// computeDataBounds/computeViewRect: 恒等，dataBounds ≡ viewRect
#pragma once
#include "QChartProjection.h"

class QCartesianProjection : public QChartProjection {
public:

	QCartesianProjection() : QChartProjection("x", "y") {}

    CoordinateSystem type() const override { return CoordinateSystem::Cartesian; }

    // ── Numeric ↔ View Cartesian：恒等 ──
    QPointF toCartesian(qreal num0, qreal num1) const override {
        return QPointF(num0, num1);
    }

    QPointF fromCartesian(qreal x, qreal y) const override {
        return QPointF(x, y);
    }

    // ── 包络：恒等（Cartesian 下两种空间同构）──
    QRectF computeDataBounds(const QRectF& viewRect) const override {
        return viewRect;   // Numeric 范围 == View Cartesian 范围
    }

    QRectF computeViewRect(const QRectF& dataBounds) const override {
        return dataBounds; // 反过来也恒等
    }

    QRectF defaultDataBounds() const override {
        return QRectF(0, 0, 10, 10); // 初始默认范围
    }
};
