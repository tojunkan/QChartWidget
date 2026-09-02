// QChartLayer3D.h —— 3D 图层（纯 Numeric 图元组装，无投影）
#ifndef QCHARTLAYER3D_H
#define QCHARTLAYER3D_H

#include "QChartLayer.h"
#include "QChartSeries3D.h"
#include "QChartAxes3D.h"
#include <QVector>
#include <memory>

class QChartCamera3D;
class QChartProjection3D;
class QChartSurfaceSeries;
class QChartTextLabel;

class QChartLayer3D : public QChartLayer {
    Q_OBJECT
public:
    enum class GridMode { Box, Lattice };

    explicit QChartLayer3D(QObject* parent = nullptr);

    // 轴（Z 轴单独）
    void setAxisX(QChartAxis* a);
    void setAxisY(QChartAxis* a);
    void setAxisZ(QChartAxis* a);
    QChartAxis* axisZ() const { return m_axisZ; }

    // 3D 系列管理
    void addSeries3D(QChartSeries3D* s);
    void removeSeries3D(QChartSeries3D* s);
    QList<QChartSeries3D*> series3DList() const { return m_series3D; }

    // 3D 投影（仅用于系列的世界缓存和采样提示）
    void setProjection3D(const QChartProjection3D* proj);
    const QChartProjection3D* projection3D() const { return m_projection3D; }

    // 轴参照系编排器
    QChartAxes3D* axes3D() { return m_axes3D.get(); }
    const QChartAxes3D* axes3D() const { return m_axes3D.get(); }

    // 网格模式
    void setGridMode(GridMode m) { m_gridMode = m; }
    GridMode gridMode() const { return m_gridMode; }

    // 轴/网格数据盒
    void setAxesDataBox(const QVector3D& dataMin, const QVector3D& dataMax);
    bool hasValidAxesDataBox() const;

    // 图元收集（纯 Numeric，无投影）
    void collectPrimitives(QChartScene& scene) const;

    // 组装 ProjectFn3D（供系列使用，保留）
    ProjectFn3D makeProjectFn(const QChartCamera3D* cam, const QRectF& plotArea) const;

protected:
    // 系列脏标记挂钩
    void hookSeriesDirty(QChartSeries3D* s);
    void unhookSeriesDirty(QChartSeries3D* s);

    QChartAxis* m_axisZ = nullptr;
    QList<QChartSeries3D*> m_series3D;
    const QChartProjection3D* m_projection3D = nullptr;
    std::unique_ptr<QChartAxes3D> m_axes3D;
    GridMode m_gridMode = GridMode::Box;
    QVector3D m_axesDataMin{0,0,0}, m_axesDataMax{0,0,0};
    mutable bool m_worldCacheDirty = true;
};

#endif // QCHARTLAYER3D_H