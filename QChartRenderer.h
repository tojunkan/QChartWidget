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
#include <QVector3D>
#include "QChartProjection3D.h"   // QChartWorldBox（QChartScene::worldBounds 按值）

class QChartAxis;
class QChartLayer;
class QChartProjection;
class QChartLegend;
class QChartSeries;
class QChartCamera3D;
class QChartLayer3D;
class QPaintDevice;

// ===== 3D 图元（design_3d.md §7.3，D-3D-9 / D-3D-10 ③；t11 按 designer 修订：去 worldAnchor、加 dataIndex）=====
// painter's algorithm 命令缓冲雏形：Renderer 3D 路径收集 → 深度排序（depth 降序=远→近）→ 绘制。
// depth 由 ProjectFn3D 全链闭包（Layer3D 组装）返回直接填充（= camera project 的 viewDepth，-viewZ，越大越远）。
struct QChartPrimitive {
    enum class Type { Point, LineSegment };
    Type type = Type::Point;
    QPointF a;                // 屏幕坐标：Point 位置 / LineSegment 起点
    QPointF b;                // 屏幕坐标：LineSegment 终点（Point 忽略）
    qreal depth = 0.0;        // 排序键：-viewZ（越大越远；绘制按 depth 降序 = 远→近，近者后画覆盖远者）
    int dataIndex = -1;       // 数据点索引（系列图元=起点/单点索引；网格地板=-1）；hover 用它定位 (u,v)
    qreal markerSize = 4.0;   // Point 标记半径（px）
    QColor color;             // 绘制色（收集时已按系列主题/override 展开）
    qreal penWidth = 1.0;     // 线宽（px）
};

// 场景快照：render 时由 QChartWidget 组装。
// projection 已解析临时投影优先级（tempProjection ? tempProjection : projection）。
struct QChartScene {
    QRectF plotArea;                 // 绘图区像素矩形
    QRectF dataBounds;               // 当前可见 Numeric 范围
    QRectF viewRect;                 // View Cartesian 视窗
    const QChartProjection* projection = nullptr;
    QList<QChartAxis*> axes;
    QList<QChartLayer*> layers;
    QColor backgroundColor;          // 画布底色（invalid = 不填充，即透明）
    QChartLegend* legend = nullptr;  // 图例（Phase 1 overlay）
    QList<QChartSeries*> legendItems; // 图例条目（widget 组装：汇总所有 layer、跳过空 name）
    bool exportMode = false;         // 导出模式：跳过调试黄框等屏显专用绘制

    // ===== 3D 段（design_3d.md §7.2；2D 场景保持默认值，零行为变化）=====
    const QChartCamera3D* camera3D = nullptr;   // 非空 = 3D 场景（2D 场景保持 null）
    QList<QChartLayer3D*> layers3D;             // 3D 图层（camera3D 非空时有效）
    QChartWorldBox worldBounds;                 // 当前可见 World 盒（fit/网格地板用）
    bool is3D() const { return camera3D != nullptr; }
};

class QChartRenderer {
public:
    virtual ~QChartRenderer();

    /// 将场景渲染到目标设备。不得假设 device 是 QWidget。
    virtual void render(const QChartScene& scene, QPaintDevice* device) = 0;

    /// 直接绘制到 device，不读写内部 QPixmap 缓存。
    /// 用途：导出（PNG/SVG/PDF）——避免矢量 device 被栅格化，且避免污染屏显缓存。
    virtual void renderUncached(const QChartScene& scene, QPaintDevice* device) = 0;

    /// 置脏：下次 render 重建对应缓存（无缓存后端时可为空操作）
    virtual void invalidateBackground() = 0;
    virtual void invalidateForeground() = 0;

    virtual void setCachingEnabled(bool enabled) = 0;
    virtual bool isCachingEnabled() const = 0;
};

#endif // QCHARTRENDERER_H
