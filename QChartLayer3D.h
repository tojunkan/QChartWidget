// QChartLayer3D.h —— 3D 图层：三轴（axisX/Y/Z 只做 toNumeric）+ 3D 系列 + 全链闭包组装 + 图元收集
// 职责（design_3d.md §7.1，t11 按 designer 修订）：
//   - 系列存入基类 m_series（复用图例/主题/调色板/所有权/析构），同时登记 m_series3D 类型化遍历
//   - makeProjectFn 组装 ProjectFn3D 全链闭包：
//       QVariant×3 --[axisX/Y/Z::toNumeric]→ qreal×3 --[projection3D::toWorld]→ QVector3D
//       --[camera3D::project]→ QChartProjectedPoint{screen, depth}
//   - collectPrimitives：曲面 worldCache 直算填充（自身 axis+projection3D，不走系列闭包，裁决 b）
//     + 3D 系列图元（全链闭包）+ 辅助网格地板（dataIndex=-1，depth 经 camera 直算）
//   - 轴刻度不在 3D 场景绘制（D-3D-13）；axisX/Y/Z 仅数值化
#ifndef QCHARTLAYER3D_H
#define QCHARTLAYER3D_H

#include "QChartLayer.h"
#include "QChartSeries3D.h"
#include <QVector>

class QChartCamera3D;
class QChartProjection3D;
class QChartSurfaceSeries;
class QChartPrimitive;

class QChartLayer3D : public QChartLayer {
    Q_OBJECT
public:
    explicit QChartLayer3D(QObject* parent = nullptr);

    // ===== 轴（Z 轴仅 toNumeric；X/Y 复用基类）=====
    void setAxisZ(QChartAxis* a);
    QChartAxis* axisZ() const { return m_axisZ; }

    // ===== 3D 系列管理（存入基类 m_series + 登记 m_series3D）=====
    void addSeries3D(QChartSeries3D* s);
    void removeSeries3D(QChartSeries3D* s);
    QList<QChartSeries3D*> series3DList() const { return m_series3D; }

    // ===== 3D 投影（Widget3D 注入，非持有）=====
    void setProjection3D(const QChartProjection3D* proj) { m_projection3D = proj; }
    const QChartProjection3D* projection3D() const { return m_projection3D; }

    // ===== 辅助网格地板（可选，D-3D-13；y=0 平面 World 线段走同一 3D 路径）=====
    void setGridFloorVisible(bool v);
    bool gridFloorVisible() const { return m_gridFloorVisible; }
    /// half<=0 → 默认 10.0（Widget3D 可按 worldBounds 外接半径设置）
    void setGridFloorHalfSize(qreal half);
    qreal gridFloorHalfSize() const { return m_gridFloorHalf; }

    // ===== 图元收集（Renderer 3D 路径调用；不排序、不绘制）=====
    void collectPrimitives(const QChartCamera3D* cam, const QRectF& plotArea,
                           QVector<QChartPrimitive>& out) const;

    /// 组装 ProjectFn3D 全链闭包（toNumeric×3 → toWorld → camera.project）
    /// public：QChartWidget3D 悬停命中（t13）复用同一闭包做屏幕近邻
    ProjectFn3D makeProjectFn(const QChartCamera3D* cam, const QRectF& plotArea) const;

protected:
    QChartAxis* m_axisZ = nullptr;
    QList<QChartSeries3D*> m_series3D;
    const QChartProjection3D* m_projection3D = nullptr;  // Widget3D 注入（非持有）
    bool m_gridFloorVisible = false;
    qreal m_gridFloorHalf = 0.0;   // 0 = 默认（10.0）
};

#endif // QCHARTLAYER3D_H
