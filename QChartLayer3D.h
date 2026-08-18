// QChartLayer3D.h —— 3D 图层（design_3d.md §7.1 + design_3d_axes.md §8.3 扩展）
// 职责：
//   - 系列存入基类 m_series（复用图例/主题/调色板/所有权/析构），同时登记 m_series3D 类型化遍历
//   - 持有 QChartAxes3D 编排器（拥有；默认绑定 axisX/Y/Z → dim0/1/2，setAxisX/Y/Z 时自动重绑）
//   - makeProjectFn 组装 ProjectFn3D 全链闭包（axis toNumeric×3 → projection3D.toWorld → camera.project）
//   - collectPrimitives：轴/网格图元（axesDataBox 有效时，Grid/ForegroundDecor 分层）+ 系列图元
//     + 曲面 worldCache 直算填充；labels 可选出参（QChartTextLabel billboard）
//   - 快速通道（§5.4）：isIdentityMapping()==true → emitLine 免 toWorld（Numeric≡World 直通）、
//     段数 = samplingSegmentsHint()（Cartesian3D=2）
//   - ⚠ 三层分离：本类做 toWorld/投影（Layer3D 是 3D 侧唯一投影点）；QChartAxes3D 只产 Numeric 几何
//   - ⚠ gridFloorVisible/gridFloorHalfSize 已移除（§8.5 并入 Box 模式地板网格）
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
    enum class GridMode { Box, Lattice };   // §5.1，默认 Box

    explicit QChartLayer3D(QObject* parent = nullptr);

    // ===== 轴（Z 轴仅 toNumeric；X/Y 复用基类）=====
    /// 重绑时同步 axes3D 配置槽（dim0→axisX、dim1→axisY、dim2→axisZ）
    void setAxisX(QChartAxis* a);
    void setAxisY(QChartAxis* a);
    void setAxisZ(QChartAxis* a);
    QChartAxis* axisZ() const { return m_axisZ; }

    // ===== 3D 系列管理（存入基类 m_series + 登记 m_series3D）=====
    void addSeries3D(QChartSeries3D* s);
    void removeSeries3D(QChartSeries3D* s);
    QList<QChartSeries3D*> series3DList() const { return m_series3D; }

    // ===== 3D 投影（Widget3D 注入，非持有）=====
    void setProjection3D(const QChartProjection3D* proj) { m_projection3D = proj; }
    const QChartProjection3D* projection3D() const { return m_projection3D; }

    // ===== 轴参照系编排器（拥有；§8.3）=====
    QChartAxes3D* axes3D() { return m_axes3D.get(); }
    const QChartAxes3D* axes3D() const { return m_axes3D.get(); }

    // ===== 网格模式（§5.1）=====
    void setGridMode(GridMode m) { m_gridMode = m; }
    GridMode gridMode() const { return m_gridMode; }
    // gridVisible 沿用基类（默认 true）：网格总开关

    // ===== 轴/网格数据盒（Numeric；Widget3D 注入：dataBounds3D 或 A9 域盒）=====
    /// 默认 (0,0,0)-(0,0,0) = 无效 → 不生成任何轴/网格图元（现有直接组装场景零影响）
    void setAxesDataBox(const QVector3D& dataMin, const QVector3D& dataMax) {
        m_axesDataMin = dataMin;
        m_axesDataMax = dataMax;
    }
    bool hasValidAxesDataBox() const;

    // ===== 图元收集（签名向后兼容：labels 可选出参；分层经 QChartPrimitive::Layer）=====
    void collectPrimitives(const QChartCamera3D* cam, const QRectF& plotArea,
                           QVector<QChartPrimitive>& out,
                           QVector<QChartTextLabel>* labels = nullptr) const;

    /// 组装 ProjectFn3D 全链闭包（toNumeric×3 → toWorld → camera.project）
    /// public：QChartWidget3D 悬停命中（t13）复用同一闭包做屏幕近邻
    ProjectFn3D makeProjectFn(const QChartCamera3D* cam, const QRectF& plotArea) const;

protected:
    /// Numeric 点 → Screen（toWorld → camera3D->project）
    QChartProjectedPoint projectNumeric(const QVector3D& num, const QChartCamera3D* cam,
                                        const QRectF& plotArea) const;
    /// 直线采样（Numeric 两点 → 段数 = projection3D->samplingSegmentsHint() 个线段图元；
    /// identity 快速通道：免 toWorld，Numeric≡World 直通；每段 depth=段中点；任一端 NaN 跳过该段）
    void emitLine(QVector3D numA, QVector3D numB, QChartPrimitive::Layer layer,
                  const QColor& color, qreal penWidth, const QChartCamera3D* cam,
                  const QRectF& plotArea, QVector<QChartPrimitive>& out) const;
    /// 该维刻度值（axes3D 委托；axis 为 null 返回空）
    QVector<qreal> dimTicks(int dim) const;

    QChartAxis* m_axisZ = nullptr;
    QList<QChartSeries3D*> m_series3D;
    const QChartProjection3D* m_projection3D = nullptr;  // Widget3D 注入（非持有）
    std::unique_ptr<QChartAxes3D> m_axes3D;
    GridMode m_gridMode = GridMode::Box;
    QVector3D m_axesDataMin{0, 0, 0}, m_axesDataMax{0, 0, 0};
};

#endif // QCHARTLAYER3D_H
