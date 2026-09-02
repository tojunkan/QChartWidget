// QChartProjection.h —— 2D 坐标投影基类
// 继承自 QChartAbstractProjection，扩展 2D 独有的包络计算（QRectF）
// Numeric 空间维度：2 (x, y)
#pragma once

#include "QChartAbstractProjection.h"
#include <QPointF>
#include <QRectF>

class QChartProjection : public QChartAbstractProjection {
public:
    // 构造：2D 只有两个维度名称
    QChartProjection(QString name0 = "x", QString name1 = "y")
        : QChartAbstractProjection({name0, name1}) {}

    virtual ~QChartProjection() = default;

    // ===== 身份标识（2D 特有） =====
    virtual CoordinateSystem type() const = 0;

    // ===== 2D 核心映射（保留原有纯虚接口，供子类实现） =====
    /// 双精度标量版本（子类必须实现）
    virtual QPointF toCartesian(qreal num0, qreal num1) const = 0;
    virtual QPointF fromCartesian(qreal x, qreal y) const = 0;

    // ===== 2D 包络转换（Pan/Zoom 用） =====
    virtual QRectF computeDataBounds(const QRectF& viewRect) const = 0;
    virtual QRectF computeViewRect(const QRectF& dataBounds) const = 0;
    virtual QRectF defaultDataBounds() const { return QRectF(0, 0, 10, 10); }

    // ===== 实现统一基类接口（final 以避免子类重载干扰） =====
    QVector3D toCartesian(const QVector3D& num) const final {
        QPointF p = toCartesian(num.x(), num.y());
        return QVector3D(p.x(), p.y(), 0.0f);
    }

    QVector3D fromCartesian(const QVector3D& cart) const final {
        QPointF p = fromCartesian(cart.x(), cart.y());
        return QVector3D(p.x(), p.y(), 0.0f);
    }

    int dimension() const final { return 2; }
};