// QFunctionalProjection.h —— 用户自定义坐标投影
// 通过 lambda 定义 Numeric ↔ View Cartesian 映射，无需子类化
// 最简用法：只传 forward + backward 两个 lambda
// 包络转换（dataToView / viewToData）：不传则默认恒等，适用于鱼眼、扭曲 Cartesian 等场景
#pragma once
#include "QChartProjection.h"
#include <functional>
#include <QDebug>

class QFunctionalProjection : public QChartProjection {
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

    // ── 包络转换（默认恒等）──
    QRectF computeDataBounds(const QRectF& viewRect) const override {
        if (m_viewToData)
            return m_viewToData(viewRect);
        // 默认：恒等映射
        return viewRect;
    }

    QRectF computeViewRect(const QRectF& dataBounds) const override {
        if (m_dataToView)
            return m_dataToView(dataBounds);
        // 默认：恒等映射
        return dataBounds;
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
