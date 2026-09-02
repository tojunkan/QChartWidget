// QChartProjection3D.h —— 3D 坐标投影基类
// 继承自 QChartAbstractProjection，扩展 3D 独有的包围盒（ViewCube）和曲面生成
// Numeric 空间维度：3 (x, y, z)
#pragma once

#include "QChartAbstractProjection.h"
#include "ViewCube.h"
#include <QVector3D>
#include <utility>

class QChartProjection3D : public QChartAbstractProjection {
public:
    QChartProjection3D(QString name0 = "x", QString name1 = "y", QString name2 = "z")
        : QChartAbstractProjection({name0, name1, name2}) {}

    virtual ~QChartProjection3D() = default;

    // ===== 3D 核心映射（保留原有纯虚接口） =====
    /// n2 默认 0，支持 2D 曲面嵌入 3D 空间
    virtual QVector3D toCartesian(qreal n0, qreal n1, qreal n2 = 0.0) const = 0;
    virtual QVector3D fromCartesian(const QVector3D& w) const = 0;

    // ===== 3D 包围盒（采样法，默认 16×16×16） =====
    virtual ViewCube computeViewCube(const QVector3D& dataMin,
                                               const QVector3D& dataMax) const;

    virtual std::pair<QVector3D, QVector3D> defaultDataBounds() const {
        return { QVector3D(0, 0, 0), QVector3D(10, 10, 10) };
    }

    // ===== 实现统一基类接口（final） =====
    QVector3D toCartesian(const QVector3D& num) const final {
        return toCartesian(num.x(), num.y(), num.z());
    }

    int dimension() const final { return 3; }

    // ================================================================
    // ★★★ 3D 独有辅助：参数曲面 → TriangleMesh 图元（Numeric 空间采样） ★★★
    // ================================================================
    /// <summary>
    /// 将参数曲面 dataSurface(u,v) 采样为 TriangleMesh 图元。
    /// - 行主序顶点 (v 为外循环，u 为内循环)。
    /// - 只填充 numVerts 和 numIndices（Numeric 坐标），不调用 forward()。
    /// - 遇到 NaN/Inf 顶点时，自动跳过该四边形的两个三角形（鲁棒退化处理）。
    /// - 调用方需自行设置颜色/填充色等样式属性。
    /// </summary>
    /// <param name="dataSurface">(u,v)∈[0,1]² → Numeric 三元组</param>
    /// <param name="uSeg">u 方向分段数</param>
    /// <param name="vSeg">v 方向分段数</param>
    /// <param name="out">输出的单个 TriangleMesh 图元（追加前请确保 out 是干净的）</param>
    void createSurface(std::function<QVector3D(qreal u, qreal v)> dataSurface,
                       int uSeg,
                       int vSeg,
                       QChartPrimitive& out) const
    {
        if (!dataSurface || uSeg < 1 || vSeg < 1) return;

        out.type = QChartPrimitive::Type::TriangleMesh;
        out.numVerts.clear();
        out.numIndices.clear();

        const int vertsU = uSeg + 1;
        const int vertsV = vSeg + 1;
        out.numVerts.reserve(vertsU * vertsV);

        // ---- 1. 生成顶点（行主序：v 外循环，u 内循环） ----
        for (int j = 0; j <= vSeg; ++j) {
            qreal v = static_cast<qreal>(j) / vSeg;
            for (int i = 0; i <= uSeg; ++i) {
                qreal u = static_cast<qreal>(i) / uSeg;
                QVector3D num = dataSurface(u, v);
                out.numVerts.append(num);
            }
        }

        // ---- 2. 生成索引（跳过包含无效顶点的四边形） ----
        out.numIndices.reserve(uSeg * vSeg * 6);

        for (int j = 0; j < vSeg; ++j) {
            for (int i = 0; i < uSeg; ++i) {
                int idx00 = j * vertsU + i;
                int idx10 = j * vertsU + i + 1;
                int idx01 = (j + 1) * vertsU + i;
                int idx11 = (j + 1) * vertsU + i + 1;

                // 检查四个顶点是否全部有效
                const QVector3D& v00 = out.numVerts[idx00];
                const QVector3D& v10 = out.numVerts[idx10];
                const QVector3D& v01 = out.numVerts[idx01];
                const QVector3D& v11 = out.numVerts[idx11];

                bool allFinite = std::isfinite(v00.x()) && std::isfinite(v00.y()) && std::isfinite(v00.z()) &&
                                 std::isfinite(v10.x()) && std::isfinite(v10.y()) && std::isfinite(v10.z()) &&
                                 std::isfinite(v01.x()) && std::isfinite(v01.y()) && std::isfinite(v01.z()) &&
                                 std::isfinite(v11.x()) && std::isfinite(v11.y()) && std::isfinite(v11.z());

                if (!allFinite) {
                    // 跳过这个四边形（退化为洞）
                    continue;
                }

                // 三角形 1: (i,j) -> (i+1,j) -> (i,j+1)
                out.numIndices.append(idx00);
                out.numIndices.append(idx10);
                out.numIndices.append(idx01);

                // 三角形 2: (i+1,j) -> (i+1,j+1) -> (i,j+1)
                out.numIndices.append(idx10);
                out.numIndices.append(idx11);
                out.numIndices.append(idx01);
            }
        }
    }
};

// ===== computeViewRect 的默认实现（采样法，可被子类覆盖） =====
inline ViewCube QChartProjection3D::computeViewCube(const QVector3D& dataMin,
                                                              const QVector3D& dataMax) const
{
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
                QVector3D w = toCartesian(n0, n1, n2);
                if (std::isfinite(w.x()) && std::isfinite(w.y()) && std::isfinite(w.z())) {
                    minX = qMin(minX, w.x()); maxX = qMax(maxX, w.x());
                    minY = qMin(minY, w.y()); maxY = qMax(maxY, w.y());
                    minZ = qMin(minZ, w.z()); maxZ = qMax(maxZ, w.z());
                }
            }
        }
    }

    // 全 NaN 回退到 Numeric 盒
    if (!std::isfinite(minX)) {
        return ViewCube{ dataMin, dataMax };
    }
    return ViewCube{ QVector3D(minX, minY, minZ), QVector3D(maxX, maxY, maxZ) };
}