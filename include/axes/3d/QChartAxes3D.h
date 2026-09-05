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

    QCube dataBounds;

    AxisConfig& axis(int dim) { return m_cfg[dim]; }
    const AxisConfig& axis(int dim) const { return m_cfg[dim]; }

    bool visible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }

    // 纯几何工具（静态）
    static QVector<QVector3D> boxCorners(const QVector3D& dataMin, const QVector3D& dataMax);
    static QVector<QPair<int, int>> boxEdges();
    static QVector<int> spineEdgeIndices();

private:
    AxisConfig m_cfg[3];
    bool m_visible = true;
};

#endif // QCHARTAXES3D_H