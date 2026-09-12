// QChartRenderer.h —— 渲染器抽象接口 + 场景快照
// 职责：把「场景快照」画到任意 QPaintDevice（QWidget / QImage / QPixmap / 打印机等）。
// render() 只依赖快照 + 目标 device，不反向依赖 QChartWidget。
// 未来 Phase 3 的 QOpenGLChartRenderer 与此接口并列（Series 吐绘制命令/场景图，GL 消费）。
#ifndef QCHARTRENDERER_H
#define QCHARTRENDERER_H

#include <QColor>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector3D>
#include <QPainterPath>
#include <Qt>
#include "QChartAbstractProjection.h"   // ViewCube（QChartScene::viewCube 按值）
#include "QChartCamera.h"
#include "QChartPrimitive.h"
#include "QChartTextLabel.h"
#include "QChartScene.h"

class QChartAxis;
class QChartLayer;
class QChartAbstractProjection;
class QChartLegend;
class QChartSeries;
class QChartCamera3D;
class QChartLayer3D;
class QPaintDevice;

// ===== 3D 图元（design_3d.md §7.3 / design_3d_axes.md §7.1，D-3D-9 / D-3D-10 ③）=====
// painter's algorithm 命令缓冲雏形：Renderer 3D 路径收集 → 深度排序（depth 降序=远→近）→ 绘制。
// depth 由 ProjectFn3D 全链闭包（Layer3D 组装）返回直接填充（= camera project 的 viewDepth，-viewZ，越大越远）。
// 分层（§7.1，v2 定案）：Grid 与 Series 统一深度排序（Grid 项 depth 减 kGridDepthBias 保证同深度系列优先）；
// ForegroundDecor 恒后画（盒边/spine/刻度点，不与系列/网格比较深度）。

/// 网格深度偏置（§7.2，painter 版 polygon offset）：Grid 项 depth -= kGridDepthBias，
/// 保证同深度处系列优先（z-fighting 时系列赢）。t29 Renderer 应用。
// static constexpr qreal kGridDepthBias = 1e-3;

/// 3D billboard 文本标签（design_3d_axes.md §6.2；t27 Layer3D 收集、t29 Renderer 绘制）

// 场景快照：render 时由 QChartWidget 组装。
// projection 已解析临时投影优先级（tempProjection ? tempProjection : projection）。

class QChartRenderer
{
public:
    virtual ~QChartRenderer() = default;

    
    // 公开接口
    

    /// 主渲染入口：执行完整的 4 步流水线
    /// 步骤 1（收集）由 Widget 在调用前完成，Scene 已填充 Numeric 数据
    /// 步骤 2（变换+裁剪）由基类执行
    /// 步骤 3（图元绘制）和步骤 4（标签绘制）由子类实现
    void render(QChartScene& scene, QPaintDevice* device);

    /// 数据变化（Series 增删改、颜色变化等）→ 下一次 render 重算变换
    // void invalidateData() { m_dataDirty = true; }

    /// 场景快照重建（视图变化：Camera 操作 / resize 经 plotArea / 投影切换；或数据变化）→
    /// 下一次 render 重算变换与裁剪。
    /// 4g：本标记由 widget 在“层重收集（ensureSceneCollected() 为真）”时调用——不再每帧无条件置位；
    /// 规则更正：**视图变化必须重收集背景**（网格脊/刻度/标签由轴持有的可见 numeric 范围生成），
    /// 本阶段为**粗粒度 viewDirty = 全量更新（重收集 + 变换 + 裁剪）**，背景/前景分级留待后续批次。
    void invalidateView() { m_viewDirty = true; }

protected:
    
    // 子类必须实现
    
    // ---- 步骤 2 拆分为两个虚函数 ----
    // 1. 变换：Numeric → Cartesian（CPU 后端做，GPU 后端跳过）
    virtual void transformNumericToCartesian(QChartScene& scene) = 0;

