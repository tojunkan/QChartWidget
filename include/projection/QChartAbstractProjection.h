// QChartAbstractProjection.h —— 2D/3D 投影统一抽象基类
// 职责：
//   1. 定义 Numeric ↔ Cartesian 的核心映射接口（toCartesian / fromCartesian）
//   2. 提供通用辅助函数：createPath（曲线采样 → 图元列表）
//   3. 维度信息与采样提示
// 派生类：QChartProjection (2D)、QChartProjection3D (3D)
#pragma once

#include "QChartPrimitive.h"
#include <QVector3D>
#include <QStringList>
#include <functional>
#include <cmath>

class QChartAbstractProjection {
public:
    
    // 坐标系类型枚举
    enum class CoordinateSystem {
        Cartesian,
        Polar,
        Functional,
        Cartesian3D,
        Spherical,
        Cylindrical,
        Functional3D
    };

    explicit QChartAbstractProjection(const QStringList& dimNames)
        : m_dimNames(dimNames) {}

    virtual ~QChartAbstractProjection() = default;

    // ===== 核心映射（纯虚，所有子类必须实现） =====
    /// Numeric 空间 → Cartesian 空间（正向）
    virtual QVector3D toCartesian(const QVector3D& num) const = 0;

    // 输入：vec3 num（Numeric 坐标）
    // 输出：vec3 cart（Cartesian 坐标）
    virtual QString glslToCartesian() const = 0;

    /// Cartesian 空间 → Numeric 空间（反向；奇点返回 NaN）
    virtual QVector3D fromCartesian(const QVector3D& cart) const = 0;

    virtual QString glslFromCartesian() const = 0;

    /// 维度数（2 或 3）
    virtual int dimension() const = 0;

    /// 维度名称（用于 Axis 标签）
    QString dimensionName(int dim) const {
        if (dim >= 0 && dim < m_dimNames.size())
            return m_dimNames[dim];
        return QString();
    }

    // ===== 辅助信息（可被子类重写） =====
    /// 是否为恒等映射（快速通道优化，默认 false）
    virtual bool isIdentityMapping() const { return false; }

    /// 曲线采样段数提示（弯曲投影建议 64，恒等映射建议 2）
    virtual int samplingSegmentsHint() const { return 32; }

    // ================================================================
    // ★★★ 通用辅助函数：曲线 → 图元列表（Numeric 空间采样） ★★★
    // ================================================================
    /// <summary>
    /// 将参数曲线 dataCurve(t) 采样为 QChartPrimitive 列表。
    /// - 每个输出图元都是连续的 Path（内部顶点全部有效）。
    /// - 遇到 NaN/Inf 自动断开，结束当前图元并开启新图元。
    /// - 只填充 numVerts（Numeric 坐标），不调用 toCartesian()。
    /// - 调用方需自行设置颜色/线宽等样式属性。
    /// </summary>
    /// <param name="dataCurve">t∈[0,1] → Numeric 三元组</param>
    /// <param name="segments">采样段数</param>
    /// <param name="out">输出图元列表（追加模式，不清空）</param>
    void createPath(std::function<QVector3D(qreal t)> dataCurve,
                    int segments,
                    QVector<QChartPrimitive>& out) const
    {
        if (!dataCurve) return;

        QChartPrimitive current;
        current.type = QChartPrimitive::Type::Path;

        for (int i = 0; i <= segments; ++i) {
            qreal t = static_cast<qreal>(i) / segments;
            QVector3D num = dataCurve(t);

            // 检查有限性
            if (!std::isfinite(num.x()) || !std::isfinite(num.y()) || !std::isfinite(num.z())) {
                // 遇到无效点：结束当前图元（如果有顶点的话）
                if (!current.numVerts.isEmpty()) {
                    out.append(current);
                    current.numVerts.clear(); // 重置状态
                }
                continue;
            }

            // 有效点：追加到当前图元
            current.numVerts.append(num);
        }

        // 收尾最后一个图元
        if (!current.numVerts.isEmpty()) {
            out.append(current);
        }
    }

protected:
    QStringList m_dimNames;   // 维度名称列表
};