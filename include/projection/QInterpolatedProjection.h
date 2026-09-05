// QInterpolatedProjection.h —— 合成投影
// 两个投影之间的平滑过渡：toCartesian 在两个投影的结果之间 lerp
// 动画期间由 QProjectionSwitchAnimation 临时挂到 Widget 上驱动 blend
#pragma once
#include "QChartProjection.h"

class QInterpolatedProjection : public QChartProjection {
    Q_OBJECT
public:
    /// a/b 非持有（a 由 Widget 持有，b 由调用者/动画持有）
    QInterpolatedProjection(QChartProjection* a, QChartProjection* b);

    /// 混合因子：0=pureA, 1=pureB
    void setBlend(qreal alpha) { m_alpha = alpha; }
    qreal blend() const { return m_alpha; }

    // ===== QChartProjection 接口 =====
    CoordinateSystem type() const override { return m_b ? m_b->type() : CoordinateSystem::Cartesian; }

    /// 核心：两投影的 toCartesian 之间 lerp
    QPointF toCartesian(qreal num0, qreal num1) const override;
    QPointF fromCartesian(qreal x, qreal y) const override;

    // 使用插值投影的 Shader 必须显式声明 uniform float u_blendAlpha;
    // 并在渲染循环中通过 setUniformValue 将 C++ 端的 m_alpha 传入。
    QString glslToCartesian() const override {
        if (!m_a || !m_b) return "num";
        // 拼接两个子投影的表达式，并用 GLSL 内置 mix 进行线性插值
        return QString("mix( (%1), (%2), u_blendAlpha )")
            .arg(m_a->glslToCartesian())
            .arg(m_b->glslToCartesian());
    }

    QString glslFromCartesian() const override {
        if (!m_a || !m_b) return "cart";
        // 反向映射同样插值
        return QString("mix( (%1), (%2), u_blendAlpha )")
            .arg(m_a->glslFromCartesian())
            .arg(m_b->glslFromCartesian());
    }

    /// 包络委托给目标投影 B（动画期间不 pan/zoom，仅刻度/包络用近似值）
    QRectF computeDataBounds(const QRectF& viewRect) const override;
    QRectF computeViewRect(const QRectF& dataBounds) const override;

private:
    QChartProjection* m_a = nullptr;
    QChartProjection* m_b = nullptr;
    qreal m_alpha = 0.0;
};
