// QChartAxes3D.h —— 3D 轴参照系编排器（非 Q_OBJECT）
// 职责：仅提供盒几何（8角/12边/spine）及轴配置容器，刻度生成委托给 QChartAxis
#ifndef QCHARTAXES3D_H
#define QCHARTAXES3D_H

#include "QCube.h"
#include <QVector3D>
#include <QVector>
#include <QPair>
#include <QString>
#include <QPointF>

class QChartAxis;

class QChartAxes3D {
public:
    QChartAxes3D() = default;

    struct AxisConfig {
        QChartAxis* axis = nullptr;
        bool visible = true;
        qreal markerSizePx = 4.0;
        QPointF labelOffsetPx{0, 0};
        bool axisTitleVisible = true;
        QString axisTitle;
    };

    // 4b：数据盒不再独立持有——需要时按需从三根轴的当前范围组装（用完即弃）。
    /// 组装数据盒：各维取该维轴的范围（axis 缺失的维度回退默认 0..10，与迁移前默认盒一致）。
    QCube dataBounds() const;

    AxisConfig& axis(int dim) { return m_cfg[dim]; }
    const AxisConfig& axis(int dim) const { return m_cfg[dim]; }

    bool visible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }

    // 纯几何工具（静态）
    static QVector<QVector3D> boxCorners(const QVector3D& dataMin, const QVector3D& dataMax);
    static QVector<QPair<int, int>> boxEdges();
    static QVector<int> spineEdgeIndices();

    /// FaceLine 模式（批次2 B）：退化安全的固定值——0 落在 [lo,hi] 内取 0，否则取区间中点。
    /// 用于角度维等"0 是安全值"的方向（球坐标的 θ=φ=0：赤道/本初子午线，远离极点）。
    static qreal safeFixedValue(qreal lo, qreal hi);

    /// FaceLine 模式的安全轴线：**沿 dim2 轴**（即"面外/等值"那一维，面就固定在它的中值上），
    /// dim0 / dim1 固定在 safeFixedValue（远离极点/奇点）。
    /// 于是：球坐标 = θ=φ=0 的**半径线**（穿过 r=mid 的球面）；柱坐标 = r=θ=0 的 **z 轴线**；
    ///       直角坐标 = x=y=0 的 **z 轴线**。刻度标签取 dim2 轴自己的。
    static QPair<QVector3D, QVector3D> faceLineSegment(const QVector3D& dataMin,
                                                       const QVector3D& dataMax);

private:
    AxisConfig m_cfg[3];
    bool m_visible = true;
};

#endif // QCHARTAXES3D_H