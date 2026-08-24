// QChartProjection3D.h —— 3D 坐标投影基类（header-only）
// 职责：Numeric 空间 ↔ World 空间的双向映射 + World 包围盒（采样法）+ 3D 路径生成
// 3D 链路: Data ─[Axis::toNumeric]─► Numeric ─[Projection3D::toWorld]─► World(x,y,z)
//         ─[Camera::viewProjectionMatrix]─► Clip ─[QChartMath::clipToNdc]─► NDC ─[视口]─► Pixel
// 与 2D QChartProjection 同构；2D 家族零改动。
#pragma once
#include <QVector3D>
#include <QString>
#include <QVector>
#include <functional>
#include <cmath>
#include <utility>

/// World 包围盒（axis-aligned，供 fit 与 batch 用）
/// ⚠ 归属说明：design_3d.md §4.2 将 QChartWorldBox/QChartProjectedPoint 的归属指定为
///   QChartCamera3D.h；该文件属 t6 任务、尚不存在，而本头（t5）的 computeWorldBounds
///   已需要完整定义，故此处先行定义。t6 落地 QChartCamera3D.h 时应 include 本头复用，
///   不要重复定义（避免 ODR 冲突）。
struct QChartWorldBox {
    QVector3D min;
    QVector3D max;
};

/// 3D 投影基类：Numeric → World 正向、World → Numeric 反向（奇点 NaN）、
/// 包围盒采样、默认数据范围、数据曲线 → World 折线（NaN 断路径）
class QChartProjection3D {
public:
    QChartProjection3D(QString name0 = "x", QString name1 = "y", QString name2 = "z")
        : m_name0(std::move(name0)), m_name1(std::move(name1)), m_name2(std::move(name2)) {}

    virtual ~QChartProjection3D() = default;

    QString dimensionName(int dim) const {
        if (dim == 0) return m_name0;
        if (dim == 1) return m_name1;
        if (dim == 2) return m_name2;
        return QString();
    }

    // ===== Numeric → World（正向；n2 默认 0 → 2 参数曲面嵌入直接用）=====
    /// NaN/Inf 自然传播，调用方负责跳过
    virtual QVector3D toWorld(qreal n0, qreal n1, qreal n2 = 0.0) const = 0;

    // ===== World → Numeric（反向；奇点 NaN 策略延续；未提供反向时为 nullptr 语义 → NaN + qWarning）=====
    virtual QVector3D fromWorld(const QVector3D& w) const = 0;

    // ===== 包围盒（Numeric 盒 → World 盒；采样法，网格默认 16×16×16）=====
    /// 沿三轴 16 段网格采样 toWorld，取有限点 min/max；
    /// 全 NaN 回退 dataMin/dataMax（与 2D FunctionalProjection 兜底一致）
    virtual QChartWorldBox computeWorldBounds(const QVector3D& dataMin,
                                              const QVector3D& dataMax) const {
        const int grid = 16;
        qreal minX = qInf(), maxX = -qInf();
        qreal minY = qInf(), maxY = -qInf();
        qreal minZ = qInf(), maxZ = -qInf();
        for (int i = 0; i <= grid; ++i) {
            qreal n0 = dataMin.x() + (static_cast<qreal>(i) / grid) * (dataMax.x() - dataMin.x());
            for (int j = 0; j <= grid; ++j) {
                qreal n1 = dataMin.y() + (static_cast<qreal>(j) / grid) * (dataMax.y() - dataMin.y());
                for (int k = 0; k <= grid; ++k) {
                    qreal n2 = dataMin.z() + (static_cast<qreal>(k) / grid) * (dataMax.z() - dataMin.z());
                    QVector3D w = toWorld(n0, n1, n2);
                    if (std::isfinite(w.x()) && std::isfinite(w.y()) && std::isfinite(w.z())) {
                        minX = qMin(minX, w.x()); maxX = qMax(maxX, w.x());
                        minY = qMin(minY, w.y()); maxY = qMax(maxY, w.y());
                        minZ = qMin(minZ, w.z()); maxZ = qMax(maxZ, w.z());
                    }
                }
            }
        }
        if (qIsInf(minX)) return QChartWorldBox{ dataMin, dataMax }; // 全 NaN → 回退
        return QChartWorldBox{ QVector3D(minX, minY, minZ), QVector3D(maxX, maxY, maxZ) };
    }

    // ===== 初始值（Widget3D 首次 fit 用）=====
    virtual std::pair<QVector3D, QVector3D> defaultDataBounds() const {
        return { QVector3D(0, 0, 0), QVector3D(10, 10, 10) }; // 与 2D QRectF(0,0,10,10) 对齐
    }

    // ===== 快速通道与采样段数提示（design_3d_axes.md §5.4，additive）=====
    /// 直线采样段数提示：弯曲投影 32，Cartesian3D 恒等 → 2（两点直线）
    virtual int samplingSegmentsHint() const { return 32; }
    /// 恒等映射快速通道（用户定案）：恒等映射下 fromWorld/toWorld ≡ 恒等，
    /// 反算 dataBounds 免采样（§2.2 快速通道）、图元生成免 toWorld/分段（直接 Num→World 直通）
    virtual bool isIdentityMapping() const { return false; }

    // ===== 数据曲线 → World 折线（NaN 断路径；返回子路径列表）=====
    /// dataCurve: t∈[0,1] → Numeric 三元组；每段子路径内连续，NaN 处断开
    QVector<QVector<QVector3D>>
    createPath3D(std::function<QVector3D(qreal t)> dataCurve, int segments = 64) const {
        QVector<QVector<QVector3D>> result;
        if (!dataCurve) return result;

        QVector<QVector3D> current;
        for (int i = 0; i <= segments; ++i) {
            qreal t = static_cast<qreal>(i) / segments;
            QVector3D numeric = dataCurve(t);
            QVector3D w = toWorld(numeric.x(), numeric.y(), numeric.z());

            // NaN/Inf → 断开路径，重开新子路径
            if (!std::isfinite(w.x()) || !std::isfinite(w.y()) || !std::isfinite(w.z())) {
                if (!current.isEmpty()) {
                    result.append(current);
                    current.clear();
                }
                continue;
            }
            current.append(w);
        }
        if (!current.isEmpty()) result.append(current);
        return result;
    }

protected:
    QString m_name0;
    QString m_name1;
    QString m_name2;
};
