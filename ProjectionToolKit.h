#pragma once
#include "QFunctionalProjection.h"
#include "QChartProjectionFactory.h"  // 提供 createFunctional
#include <QtMath>
#include <memory>

// ============ 1. 恒等映射（Cartesian） ============
// 有效范围：x：-inf~+inf
//           y：-inf~+inf
inline std::unique_ptr<QChartProjection> createIdentityProjection() {
    return QChartProjectionFactory::createFunctional(
        [](qreal x, qreal y) { return QPointF(x, y); },
        [](qreal u, qreal v) { return QPointF(u, v); },
        QRectF(-5, -5, 10, 10),  // 任意范围
        nullptr, nullptr, "x", "y"
    );
}

// ============ 2. 幂函数 w = z² = (x² - y², 2xy) ============
//有效范围：x：-inf~+inf
//           y：-inf~+inf
inline std::unique_ptr<QChartProjection> createPower2Projection() {
    return QChartProjectionFactory::createFunctional(
        // forward
        [](qreal x, qreal y) -> QPointF {
            return QPointF(x * x - y * y, 2 * x * y);
        },
        // backward: 主平方根（u>0 分支）
        [](qreal u, qreal v) -> QPointF {
            qreal r = qSqrt(u * u + v * v);
            if (r == 0) return QPointF(0, 0);
            qreal theta = qAtan2(v, u) / 2.0;
            qreal s = qSqrt(r);
            return QPointF(s * qCos(theta), s * qSin(theta));
        },
        QRectF(-3, -3, 6, 6),  // 输入范围 [-3,3]×[-3,3]，输出约 [-18,18]
        nullptr, nullptr, "x", "y"
    );
}

// ============ 3. 指数函数 w = e^z = (e^x cos y, e^x sin y) ============
//有效范围：x：-inf~+inf
//           y：-inf~+inf（原点为奇点）
inline std::unique_ptr<QChartProjection> createExpProjection() {
    return QChartProjectionFactory::createFunctional(
        [](qreal x, qreal y) -> QPointF {
            qreal ex = qExp(x);
            return QPointF(ex * qCos(y), ex * qSin(y));
        },
        [](qreal u, qreal v) -> QPointF {
            qreal r = qSqrt(u * u + v * v);
            if (r == 0) return QPointF(qQNaN(), qQNaN()); // 对数奇点
            return QPointF(qLn(r), qAtan2(v, u)); // 主值 arg ∈ (-π, π]
        },
        QRectF(-3, -M_PI, 6, 2 * M_PI),  // x∈[-3,3], y∈[-π,π] 一个周期
        nullptr, nullptr, "x", "y"
    );
}

// ============ 4. 对数函数 w = Log z = (ln|z|, arg z) ============
//有效范围：x：-inf~+inf（主值为-pi~+pi）
//           y：-inf~+inf
inline std::unique_ptr<QChartProjection> createLogProjection() {
    return QChartProjectionFactory::createFunctional(
        [](qreal x, qreal y) -> QPointF {
            qreal r = qSqrt(x * x + y * y);
            if (r == 0) return QPointF(qQNaN(), qQNaN()); // 奇点
            return QPointF(qLn(r), qAtan2(y, x)); // 主值 arg ∈ (-π, π]
        },
        [](qreal u, qreal v) -> QPointF {
            qreal eu = qExp(u);
            return QPointF(eu * qCos(v), eu * qSin(v));
        },
        QRectF(-3, -M_PI, 6, 2 * M_PI),  // 输入 |z|∈[e^-3, e^3], arg∈[-π,π]
        nullptr, nullptr, "u", "v"
    );
}

// ============ 5. 平方根主分支 w = √z = (√r cos(θ/2), √r sin(θ/2)) ============
//有效范围：x：0~+inf
//          y：-inf~+inf
inline std::unique_ptr<QChartProjection> createSqrtProjection() {
    return QChartProjectionFactory::createFunctional(
        [](qreal x, qreal y) -> QPointF {
            qreal r = qSqrt(x * x + y * y);
            if (r == 0) return QPointF(0, 0);
            qreal theta = qAtan2(y, x); // (-π, π]
            qreal s = qSqrt(r);
            return QPointF(s * qCos(theta / 2.0), s * qSin(theta / 2.0));
        },
        [](qreal u, qreal v) -> QPointF {
            // 平方逆：z = w²
            return QPointF(u * u - v * v, 2 * u * v);
        },
        QRectF(-4, -4, 8, 8),  // 输入在圆内，输出在右半平面（u≥0）
        nullptr, nullptr, "x", "y"
    );
}

