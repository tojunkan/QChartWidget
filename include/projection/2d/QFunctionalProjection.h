// QFunctionalProjection.h —— 用户自定义坐标投影
// 通过 lambda 定义 Numeric ↔ View Cartesian 映射，无需子类化
// 最简用法：只传 forward + backward 两个 lambda
// 包络转换（dataToView / viewToData）：不传则默认恒等，适用于鱼眼、扭曲 Cartesian 等场景
#pragma once
#include "QChartProjection.h"
#include <functional>
#include <QDebug>

class QFunctionalProjection : public QChartProjection {
    Q_OBJECT
public:
    /// <summary>
    /// 构造函数
    /// </summary>
    /// <param name="forward">Numeric (num0,num1) → View Cartesian (x,y)，必须提供</param>
    /// <param name="backward">View Cartesian (x,y) → Numeric (num0,num1)；null 时 fromCartesian 返回 NaN</param>
    /// <param name="defaultBounds">默认 Numeric 范围，用于初始 viewRect</param>
    /// <param name="dataToView">dataBounds → viewRect；null 时默认恒等</param>
    /// <param name="viewToData">viewRect → dataBounds；null 时默认恒等</param>
    QFunctionalProjection(
        std::function<QPointF(qreal num0, qreal num1)> forward,
        std::function<QPointF(qreal x, qreal y)> backward = nullptr,
        QRectF defaultBounds = QRectF(0, 0, 10, 10),
        std::function<QRectF(const QRectF&)> dataToView = nullptr,
        std::function<QRectF(const QRectF&)> viewToData = nullptr,
		QString name0 = "x", 
        QString name1 = "y"
    )
        : m_forward(std::move(forward))
        , m_backward(std::move(backward))
        , m_defaultBounds(defaultBounds)
        , m_dataToView(std::move(dataToView))
        , m_viewToData(std::move(viewToData))
		, QChartProjection(name0, name1)
    {}

    CoordinateSystem type() const override { return CoordinateSystem::Functional; }

    // ── Numeric ↔ View Cartesian ──
    QPointF toCartesian(qreal num0, qreal num1) const override {
        if (!m_forward) {
            qWarning() << "QFunctionalProjection::toCartesian: forward mapping is null";
            return QPointF(qQNaN(), qQNaN());
        }
        QPointF result = m_forward(num0, num1);
        // NaN/Inf 自然传播——调用方负责跳过
        return result;
    }

    QPointF fromCartesian(qreal x, qreal y) const override {
        if (!m_backward) {
            qWarning() << "QFunctionalProjection::fromCartesian: backward mapping is null, returning NaN";
            return QPointF(qQNaN(), qQNaN());
        }
        QPointF result = m_backward(x, y);
        return result;
    }

    // ── 包络转换 ──
    QRectF computeDataBounds(const QRectF& viewRect) const override {
        if (m_viewToData)
            return m_viewToData(viewRect);
        // fallback: 32×32 网格采样 fromCartesian，与 PolarProjection 一致
        const int grid = 32;
        qreal d0Min = qInf(), d0Max = -qInf(), d1Min = qInf(), d1Max = -qInf();
        for (int i = 0; i <= grid; ++i) {
            qreal x = viewRect.left() + (static_cast<qreal>(i) / grid) * viewRect.width();
            for (int j = 0; j <= grid; ++j) {
                qreal y = viewRect.top() + (static_cast<qreal>(j) / grid) * viewRect.height();
                QPointF data = fromCartesian(x, y);
                if (std::isfinite(data.x()) && std::isfinite(data.y())) {
                    d0Min = qMin(d0Min, data.x());
                    d0Max = qMax(d0Max, data.x());
                    d1Min = qMin(d1Min, data.y());
                    d1Max = qMax(d1Max, data.y());
                }
            }
        }
        if (qIsInf(d0Min)) return viewRect; // all NaN → 回退恒等
        return QRectF(d0Min, d1Min, d0Max - d0Min, d1Max - d1Min);
    }

    QRectF computeViewRect(const QRectF& dataBounds) const override {
        if (m_dataToView)
            return m_dataToView(dataBounds);
        // fallback: 对 dataBounds 边界采样 toCartesian，估算 Cartesian 包围盒
        const int grid = 16;
        qreal xMin = qInf(), xMax = -qInf(), yMin = qInf(), yMax = -qInf();
        qreal d0Min = dataBounds.left(), d0Max = d0Min + dataBounds.width();
        qreal d1Min = dataBounds.top(),  d1Max = d1Min + dataBounds.height();
        for (int i = 0; i <= grid; ++i) {
            qreal d0 = d0Min + (static_cast<qreal>(i) / grid) * (d0Max - d0Min);
            for (int j = 0; j <= grid; ++j) {
                qreal d1 = d1Min + (static_cast<qreal>(j) / grid) * (d1Max - d1Min);
                QPointF cart = toCartesian(d0, d1);
                if (std::isfinite(cart.x()) && std::isfinite(cart.y())) {
                    xMin = qMin(xMin, cart.x());
                    xMax = qMax(xMax, cart.x());
                    yMin = qMin(yMin, cart.y());
                    yMax = qMax(yMax, cart.y());
                }
            }
        }
        if (qIsInf(xMin)) return dataBounds; // all NaN → 回退恒等
        return QRectF(xMin, yMin, xMax - xMin, yMax - yMin);
    }

    QRectF defaultDataBounds() const override {
        return m_defaultBounds;
    }

private:
    std::function<QPointF(qreal num0, qreal num1)> m_forward;
    std::function<QPointF(qreal x, qreal y)> m_backward;
    QRectF m_defaultBounds;
    std::function<QRectF(const QRectF&)> m_dataToView;
    std::function<QRectF(const QRectF&)> m_viewToData;
};
