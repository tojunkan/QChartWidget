// QChartWidget3D.h —— 3D 图表控件（批次 B2：轻量 3D 容器）
// 目标形态（用户确认）：保留 QChartWidget3D 子类，3D 特定 API 隔离本类，基类只做 2D 容器行为；
// 相机归 layer——与 QChartLayer3D 自带 m_camera3D 值成员一致，本类**不再持有相机成员**。
// 本类 = 单 QChartLayer3D 托管容器：域盒/投影/相机便捷/fit/worldToPixel 全部转发 layer3D；
// 渲染/布局/plotArea/广播/GL 宿主复用基类（renderLayers/renderLayersGL/pushContextToLayers
// 覆写为 3D 场景源：layer3D.scene3D + camera3D）。
// 旧实体（m_camera3D/m_projection3D 持有、GlHost、buildScreenScene 覆写、m_worldBounds(QChartWorldBox)、
// orbit/dolly 拖拽状态、hover/uv 联动信号）注释保留+恢复点标注，不编译引用。
#ifndef QCHARTWIDGET3D_H
#define QCHARTWIDGET3D_H

#include "QChartWidget.h"
#include "QChartLayer3D.h"
#include "QValueAxis.h"
#include "QCube.h"
#include <optional>

class QChartAxis;
class QChartProjection3D;
class QChartCamera3D;
class QPointF;

class QChartWidget3D : public QChartWidget {
    Q_OBJECT
public:
    explicit QChartWidget3D(QWidget* parent = nullptr);
    ~QChartWidget3D() override;

    // ===== 3D 图层（单 layer3D 托管：构造即建默认 layer3D 并接线基类 addLayer）=====
    QChartLayer3D* layer3D() { return m_layer3D; }
    const QChartLayer3D* layer3D() const { return m_layer3D; }
    void addLayer3D(QChartLayer3D* g);   // 内部：基类 addLayer 接线（兼容旧调用名）

    // ===== 三轴绑定（透传 layer3D；轴仍为 2D QValueAxis 实例，3D 数值化共用）=====
    void setAxisX3D(QChartAxis* a);
    void setAxisY3D(QChartAxis* a);
    void setAxisZ3D(QChartAxis* a);
    QChartAxis* axisX3D() const;
    QChartAxis* axisY3D() const;
    QChartAxis* axisZ3D() const;

    // ===== 域盒 API（转发 layer3D::setDataBounds；4c：统一 QCube 语义）=====
    void setDomainBox(const QCube& box);
    void setDomainBox(const QVector3D& dataMin, const QVector3D& dataMax)   // 便捷重载（转发 QCube 主签名）
    { setDomainBox(QCube(dataMin, dataMax)); }
    void clearDomainBox();               // 回退默认盒 (0,0,0)-(10,10,10)
    /// 4b：域盒数值落在三根轴上（本类不再持有 min/max 值），此处仅记录"是否用户显式设过"
    bool hasDomainBox() const { return m_domainBoxSet; }
    /// 4c：域盒访问器（QCube）——按需从 layer3D 的三轴组装（无层时返回无效 QCube）
    QCube domainBox() const;

    // ===== 投影便捷（转发 layer3D；相机 fit 提示见 fitWorld）=====
    void setProjection3D(const QChartProjection3D* proj);
    const QChartProjection3D* projection3D() const;

    // ===== 网格模式（批次2 B：Box / FaceLine（默认）/ Lattice，转发 layer3D）=====
    void setGridMode3D(QChartLayer3D::GridMode m);
    QChartLayer3D::GridMode gridMode3D() const;

    // ===== 相机（归 layer3D；本类无相机成员）=====
    QChartCamera3D* camera3D() { return m_layer3D ? m_layer3D->camera3D() : nullptr; }
    const QChartCamera3D* camera3D() const { return m_layer3D ? m_layer3D->camera3D() : nullptr; }