// ============ 6. 莫比乌斯变换 w = (az+b)/(cz+d) ============
//有效范围：x：-inf~+inf
//		   y：-inf~+inf（奇点为 c z + d = 0）
inline std::unique_ptr<QChartProjection> createMobiusProjection(
    qreal a = 1, qreal b = 0, qreal c = 0.5, qreal d = 1)
{
    return QChartProjectionFactory::createFunctional(
        // forward: 复数除法
        [a, b, c, d](qreal x, qreal y) -> QPointF {
            qreal denom_r = c * x + d;
            qreal denom_i = c * y;
            qreal denom2 = denom_r * denom_r + denom_i * denom_i;
            if (qFuzzyIsNull(denom2)) return QPointF(qQNaN(), qQNaN());
            // (a z + b) * conj(c z + d) / |c z + d|²
            qreal num_r = a * x + b;
            qreal num_i = a * y;
            qreal u = (num_r * denom_r + num_i * denom_i) / denom2;
            qreal v = (num_i * denom_r - num_r * denom_i) / denom2;
            return QPointF(u, v);
        },
        // backward: 逆变换 z = (d w - b) / (a - c w)
        [a, b, c, d](qreal u, qreal v) -> QPointF {
            qreal denom_r = a - c * u;
            qreal denom_i = -c * v;
            qreal denom2 = denom_r * denom_r + denom_i * denom_i;
            if (qFuzzyIsNull(denom2)) return QPointF(qQNaN(), qQNaN());
            qreal num_r = d * u - b;
            qreal num_i = d * v;
            qreal x = (num_r * denom_r + num_i * denom_i) / denom2;
            qreal y = (num_i * denom_r - num_r * denom_i) / denom2;
            return QPointF(x, y);
        },
        QRectF(-5, -5, 10, 10),  // 典型范围
        nullptr, nullptr, "x", "y"
    );
}

// ============ 7. 茹科夫斯基变换 w = z + 1/z ============
//有效范围：x：-inf~+inf
//		   y：-inf~+inf（奇点为 z=0）
inline std::unique_ptr<QChartProjection> createJoukowskiProjection() {
    return QChartProjectionFactory::createFunctional(
        [](qreal x, qreal y) -> QPointF {
            qreal r2 = x * x + y * y;
            if (qFuzzyIsNull(r2)) return QPointF(qQNaN(), qQNaN());
            return QPointF(x + x / r2, y - y / r2);  // 实部: x + x/r², 虚部: y - y/r²
        },
        [](qreal u, qreal v) -> QPointF {
            // 逆变换：z = (w + sqrt(w² - 4)) / 2，取主平方根
            qreal a = u * u - v * v - 4;
            qreal b = 2 * u * v;
            qreal r = qSqrt(a * a + b * b);
            qreal sqrtU = qSqrt((r + a) / 2);
            qreal sqrtV = (b >= 0) ? qSqrt((r - a) / 2) : -qSqrt((r - a) / 2);
            return QPointF((u + sqrtU) / 2, (v + sqrtV) / 2);
        },
        QRectF(-3, -3, 6, 6),  // 单位圆外区域
        nullptr, nullptr, "x", "y"
    );
}

// ============ 8. 正弦变换 w = sin z = (sin x cosh y, cos x sinh y) ============
inline std::unique_ptr<QChartProjection> createSinProjection() {
    return QChartProjectionFactory::createFunctional(
        [](qreal x, qreal y) -> QPointF {
            return QPointF(qSin(x) * cosh(y), qCos(x) * sinh(y));
        },
        [](qreal u, qreal v) -> QPointF {
            // 逆变换较复杂，此处返回 NaN（仅作演示）
            Q_UNUSED(u); Q_UNUSED(v);
            return QPointF(qQNaN(), qQNaN());
        },
        QRectF(-M_PI, -2, 2 * M_PI, 4),  // x∈[-π,π], y∈[-2,2]
        nullptr, nullptr, "x", "y"
    );
}

// ============ 9. 等距鱼眼投影（二维入射角→图像平面） ============
//有效范围：θx：-π~+π
//          θy：-π~+π
inline std::unique_ptr<QChartProjection> createFisheyeEquidistantProjection(qreal f = 1.0) {
    return QChartProjectionFactory::createFunctional(
        [f](qreal thetaX, qreal thetaY) -> QPointF {
            qreal r = f * qSqrt(thetaX * thetaX + thetaY * thetaY);
            qreal phi = qAtan2(thetaY, thetaX);
            return QPointF(r * qCos(phi), r * qSin(phi));
        },
        [f](qreal x, qreal y) -> QPointF {
            qreal r = qSqrt(x * x + y * y);
            if (r == 0) return QPointF(0, 0);
            qreal theta = r / f;   // 等距投影逆公式
            qreal phi = qAtan2(y, x);
            return QPointF(theta * qCos(phi), theta * qSin(phi));
        },
        QRectF(-M_PI, -M_PI, 2 * M_PI, 2 * M_PI),  // 入射角范围 [-π,π]²
        nullptr, nullptr, "θx", "θy"
    );
}

// ============ 10. 漩涡扭曲（极坐标旋转） ============
//有效范围：x:-radius~+radius
//          y:-radius~+radius
inline std::unique_ptr<QChartProjection> createSwirlProjection(qreal strength = 0.5, qreal radius = 5.0) {
    return QChartProjectionFactory::createFunctional(
        [strength, radius](qreal x, qreal y) -> QPointF {
            qreal r = qSqrt(x * x + y * y);
            if (r < 1e-6 || r > radius) return QPointF(x, y);
            qreal theta = qAtan2(y, x) + strength * (radius - r);
            return QPointF(r * qCos(theta), r * qSin(theta));
        },
        [strength, radius](qreal u, qreal v) -> QPointF {
            qreal r = qSqrt(u * u + v * v);
            if (r < 1e-6 || r > radius) return QPointF(u, v);
            qreal theta = qAtan2(v, u) - strength * (radius - r);
            return QPointF(r * qCos(theta), r * qSin(theta));
        },
        QRectF(-5, -5, 10, 10),  // 5×5 范围
        nullptr, nullptr, "x", "y"
    );
}