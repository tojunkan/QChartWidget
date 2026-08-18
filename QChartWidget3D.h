// QChartWidget3D.h —— 3D 图表控件（: QChartWidget 子类，design_3d.md §8.3 定案）
// 3D 特定 API 与交互（orbit/dolly/pan + 悬停命中 + 联动信号）全部隔离在本类；
// 基类只提供 §8.2 两处钩子（buildScreenScene/buildExportScene 虚化），2D 类行为零变化。
// 构造时序 ⚠：先 setProjection(默认 QCartesianProjection) 满足基类流程（addLayer 接线/
//   布局/2D 相机初始化）；该 2D projection 仅占位——渲染走 camera3D/layers3D 的 3D 段。
#ifndef QCHARTWIDGET3D_H
#define QCHARTWIDGET3D_H

#include "QChartWidget.h"
#include "QChartCamera3D.h"
#include "QChartLayer3D.h"
#include "QChartProjection3D.h"   // QChartWorldBox
#include <memory>
#include <optional>

class QChartProjection3D;
class QOpenGLChartRenderer;

class QChartWidget3D : public QChartWidget {
    Q_OBJECT
public:
    /// 渲染后端（design_phase3.md §2.2，A9：GL 默认、QPainter 保底；t46 拾取分支用，开关/环境变量属 t48）
    enum class RenderBackend { OpenGL, QPainter };
    explicit QChartWidget3D(QWidget* parent = nullptr);
    ~QChartWidget3D() override;   // GlHost（内嵌 QOpenGLWidget）析构需完整类型 → cpp 定义

    // ===== 渲染后端（§2.2 ⚠ 统一后端原则：本开关同时决定渲染与拾取，禁止混搭；t48 接环境变量）=====
    void setRenderBackend(RenderBackend b);
    RenderBackend renderBackend() const { return m_renderBackend; }

    // ===== 3D 相机（构造时内置默认 QChartCamera3D，可替换）=====
    QChartCamera3D* camera3D() const { return m_camera3D.get(); }   // 非持有
    void setCamera3D(std::unique_ptr<QChartCamera3D> cam);

    // ===== 3D 投影（必须设置；setProjection3D 时自动按 defaultDataBounds fit 相机）=====
    void setProjection3D(std::unique_ptr<QChartProjection3D> proj);
    const QChartProjection3D* projection3D() const { return m_projection3D.get(); }

    // ===== 3D 图层 =====
    void addLayer3D(QChartLayer3D* g);   // 内部：基类 addLayer（复用图例/主题/调色板接线）+ 登记 layers3D
    QList<QChartLayer3D*> layers3D() const { return m_layers3D; }

    // ===== 显式域盒（Numeric；A3 优先级最高，design_3d_axes.md §3）=====
    void setDomainBox(const QVector3D& dataMin, const QVector3D& dataMax);
    void clearDomainBox();                       // 回退到 数据包围盒 > defaultDataBounds
    bool hasDomainBox() const;

    // ===== 视图→dataBounds 反算缓存（§2.2）=====
    QVector3D dataBounds3DMin() const { return m_dataBounds3DMin; }
    QVector3D dataBounds3DMax() const { return m_dataBounds3DMax; }
    bool dataBounds3DValid() const { return m_dataBounds3DValid; }   // false = A9 域盒兜底生效

    // ===== 坐标 / fit =====
    QPointF worldToPixel(const QVector3D& w) const;   // camera3D->project(...).screen
    /// A3 全链：resolveDataBox → computeWorldBounds → setViewCubeToFit → 反算 → 推轴盒 → 重绘
    void fitWorld();

    // ===== 双 Widget 联动信号（§9；(u,v) 为 Numeric 空间）=====
signals:
    void uvHovered(qreal u, qreal v);     // 悬停点 (u,v)（3D 侧 = 屏幕近邻命中）
    void uvSelected(qreal u, qreal v);    // 点击选中 (u,v)
    void uvHoveredEnd();

protected:
    // ===== 交互（重写；D-3D-4：手势 → Camera 几何）=====
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;