    // ===== fit / 坐标便捷（转发层相机）=====
    /// 相机 fit：取域盒（无则默认盒）外扩 12% 为 viewCube，并按 fov 重算 distance/near/far
    /// 4d：自动适配（autoFit）的**唯一消费点**——三条触发路径全部经此：
    ///   ① 域盒变化（setDomainBox/clearDomainBox）② 投影设置（setProjection3D）③ 显式调用。
    ///   · autoFit=true（默认）：数据锚点跟随域盒（外扩 6% → viewCube），镜头解算并入
    ///     QChartCamera3D::fitCameraConfig(plotArea, FitConstraint::FixedFov)（4c 手写式等价），
    ///     末尾复位姿态 yaw=45/pitch=30；
    ///   · autoFit=false：**完全不触碰相机**（viewCube 与 distance/fov/near/far/yaw/pitch 全部保持），
    ///     域盒仍写三轴范围——保留用户手调参数，用户可自行 setViewCube/fitCameraConfig 适配。
    void fitWorld();

    // ===== 4e：驱动链方向状态 =====
    //  数值侧（4d 已接线）：域盒/投影变化 → fitWorld（autoFit 门 + FixedFov 解算 + 锚点跟随 + 姿态复位）。
    //  像素侧（4e）：plotArea 变化 → 仅按 fitCameraConfig 重解算镜头（distance/fov/near/far；
    //    含竖高视口按窄边临界半视角的 4d 语义）——**不改数据锚点、不复位姿态**，autoFit=false 时零触碰。
    //  相机侧（record）：三维的“viewCube → 三轴范围”反算**本阶段不做**（留后，与系列包围盒同批）——
    //    三维轴范围当前只由域盒（数值侧）持有，相机侧（orbit/dolly/pan 手势）归 4f。
    int plotAreaFitCount() const { return m_plotAreaFitCount; }             // 诊断：像素侧驱动次数（解算实跑）
    int plotAreaFitChangeCount() const { return m_plotAreaFitChangeCount; } // 诊断：其中真正改变镜头的次数
    void resetDriveCounters() { m_plotAreaFitCount = 0; m_plotAreaFitChangeCount = 0; m_interactionFitCount = 0; }

    // ===== 4f：鼠标交互（只做“事件 → 既有相机 API”的接线；开关见基类 setInteractionEnabled）=====
    /// 左键拖动 = orbit(Δyaw, Δpitch)（俯仰由相机内部钳 ±89°）；
    /// 中键/右键拖动 = panViewCube（平移视野盒中心；不改盒尺寸、不触发 fit——语义：拖动内容，
    ///   中心反向平移，Y 屏向下对应世界 +Y；与 orbit 共用同一交互开关）；
    /// 滚轮 = dolly(factor)（改视野盒尺寸），并按 **autoFit 语义联动**：
    ///   autoFit 开 → 随后走既有 fit 路径（fitCameraConfig(plotArea, FixedFov)）重算 distance/近远面；
    ///   autoFit 关 → 保留用户手调参数（distance/fov/near/far 不变）。
    /// 说明：三维的 viewCube→三轴反算仍留后（见上），故交互后三轴范围不变（相机侧留后项）。
    static qreal orbitYawDelta(qreal dxPixels)   { return dxPixels * 0.5; }    // °/px（纯计算）
    static qreal orbitPitchDelta(qreal dyPixels) { return -dyPixels * 0.5; }   // °/px（屏幕向下 → 俯角减小）
    static qreal wheelDollyFactor(int angleDeltaY);                            // 120/格 → 1.10 倍
    int interactionFitCount() const { return m_interactionFitCount; }          // 诊断：滚轮触发的 fit 次数
    QPointF worldToPixel(const QVector3D& w) const;   // camera3D->project(w, plotArea).screen
    QCube viewCube() const;                           // 相机当前 viewCube（转发）
    void setViewCube(const QCube& box);               // 转发相机 + resetNearFar 提示

protected:
    void onPlotAreaChanged(const QRectF& newPlotArea) override;   // 4e：像素侧驱动（仅重解算镜头）
    // 4f：鼠标交互钩子（基类 final 事件分发 → 交互开关 → 本类实现；三维语义覆盖二维默认）
    void onMousePress(QMouseEvent* e) override;
    void onMouseMove(QMouseEvent* e) override;
    void onMouseRelease(QMouseEvent* e) override;
    void onWheel(QWheelEvent* e) override;
    // ===== 3D 渲染管线覆写（基类虚钩子，批次 B2）=====
    void pushContextToLayers() override;
    void renderLayers(QPaintDevice* device) override;
    void renderLayersGL(QPaintDevice* device) override;
    void onBeforePaint() override;                    // 3D 不走 2D axis→camera 同步链
    void drawExternalContent(QPainter& painter) override;  // 3D 无边框轴/标题外部内容

private:
    QChartLayer3D* m_layer3D = nullptr;               // 托管默认 layer3D（QObject 子，非持有于 m_layers）
    QValueAxis* m_axisX3D = nullptr;                  // 默认轴（可经 setAxisX3D 替换；owned by this）
    QValueAxis* m_axisY3D = nullptr;
    QValueAxis* m_axisZ3D = nullptr;
    bool m_domainBoxSet = false;        // 4b：域盒数值由三根轴持有，本类仅记录是否用户显式设过
    int  m_plotAreaFitCount = 0;        // 4e 诊断：plotArea 变化触发的镜头重解算次数（解算实跑）
    int  m_plotAreaFitChangeCount = 0;  // 4e 诊断：其中真正改变镜头参数（distance/fov/near/far）的次数
    // 4f：交互状态
    enum class Drag3D { None, Orbit, Pan };
    Drag3D m_drag3D = Drag3D::None;
    QPointF m_lastPixel;
    int  m_interactionFitCount = 0;     // 4f 诊断：滚轮 dolly + autoFit 触发的 fit 次数

