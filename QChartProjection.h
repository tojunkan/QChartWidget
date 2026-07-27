// QChartProjection.h
#pragma once
#include <QPointF>
#include <QRectF>

// 坐标系类型枚举（定义在 Projection 命名空间下）
enum class CoordinateSystem {
    Cartesian,
    Polar,
    // 未来可扩展：Ternary, Smith, ...
};

class QChartProjection {
public:
    virtual ~QChartProjection() = default;

    // 1. 身份标识：这是什么坐标系？
    virtual CoordinateSystem type() const = 0;

    // 2. 正向映射：归一化 (0~1) -> 像素
    virtual QPointF mapToPixel(qreal normX, qreal normY, const QRectF& plotArea) const = 0;

    // 3. 反向映射：像素 -> 归一化 (用于鼠标事件)
    virtual QPointF mapToNormalized(const QPointF& pixel, const QRectF& plotArea) const = 0;
};