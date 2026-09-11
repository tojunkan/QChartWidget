// QChartLayer3D.h —— 3D 图层（纯 Numeric 图元组装，无投影）
#ifndef QCHARTLAYER3D_H
#define QCHARTLAYER3D_H

#include "QChartLayer.h"
#include "QChartSeries3D.h"
#include "QChartAxes3D.h"
#include "QChartCamera3D.h"   // 值成员 m_camera3D 需完整类型（批次 B1）
#include <QVector>
#include <memory>

class QChartProjection3D;
class QChartSurfaceSeries;
class QChartTextLabel;

class QChartLayer3D : public QChartLayer {
    Q_OBJECT
public:
    /// 3D 网格模式（批次2 B）：
    ///   Box      = 盒：现有盒几何 12 条边（含主轴脊）+ 底面网格；三条主轴按刻度逐个标注
    ///              （LabelMode::Tickwise；标签优先显式坐标，退化时走图元绑定回退）。
    ///              **仅三维直角坐标（Cartesian3D）有效**——其它投影 qWarning 并自动回退 FaceLine。
    ///   FaceLine = 面线（默认）：本阶段只画一条退化安全的轴线（QChartAxes3D::faceLineSegment，
    ///              球坐标即 θ=φ=0 半径线），按刻度逐个标注；面不画（FACE-DEFERRED，待曲面系列阶段）。
    ///   Lattice  = 晶格：只画线（三主轴 + 三族网格 + 盒 12 边），LabelMode::None——无任何文字。
    enum class GridMode { Box, FaceLine, Lattice };

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
    void setDataBounds(const QVector3D& dataMin, const QVector3D& dataMax);
    bool hasValidDataBounds() const;

    // 图元收集（纯 Numeric，无投影）
    void collectPrimitives();

    // ===== 相机/场景访问（批次 B1：3D 相机值成员 + scene3D 上下文，模式同 2D layer）=====
    QChartCamera3D* camera3D() { return &m_camera3D; }
    const QChartCamera3D* camera3D() const { return &m_camera3D; }
    void setScene3DProjection(const QChartAbstractProjection* p) { m_scene3D.projection = p; }
    void setScene3DPlotArea(const QRectF& plotArea) { m_scene3D.plotArea = plotArea; }
    void setScene3DBackground(const QColor& c) { m_scene3D.backgroundColor = c; }
    const QChartScene& scene3D() const { return m_scene3D; }
    QChartScene& scene3D() { return m_scene3D; }

protected:
    // 系列脏标记挂钩

    QChartAxis* m_axisZ = nullptr;
    QList<QChartSeries3D*> m_series3D;
    const QChartProjection3D* m_projection3D = nullptr;
    QChartCamera3D m_camera3D;
    QChartScene m_scene3D;  // 3D 场景快照（由 collectPrimitives 填充）

    std::unique_ptr<QChartAxes3D> m_axes3D;
    GridMode m_gridMode = GridMode::FaceLine;   // 批次2 B：FaceLine 为默认模式
    QVector3D m_axesDataMin, m_axesDataMax;
};

#endif // QCHARTLAYER3D_H