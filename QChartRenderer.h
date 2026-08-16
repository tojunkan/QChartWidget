// QChartRenderer.h —— 渲染器抽象接口 + 场景快照
// 职责：把「场景快照」画到任意 QPaintDevice（QWidget / QImage / QPixmap / 打印机等）。
// render() 只依赖快照 + 目标 device，不反向依赖 QChartWidget。
// 未来 Phase 3 的 QOpenGLChartRenderer 与此接口并列（Series 吐绘制命令/场景图，GL 消费）。
#ifndef QCHARTRENDERER_H
#define QCHARTRENDERER_H

#include <QColor>
#include <QList>
#include <QRectF>

class QChartAxis;
class QChartLayer;
class QChartProjection;
class QChartLegend;
class QChartSeries;
class QPaintDevice;

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
