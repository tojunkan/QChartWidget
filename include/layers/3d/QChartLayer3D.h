// QChartLayer3D.h —— 3D 图层（纯 Numeric 图元组装，无投影）
// 4a：改继承维度无关抽象层 QChartAbstractLayer（不再继承二维 QChartLayer）；场景快照并入继承的
// m_scene（原 m_scene3D 删除，scene3D()/setScene3D* 保留为三维命名访问口）；三维层自持本层用到的
// 轴绑定（axisX/axisY/axisZ）与网格样式（gridVisible/gridColor + gridChanged），行为与迁移前一致。
#ifndef QCHARTLAYER3D_H
#define QCHARTLAYER3D_H

#include "QChartAbstractLayer.h"
#include "QChartSeries3D.h"
#include "QChartAxes3D.h"
#include "QChartCamera3D.h"   // 值成员 m_camera3D 需完整类型（批次 B1）
#include <QVector>
#include <memory>
#include <optional>

class QChartAxis;
class QChartProjection3D;
class QChartSurfaceSeries;
class QChartTextLabel;

class QChartLayer3D : public QChartAbstractLayer {
    Q_OBJECT
    // 4a-fix（t37 F1）：re-parent 后属性面丢失 → 按基线（继承二维层时期）逐字恢复两条 Q_PROPERTY：
    //   gridVisible=1、gridColor=2（index 0 为 QObject::objectName）；READ/WRITE/NOTIFY 指向本类已自持的
    //   网格样式访问器与 gridChanged 信号，语义与基线一致。
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible NOTIFY gridChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY gridChanged)
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

    // 轴（4a：三维层自持三轴绑定；setAxisX/Y 同步 axes3D 编排器）
    void setAxisX(QChartAxis* a);
    void setAxisY(QChartAxis* a);
    void setAxisZ(QChartAxis* a);
    QChartAxis* axisX() const { return m_axisX; }
    QChartAxis* axisY() const { return m_axisY; }
    QChartAxis* axisZ() const { return m_axisZ; }

    // 网格样式（4a：自持；语义与原二维层成员一致）
    bool isGridVisible() const { return m_gridVisible; }
    void setGridVisible(bool v);
    QColor gridColor() const { return m_gridColorOverride.value_or(m_themeGridColor); }
    void setGridColor(const QColor& c);
    void setThemeGridColor(const QColor& c);
    void clearGridColor();
    std::optional<QColor> gridColorOverride() const { return m_gridColorOverride; }

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

    // 轴/网格数据盒（4b：不独立持有——写入 = 写三根轴的范围；读取 = 从三轴按需组装）
    // 4c：盒子类型统一为 QCube（主签名）；保留两点重载仅为调用便利，内部统一走 QCube。
    void setDataBounds(const QCube& box);
    void setDataBounds(const QVector3D& dataMin, const QVector3D& dataMax)   // 便捷重载（转发 QCube 主签名）
    { setDataBounds(QCube(dataMin, dataMax)); }
    bool hasValidDataBounds() const;

    // 图元收集（纯 Numeric，无投影）
    void collectPrimitives() override;                 // 4a：实现抽象层接口
    void recomputeDataBounds() override;               // 4a：与迁移前继承的二维空默认一致（空实现）
    /// 4a：边框轴外边距占用（用本层 axisX/axisY；算术与迁移前经二维层继承的实现逐字一致）
    void borderAxisSizeHint(const QFont& font, qreal& left, qreal& top,
                            qreal& right, qreal& bottom) const override;

    // ===== 相机/场景访问（批次 B1：3D 相机值成员 + scene3D 上下文，模式同 2D layer）=====
    QChartCamera3D* camera3D() { return &m_camera3D; }
    const QChartCamera3D* camera3D() const { return &m_camera3D; }
    // 4a：场景快照 = 继承的 m_scene（原 m_scene3D 已并入）；以下为三维命名访问口，语义不变
    void setScene3DProjection(const QChartAbstractProjection* p) { m_scene.projection = p; }
    void setScene3DPlotArea(const QRectF& plotArea) { m_scene.plotArea = plotArea; }
    void setScene3DBackground(const QColor& c) { m_scene.backgroundColor = c; }
    const QChartScene& scene3D() const { return m_scene; }
    QChartScene& scene3D() { return m_scene; }

signals:
    /// 网格样式变化（4a：自持；语义同原二维层信号）
    void gridChanged();

protected:
    // 系列脏标记挂钩

    QChartAxis* m_axisX = nullptr;   // 4a：三维层自持（原从二维层继承）
    QChartAxis* m_axisY = nullptr;
    QChartAxis* m_axisZ = nullptr;
    QList<QChartSeries3D*> m_series3D;
    const QChartProjection3D* m_projection3D = nullptr;
    QChartCamera3D m_camera3D;
    // m_scene 由抽象层提供（4a：原 m_scene3D 并入）

    std::unique_ptr<QChartAxes3D> m_axes3D;
    GridMode m_gridMode = GridMode::FaceLine;   // 批次2 B：FaceLine 为默认模式
    // 4b：m_axesDataMin/m_axesDataMax 已删除（数据盒由三根轴范围组装；需要时 m_axes3D->dataBounds()）

    // 网格样式（4a：自持；字段与语义同原二维层）
    bool m_gridVisible = true;
    std::optional<QColor> m_gridColorOverride;           // 用户显式设过（setGridColor）
    QColor m_themeGridColor = QColor(220, 220, 220);     // 主题注入默认（setThemeGridColor）
};

#endif // QCHARTLAYER3D_H