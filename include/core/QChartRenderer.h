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
#include <QpainterPath>
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

    /// 视图变化（Camera 变化、窗口 resize 等）→ 下一次 render 重算变换和裁剪
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
    // 可选钩子

    virtual void onRenderBegin(QPaintDevice* device) { Q_UNUSED(device); }
    virtual void onRenderEnd(QPaintDevice* device) { Q_UNUSED(device); }

    // 内部状态
    // bool m_dataDirty = true;          // 数据变化 → 需要重算变换 数据变化应该在构建scene的时候判定，这是widget的活
    bool m_viewDirty = true;          // 视图变化 → 需要重算变换和裁剪
    QVector<bool> m_visibilityCache;  // 与 scene.primitives 一一对应
};

#endif // QCHARTRENDERER_H