    // ===== 布局（Phase 3 GL 宿主跟随 plotArea；基类逻辑保留）=====
    void resizeEvent(QResizeEvent*) override;

    // ===== 场景钩子（§8.2 重写填 3D 段；2D 字段留默认）=====
    QChartScene buildScreenScene() const override;
    QChartScene buildExportScene(QChartExportScope scope, const QSize& size,
                                 QSizeF& outDeviceSize) const override;

private:
    /// 3D 悬停简化版（§8.3 修订）：屏幕近邻（QChartHitTester，Phase 3 任务 0）→ dataIndex → Data (u,v)
    /// → 发 uvHovered/uvHoveredEnd；不弹 tooltip（D-3D-13）
    void updateHover(const QPointF& pos);

    // ===== Phase 3 GL 宿主（t42，design_phase3.md §2.2/§7.3）=====
    /// GlHost 几何跟随 plotArea（resizeEvent 调用；plotArea 未就绪 → 全 widget）
    void layoutGlHost();

    // ===== 控制器（§2.2/§3）=====
    /// viewCube 5×5×5 网格采样 → fromWorld → min/max 聚合（非有限跳过，全 NaN → Valid=false）；
    /// Cartesian3D 快速通道（isIdentityMapping）免采样直接取盒 min/max
    void recomputeDataBounds3D();
    /// 更新各 layer3D 轴盒：dataBounds3D 有效用它，否则 A9 锚定域盒（静态）
    void pushAxesDataBoxToLayers();
    /// 数据包围盒：遍历 layers3D series3DList → points() → toNumeric×3 → min/max（一次性，非每帧）
    std::pair<QVector3D, QVector3D> computeSeriesDataBounds() const;
    /// A3 链：显式域盒 > 数据包围盒 > defaultDataBounds
    std::pair<QVector3D, QVector3D> resolveDataBox() const;

    std::unique_ptr<QChartCamera3D> m_camera3D;
    std::unique_ptr<QChartProjection3D> m_projection3D;
    QList<QChartLayer3D*> m_layers3D;
    QChartWorldBox m_worldBounds;

    // ===== Phase 3 GL 宿主（t42，§2.2 组合；渲染器挂接——后端开关属实现⑤ t48）=====
    class GlHost;                                  // 内嵌 QOpenGLWidget（cpp 定义）
    std::unique_ptr<GlHost> m_glHost;
    QOpenGLChartRenderer* m_glRenderer = nullptr;  // GL 渲染器（GlHost 生命周期内）
    RenderBackend m_renderBackend = RenderBackend::OpenGL;   // A9：GL 默认（QPainter 保底）

    // 视图→dataBounds 反算缓存（§2.2）
    QVector3D m_dataBounds3DMin{0, 0, 0}, m_dataBounds3DMax{0, 0, 0};
    bool m_dataBounds3DValid = false;
    // A3 域盒链缓存（A9 兜底：dataBounds3D 无效时轴/网格用此锚定盒，静态）
    std::pair<QVector3D, QVector3D> m_anchorBox{ QVector3D(0, 0, 0), QVector3D(0, 0, 0) };
    std::optional<QVector3D> m_domainMin, m_domainMax;

    // 交互状态（R6：平移无鼠标手势 → 仅 orbit 拖拽）
    bool m_orbitDrag = false;
    QPointF m_pressPos;      // 左键按下位置（点击 vs 拖拽判定）
    QPointF m_lastPos;
    qreal m_orbitSensitivity = 0.3;     // 度/像素
    qreal m_dollySensitivity = 0.1;     // k：factor = exp(-notch·k)

    // 悬停状态
    bool m_hoverActive = false;
    QPointF m_lastHoverUV;
};

#endif // QCHARTWIDGET3D_H
