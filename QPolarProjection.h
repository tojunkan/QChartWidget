// QPolarProjection.h —— 极坐标投影
// dim0 = 角度（度），dim1 = 半径（物理单位）
// toCartesian(θ°, r):  x = r·cos(θ·π/180),  y = r·sin(θ·π/180)
// fromCartesian(x, y): θ = atan2(y,x) ∈ [0°,360°),  r = √(x²+y²)
//                         极点(r=0)处 θ 无定义 → 返回 (NaN, 0)
// computeDataBounds: 沿 viewRect 四边采样 → min/max θ 和 min/max r
// computeViewRect: 扇形 Cartesian 轴对齐包围盒
#pragma once
#include "QChartProjection.h"
#include "QChartDebug.h"
#include <QtMath>
#include <algorithm>

class QPolarProjection : public QChartProjection {
public:

	QPolarProjection() : QChartProjection("θ", "r") {}

    CoordinateSystem type() const override { return CoordinateSystem::Polar; }

    // ── Numeric ↔ View Cartesian ──
    QPointF toCartesian(qreal num0, qreal num1) const override {
        // num0 = 角度（度）, num1 = 半径
        qreal rad = qDegreesToRadians(num0);
        return QPointF(
            num1 * qCos(rad),
            num1 * qSin(rad)
        );
    }

    QPointF fromCartesian(qreal x, qreal y) const override {
        qreal r = qSqrt(x * x + y * y);
        if (qFuzzyIsNull(r)) {
            // 极点：半径为零，角度无定义 → dim0 返回 NaN
            qCDebug(logProjection) << "fromCartesian: at pole (r=0), returning (NaN, 0)";
            return QPointF(qQNaN(), 0.0);
        }
        qreal rad = qAtan2(y, x);          // (-π, π]
        if (rad < 0.0) rad += 2.0 * M_PI;  // [0, 2π)
        qreal deg = qRadiansToDegrees(rad); // [0°, 360°)
        return QPointF(deg, r);
    }

    // ── 包络转换 ──

// QPolarProjection.h —— computeDataBounds 完整修复版

    QRectF computeDataBounds(const QRectF& viewRect) const override {
        // ── 1. 检测原点是否在视口内（精确 rMin） ──
        bool containsOrigin = viewRect.contains(QPointF(0, 0));

        // ── 2. 32×32 网格采样（求 θ 范围 和 rMax） ──
        const int grid = 32;
        qreal thetaMin = 360.0, thetaMax = 0.0;
        qreal rMin = containsOrigin ? 0.0 : qInf();
        qreal rMax = 0.0;

        auto processPoint = [&](qreal x, qreal y) {
            QPointF polar = fromCartesian(x, y);
            qreal theta = polar.x();
            qreal r = polar.y();
            if (std::isfinite(theta) && std::isfinite(r)) {
                thetaMin = qMin(thetaMin, theta);
                thetaMax = qMax(thetaMax, theta);
                rMax = qMax(rMax, r);
                if (!containsOrigin) {
                    rMin = qMin(rMin, r);
                }
            }
            };

        for (int i = 0; i <= grid; ++i) {
            qreal x = viewRect.left() + (static_cast<qreal>(i) / grid) * viewRect.width();
            for (int j = 0; j <= grid; ++j) {
                qreal y = viewRect.top() + (static_cast<qreal>(j) / grid) * viewRect.height();
                processPoint(x, y);
            }
        }

        // ── 3. 兜底 ──
        if (!std::isfinite(rMin)) rMin = 0.0;
        if (!std::isfinite(rMax)) rMax = 1.0;

        // ── 4. 跨 0° 边界处理（核心修复） ──
        // 判断矩形是否跨越 0° 射线（即正 X 轴）
        // 方法：检测矩形是否同时包含 theta 接近 0° 和接近 360° 的点
        //      或者矩形覆盖了超过半圆的范围
        bool crossesZero = (thetaMax - thetaMin > 180.0);

        // 更精确的跨 0° 检测：检查矩形是否覆盖正 X 轴
        // 正 X 轴上的点满足 y=0, x>0
        // 如果矩形包含正 X 轴上的某点，则 θ 范围应包含 0°
        // 检测正 X 轴与矩形的交集：x ∈ [viewRect.left(), viewRect.right()], y=0
        bool coversPositiveX = (viewRect.left() <= 0.0 && viewRect.right() >= 0.0)
            && (viewRect.top() <= 0.0 && viewRect.bottom() >= 0.0)
            && (viewRect.right() > 0.0); // 至少有一部分正 X 轴

        if (crossesZero || coversPositiveX) {
            // 返回完整圆盘的角度范围 [0°, 360°)
            qCDebug(logProjection) << "computeDataBounds: crosses 0° → returning full circle";
            return QRectF(0.0, rMin, nextafter(360.0, -INFINITY), rMax - rMin);
        }

        // ── 5. 正常返回（θ 范围不跨 0°） ──
        // 确保 thetaMin <= thetaMax
        if (thetaMin > thetaMax) {
            std::swap(thetaMin, thetaMax);
        }

        qCDebug(logProjection) << "computeDataBounds: viewRect" << viewRect
            << "→ θ[" << thetaMin << "," << thetaMax
            << "] r[" << rMin << "," << rMax << "]";

        return QRectF(thetaMin, rMin,
            thetaMax - thetaMin, rMax - rMin);
    }

    QRectF computeViewRect(const QRectF& dataBounds) const override {
        // dataBounds: (θMin, rMin, θSpan, rSpan)
        // 扇形在 Cartesian 空间的外接轴对齐包围盒
        qreal thetaMin = dataBounds.left();
        qreal thetaMax = dataBounds.left() + dataBounds.width();
        qreal rMin    = dataBounds.top();
        qreal rMax    = dataBounds.top() + dataBounds.height();

        // 遍历扇形的四条边界：内弧、外弧、两条径向边
        const int arcSamples = 32;
        qreal xMin = qInf(), xMax = -qInf(), yMin = qInf(), yMax = -qInf();

        // 内弧 (rMin, θ: thetaMin→thetaMax)
        for (int i = 0; i <= arcSamples; ++i) {
            qreal theta = thetaMin + static_cast<qreal>(i) / arcSamples * (thetaMax - thetaMin);
            QPointF c = toCartesian(theta, rMin);
            xMin = qMin(xMin, c.x()); xMax = qMax(xMax, c.x());
            yMin = qMin(yMin, c.y()); yMax = qMax(yMax, c.y());
        }
        // 外弧 (rMax, θ: thetaMin→thetaMax)
        for (int i = 0; i <= arcSamples; ++i) {
            qreal theta = thetaMin + static_cast<qreal>(i) / arcSamples * (thetaMax - thetaMin);
            QPointF c = toCartesian(theta, rMax);
            xMin = qMin(xMin, c.x()); xMax = qMax(xMax, c.x());
            yMin = qMin(yMin, c.y()); yMax = qMax(yMax, c.y());
        }
        // 两条径向边已在弧的采样中覆盖（θ=thetaMin 和 θ=thetaMax 都有采样）

        qCDebug(logProjection) << "computeViewRect: dataBounds" << dataBounds
                          << "→ [" << xMin << "," << xMax << "] × [" << yMin << "," << yMax << "]";

        return QRectF(xMin, yMin, xMax - xMin, yMax - yMin);
    }

    QRectF defaultDataBounds() const override {
        // 默认：完整圆盘，半径 10
        return QRectF(0, 0, 360, 10);
    }
};