    // 2. 裁剪 + 标签解析（CPU 后端精确裁，GPU 后端可以粗裁或全可见）
    virtual void cullAndResolveLabels(QChartScene& scene) = 0;

    /// 绘制所有可见图元（visibility[i] == true）
    virtual void drawPrimitives(QChartScene& scene,
                                QPaintDevice* device,
                                const QVector<bool>& visibility) = 0;

    /// 绘制所有可见标签（label.visible == true）
    virtual void drawLabels(QChartScene& scene,
                            QPaintDevice* device) = 0;

    static void drawLabel(QPainter& painter,
                          const QRectF& plotArea,
                          const QPointF& pixelAnchor,
                          const QString& text,
                          const QColor& color,
                          qreal fontSize,
                          Qt::Alignment alignment);

    /// 标签锚点像素可见性判定（批次2 收尾：由 QPainterChartRenderer / QOpenGLChartRenderer
    /// 中的两份副本搬入基类，单一实现、两后端共用——禁止再留副本）：
    ///   非退化 plotArea（!isEmpty）+ camera->project 结果有限 + 投影像素落在 plotArea 内。
    /// 语义见 include/core/QChartTextLabel.h 的"标签锚点契约（三级优先级）"tier1 显式锚点。
    bool anchorVisibleInPlotArea(const QChartScene& scene, const QVector3D& cart) const;

    // ========================================================================
    // 【未来混合后端预研 · 当前不启用】hybridResolveFreeLabelAnchor —— 自由标签 CPU 前置解析旁路
    // ------------------------------------------------------------------------
    // 既定契约（批次1 过审）：纯 GPU 后端无能力渲染自由标签（tier3：numericAnchor 全 NaN 且
    // refPrimitiveId == -1）→ GL cull 一律置不可见；二维网格脊标签因此在 GL 后端不显示（已知
    // 缺陷、既定接受差异）。
    // 本函数把 t29 期间验证过的"CPU 侧前置 projection + 裁剪"实现收拢为**独立旁路**（名字带
    // hybrid 前缀，便于全仓 grep 辨认）：
    //   锚点 = 同 sourceId 组尾图元（GL 粗裁语义：最后一个图元）→ projection->toCartesian；
    //   可见性 = anchorVisibleInPlotArea（与 CPU 后端同一判定）。
    // ★ 默认不被任何正常渲染路径调用（QOpenGLChartRenderer::cullAndResolveLabels 对 tier3 明确
    //   置不可见）；仅供未来"混合后端（CPU 前置 projection+裁剪）"阶段启用——启用时改为在该处
    //   调用本函数即可，属该阶段的评审内容。
    // 验证记录（t29，已按用户裁定旁路化）：曾以本实现跑通"GL 可见自由标签"——widget GL
    //   translate 同构探针 inkA(parent)=450 / inkB(local)=450 / maskDiff=0。
    // 返回 true = 已解析（label.cartesianAnchor/visible 已写入）；false = 不适用（非自由标签、
    //   组内无图元、或缺投影）。
    bool hybridResolveFreeLabelAnchor(const QChartScene& scene, QChartTextLabel& label) const;
    // ========================================================================

    // 可选钩子

    virtual void onRenderBegin(QPaintDevice* device) { Q_UNUSED(device); }
    virtual void onRenderEnd(QPaintDevice* device) { Q_UNUSED(device); }

    // 内部状态
    // bool m_dataDirty = true;          // 数据变化 → 需要重算变换 数据变化应该在构建scene的时候判定，这是widget的活
    // 4g：视图/数据变化（= 场景快照重建）→ 需要重算变换与裁剪；无变化则不重算（缓存有效）。
    // 粗粒度全量更新语义见 invalidateView() 注释（背景/前景分级留待后续批次）。
    bool m_viewDirty = true;
    QVector<bool> m_visibilityCache;  // 与 scene.primitives 一一对应
};

#endif // QCHARTRENDERER_H
