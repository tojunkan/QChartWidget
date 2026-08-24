// QChartFunctionalProjection3D.h —— 用户自定义 3D 坐标投影
// 通过 lambda 定义 Numeric ↔ World 映射，无需子类化。
// 统一支持两类用法：
//   - 2→3 参数曲面嵌入：forward 忽略 n2（如莫比乌斯环 u∈[0,360), v∈[-0.5,0.5]）
//   - 3→3 坐标变换：forward 使用全部三个 Numeric 分量
// 反向（backward）可不提供：nullptr 时 fromWorld 返回 NaN + qWarning。
// 包围盒默认走基类 16×16×16 采样；可传 boundsFn 覆盖。
#pragma once
#include "QChartProjection3D.h"
#include <functional>
#include <QDebug>

class QChartFunctionalProjection3D : public QChartProjection3D {
public:
    /// forward: Numeric (n0,n1,n2) → World，必传
    /// backward: World → Numeric；nullptr → fromWorld 返回 NaN
    /// defaultDataMin/Max: 默认 Numeric 范围（Widget3D 首次 fit 用）
    /// boundsFn: 自定义包围盒计算；nullptr → 内部采样
    QChartFunctionalProjection3D(
        std::function<QVector3D(qreal n0, qreal n1, qreal n2)> forward,
        std::function<QVector3D(qreal x, qreal y, qreal z)> backward = nullptr,
        QVector3D defaultDataMin = QVector3D(0, 0, 0),
        QVector3D defaultDataMax = QVector3D(1, 1, 1),
        std::function<QChartWorldBox(const QVector3D&, const QVector3D&)> boundsFn = nullptr,
        QString name0 = "u", QString name1 = "v", QString name2 = "w")
        : QChartProjection3D(std::move(name0), std::move(name1), std::move(name2))
        , m_forward(std::move(forward))
        , m_backward(std::move(backward))
        , m_defaultDataMin(defaultDataMin)
        , m_defaultDataMax(defaultDataMax)
        , m_boundsFn(std::move(boundsFn))
    {}

    // ── Numeric → World ──
    QVector3D toWorld(qreal n0, qreal n1, qreal n2 = 0.0) const override {
        if (!m_forward) {
            qWarning() << "QChartFunctionalProjection3D::toWorld: forward mapping is null";
            return QVector3D(qQNaN(), qQNaN(), qQNaN());
        }
        return m_forward(n0, n1, n2);
    }

    // ── World → Numeric ──
    QVector3D fromWorld(const QVector3D& w) const override {
        if (!m_backward) {
            qWarning() << "QChartFunctionalProjection3D::fromWorld: backward mapping is null, returning NaN";
            return QVector3D(qQNaN(), qQNaN(), qQNaN());
        }
        return m_backward(w.x(), w.y(), w.z());
    }

    // ── 包围盒：自定义优先，否则基类采样 ──
    QChartWorldBox computeWorldBounds(const QVector3D& dataMin,
                                      const QVector3D& dataMax) const override {
        if (m_boundsFn)
            return m_boundsFn(dataMin, dataMax);
        return QChartProjection3D::computeWorldBounds(dataMin, dataMax);
    }

    // ── 初始值 ──
    std::pair<QVector3D, QVector3D> defaultDataBounds() const override {
        return { m_defaultDataMin, m_defaultDataMax };
    }

private:
    std::function<QVector3D(qreal n0, qreal n1, qreal n2)> m_forward;
    std::function<QVector3D(qreal x, qreal y, qreal z)> m_backward;
    QVector3D m_defaultDataMin;
    QVector3D m_defaultDataMax;
    std::function<QChartWorldBox(const QVector3D&, const QVector3D&)> m_boundsFn;
};
