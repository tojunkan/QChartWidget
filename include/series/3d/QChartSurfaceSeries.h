// QChartSurfaceSeries.h —— 3D 曲面系列（行主序网格）
// Data 层：网格 QVector<QDataPoint3D>（rows×cols 行主序）；setParametricGrid 便捷生成 (u,v) 格点。
// 收集产物（第 2 步统一走 QChartPrimitive.h 的 generatePrimitive / generateWireframe）：
//   · RenderMode::Surface（默认）：一个 Type::Grid 图元（rows×cols 共享控制点，索引由渲染端派生）
//   · SurfaceWithGrid：上面的 Grid + 网格连线（rows+cols 条 Path，线比面更实、压在面上）
//   · GridOnly：只出网格连线
// 曲面本身由渲染端负责弯曲（GL: quads 域 TCS/TES；CPU: 按控制点画平面三角）。
#pragma once
#include "QChartSeries3D.h"

class QChartSurfaceSeries : public QChartSeries3D {
    Q_OBJECT
public:
    /// 渲染模式（只影响"吐哪些图元"，与数据无关）。
    /// 以后若要加等高线之类的辅助显示，在这里加一个取值即可。
    enum class RenderMode {
        Surface,          // 只画面（默认，画面与加本枚举之前完全一致）
        SurfaceWithGrid,  // 面 + 网格连线（辅助显示，线比面更实、压在面上）
        GridOnly          // 只画网格连线（线框模式）
    };

    explicit QChartSurfaceSeries(const QString& name = {}, QObject* parent = nullptr);

    RenderMode renderMode() const { return m_renderMode; }
    void setRenderMode(RenderMode m) {
        if (m_renderMode == m) return;
        m_renderMode = m;
        emit dataChanged();   // 产物集合变了 → 触发 layer 重收（与数据变化同一条通路）
    }

    // ===== Data 层：网格（行主序，rows×cols）=====
    /// pts.size() 必须 == rows*cols，否则忽略并 qWarning（不改变状态）
    void setGrid(int rows, int cols, const QVector<QDataPoint3D>& pts);
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    QDataPoint3D gridAt(int row, int col) const;
    /// 便捷：按参数域生成 (u,v) 格点（x=u, y=v, z 未用），u∈[u0,u1]、v∈[v0,v1]
    void setParametricGrid(int rows, int cols,
                           qreal u0, qreal u1, qreal v0, qreal v1);

    // ===== 图元收集：一个 TriangleMesh（Numeric 顶点 + 三角索引）=====
    void collectPrimitives(QChartSegment& seg,
                           const QChartAxis* ax, const QChartAxis* ay,
                           const QChartAxis* az = nullptr) const override;

    // ===== World 层缓存：基类 QChartSeries3D::worldCache（t51 上移，VBO 源）=====

signals:
    void gridChanged();

private:
    int m_rows = 0, m_cols = 0;
    RenderMode m_renderMode = RenderMode::Surface;
};