    // ===== 旧实体恢复点（注释保留；归属阶段见各注）=====
    // std::unique_ptr<QChartCamera3D> m_camera3D;        —— 相机归 layer3D（批次 B2 移除）
    // std::unique_ptr<QChartProjection3D> m_projection3D; —— 投影转发 layer3D（批次 B2 移除）
    // QCube m_worldBounds;                                 —— 4c：类型预置为 QCube（原 QChartWorldBox 旧别名不再恢复）
    // class GlHost; std::unique_ptr<GlHost> m_glHost; QOpenGLChartRenderer* m_glRenderer;
    //   RenderBackend m_renderBackend —— 基类 QChartAbstractWidget 已有 plotArea 对齐 GL 宿主（批次 A）
    // QChartScene buildScreenScene()/buildExportScene() 覆写 —— 场景组装由 layer3D.collect 取代
    // orbit/dolly/pan 手势状态（m_orbitDrag/m_pressPos/m_lastPos/...）—— 待交互阶段恢复
    // uvHovered/uvSelected/uvHoveredEnd 信号 + updateHover —— 待拾取阶段恢复
    // 4c（类型预置）：系列/反算接口一律 QCube，恢复时不再欠类型债——
    //   QCube computeSeriesDataBounds3D() const;   // 系列数据包围盒（QCube；多系列用 QCube::united 合并）
    //   void  recomputeDataBounds3D();             // 反算（内部以 QCube 组装/包含判定；无方向状态，4e 承接驱动链）
    //   void  setWorldBounds(const QCube& box);    // 世界包围盒（替代旧 QChartWorldBox 版本）
    // 以上为 3D 系列/交互阶段恢复点（当前不实现、不编译引用）。
    // （批次 B2 补全，按旧 HEAD 头逐项核对）
    // layers3D() 列表访问器 / m_layers3D —— 现为单 layer3D 托管（layer3D() 替代；多 3D 层待 3D 容器扩展阶段）
    // 4c（类型预置）：域盒访问器统一 QCube（现已有 domainBox() 实现）——
    //   QCube dataBounds3D() const;                // 等价 domainBox()（如需旧名便捷可在数据链阶段加转发）
    // 旧 dataBounds3DMin()/dataBounds3DMax()（QVector3D 分解式）不再恢复，避免新类型债。
    // setCamera3D(std::unique_ptr<QChartCamera3D>) 旧签名 —— 相机归 layer（现经 camera3D() 配置层相机）
    // invalidateBackground()/invalidateForeground() 覆写 —— 语义已被 renderer viewDirty/layer dataDirty 取代（同基类）
    // 4c（类型预置）：交互锚定盒统一 QCube ——
    //   QCube m_anchorBox;                          // 待交互阶段（4f）恢复；不再用 min/max 双向量
    // mouse*/wheel/leaveEvent/resizeEvent 覆写 —— 待交互阶段（4f）恢复（基类事件已统一 final 分发）
};

#endif // QCHARTWIDGET3D_H
