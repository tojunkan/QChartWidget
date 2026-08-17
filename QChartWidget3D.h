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

class QChartProjection3D;

class QChartWidget3D : public QChartWidget {
    Q_OBJECT
public:
    explicit QChartWidget3D(QWidget* parent = nullptr);

    // ===== 3D 相机（构造时内置默认 QChartCamera3D，可替换）=====
    QChartCamera3D* camera3D() const { return m_camera3D.get(); }   // 非持有
    void setCamera3D(std::unique_ptr<QChartCamera3D> cam);

    // ===== 3D 投影（必须设置；setProjection3D 时自动按 defaultDataBounds fit 相机）=====
    void setProjection3D(std::unique_ptr<QChartProjection3D> proj);
    const QChartProjection3D* projection3D() const { return m_projection3D.get(); }

    // ===== 3D 图层 =====
    void addLayer3D(QChartLayer3D* g);   // 内部：基类 addLayer（复用图例/主题/调色板接线）+ 登记 layers3D
    QList<QChartLayer3D*> layers3D() const { return m_layers3D; }

    // ===== 坐标 / fit =====
    QPointF worldToPixel(const QVector3D& w) const;   // camera3D->project(...).screen
    void fitWorld();   // projection3D->computeWorldBounds(defaultDataBounds) → camera3D->fitToBounds

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

    // ===== 场景钩子（§8.2 重写填 3D 段；2D 字段留默认）=====
    QChartScene buildScreenScene() const override;
    QChartScene buildExportScene(QChartExportScope scope, const QSize& size,
                                 QSizeF& outDeviceSize) const override;

private:
    /// 3D 悬停简化版（§8.3 修订）：屏幕近邻（图元与鼠标 <8px 取最近）→ dataIndex → Data (u,v)
    /// → 发 uvHovered/uvHoveredEnd；不弹 tooltip（D-3D-13）
    void updateHover(const QPointF& pos);
    static qreal distToPrimitive(const QPointF& pos, const QChartPrimitive& prim);

    std::unique_ptr<QChartCamera3D> m_camera3D;
    std::unique_ptr<QChartProjection3D> m_projection3D;
    QList<QChartLayer3D*> m_layers3D;
    QChartWorldBox m_worldBounds;

    // 交互状态
    bool m_orbitDrag = false;
    bool m_panDrag = false;
    QPointF m_pressPos;      // 左键按下位置（点击 vs 拖拽判定）
    QPointF m_lastPos;
    qreal m_orbitSensitivity = 0.3;     // 度/像素
    qreal m_dollySensitivity = 0.1;     // k：factor = exp(-notch·k)

    // 悬停状态
    bool m_hoverActive = false;
    QPointF m_lastHoverUV;
};

#endif // QCHARTWIDGET3D_H
