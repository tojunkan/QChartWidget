// QChartProjection.h —— 坐标投影基类
// 职责：Numeric 空间 ↔ View Cartesian 空间的双向映射 + 视窗包络计算
// 五空间链路: Numeric ─[toCartesian]──► View Cartesian ─[cartesianToPixel]──► Pixel
#pragma once
#include "QChartDebug.h"
#include <QPointF>
#include <QRectF>
#include <QPainterPath>
#include <functional>
#include <cmath>
#include <QLoggingCategory>

//Q_LOGGING_CATEGORY(logProjection, "chart.projection")

// 坐标系类型枚举
enum class CoordinateSystem {
    Cartesian,
    Polar,
    Functional,
};

class QChartProjection {
public:
    virtual ~QChartProjection() = default;

    // ===== 身份标识 =====
    virtual CoordinateSystem type() const = 0;

    // ===== Numeric ↔ View Cartesian（纯几何，无需 dataBounds）=====

    /// <summary>
    /// 正向：两个维度的 Numeric 值 → View Cartesian 坐标
    /// NaN/Inf 自然传播，调用方负责跳过
    /// </summary>
    virtual QPointF toCartesian(qreal num0, qreal num1) const = 0;

    /// <summary>
    /// 反向：View Cartesian → 两个维度的 Numeric 值
    /// 奇点（如 Polar 极点 r=0）处 dim0 返回 NaN
    /// </summary>
    virtual QPointF fromCartesian(qreal x, qreal y) const = 0;

    // ===== dataBounds ↔ viewRect 包络转换 =====

    /// <summary>
    /// viewRect（View Cartesian 矩形）→ dataBounds（Numeric 空间覆盖范围）
    /// Pan/Zoom 后调用，计算可见数据范围用于刻度生成
    /// </summary>
    virtual QRectF computeDataBounds(const QRectF& viewRect) const = 0;

    /// <summary>
    /// dataBounds（Numeric 空间范围）→ viewRect（View Cartesian 内接矩形）
    /// setRange 语法糖后调用
    /// </summary>
    virtual QRectF computeViewRect(const QRectF& dataBounds) const = 0;

    // ===== 初始值 =====
    /// <summary>
    /// 默认 Numeric 范围。Widget 首次构造时用于确定初始 viewRect
    /// </summary>
    virtual QRectF defaultDataBounds() const { return QRectF(0, 0, 10, 10); }

    // ===== 路径生成 =====
    /// <summary>
    /// 采样 dataCurve(t) → (num0, num1)，经 toCartesian 映射后连接为 QPainterPath
    /// 遇到 NaN 自动断开（moveTo 重开新子路径），处理极坐标奇点
    /// </summary>
    /// <param name="dataCurve">t∈[0,1] → Numeric (num0, num1)，由调用方（Axis）定义曲线形状</param>
    /// <param name="segments">采样段数，默认 64</param>
    QPainterPath createPath(std::function<QPointF(qreal t)> dataCurve,
                            int segments = 64) const {
        QPainterPath path;
        if (!dataCurve) return path;

        bool firstValid = true;
        for (int i = 0; i <= segments; ++i) {
            qreal t = static_cast<qreal>(i) / segments;
            QPointF numeric = dataCurve(t);                         // Axis 定义"在哪画"
            QPointF cartesian = toCartesian(numeric.x(), numeric.y()); // Projection 定义"怎么映射"

            // NaN/Inf → 断开路径，重开新子路径
            if (!std::isfinite(cartesian.x()) || !std::isfinite(cartesian.y())) {
                firstValid = true;
                continue;
            }

            if (i == 0 || i == segments || i % 10 == 0) {
                qCDebug(logRender) << "createPath: i=" << i
                    << "numeric=" << numeric
                    << "cartesian=" << cartesian
                    << "isfinite=" << std::isfinite(cartesian.x());
            }

            if (firstValid) {
                path.moveTo(cartesian);
                firstValid = false;
            } else {
                path.lineTo(cartesian);
            }
        }
        return path;
        // 注意：返回的 path 坐标在 View Cartesian 空间，调用方还需 cartesianToPixel 变换后绘制
    }
};
