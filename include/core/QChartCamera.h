// QChartCamera.h —— 相机基类 + 2D 相机
// 职责（分工红线）：
//   基类 QChartCamera：QObject + viewChanged 信号（2D/3D 视图状态变化共用）。
//   2D 相机 QChartCamera2D：只负责 viewRect 几何：拥有 viewRect + fit 策略
//   + View Cartesian ↔ Pixel 线性映射 + setViewRect/pan/zoom 的视窗几何运算。
// 不知道 Projection、不反算 dataBounds（dataBounds 依赖 projection->computeDataBounds，
//   由 QChartWidget 持有并维护）、不拥有 plotArea（映射/拟合均以参数传入）。
// 2D 相机是未来 3D 相机（position/lookAt/up/FOV → viewProjectionMatrix()）的退化特例。
#ifndef QCHARTCAMERA_H
#define QCHARTCAMERA_H

#include <QObject>
#include <QRectF>
#include <QPointF>

// viewRect 与 plotArea 长宽比匹配策略
enum class ViewRectFitMode {
    Stretch,     // 不调整 viewRect——cartesianToPixel 直接拉伸，图形可能变形
    Expand,      // 扩张 viewRect 较小维度以匹配 plotArea 长宽比，数据始终完整（默认）
    Crop,        // 收缩 viewRect 较大维度以匹配 plotArea 长宽比，可能裁掉部分数据
    Preserve     // 保持 viewRect 不变的同时保证其面积也不变，依此保证数据的完整性和比例不变（新增，修复了Stretch/Expand/Crop的缺陷）
};

/// 相机基类：共同信号（2D/3D 视图状态变化都发 viewChanged）
class QChartCamera : public QObject {
    Q_OBJECT
public:
    explicit QChartCamera(QObject* parent = nullptr);
    ~QChartCamera() override;

signals:
    /// viewRect（或其派生属性 center/zoom）发生变化
    void viewChanged();
};

/// 2D 相机 = 现有 QChartCamera 整体搬入（行为零变化）：
/// viewRect + fit 策略 + center/zoom 属性 + cartesianToPixel/pixelToCartesian + pan/zoom 几何
class QChartCamera2D : public QChartCamera {
    Q_OBJECT
    Q_PROPERTY(QRectF viewRect READ viewRect WRITE setViewRect NOTIFY viewChanged)
    Q_PROPERTY(QPointF center READ center WRITE setCenter NOTIFY viewChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)
public:
    explicit QChartCamera2D(QObject* parent = nullptr);

    // ===== 视窗状态（主状态）=====
    QRectF viewRect() const { return m_viewRect; }
    /// 绝对设置 viewRect（动画等）。设置什么就是什么，不做 fit 修正。
    void setViewRect(const QRectF& r);

    // ===== center / zoom（供 QPropertyAnimation 直接驱动平移/缩放）=====
    /// center == viewRect.center()。
    /// setCenter(c)：平移 viewRect 使中心变为 c；zoom（= viewRect.width()）保持不变。
    QPointF center() const { return m_viewRect.center(); }
    void setCenter(const QPointF& c);

    /// zoom == viewRect.width()（zoom 值约定以视窗宽度度量）。
    /// setZoom(z)：以当前 center 为中心、保持 viewRect 长宽比不变，把宽度设为 z
    /// （高度按当前长宽比同步缩放）。center 保持不变。
    qreal zoom() const { return m_viewRect.width(); }
    void setZoom(qreal z);

    // ===== 视窗几何操作（平移/缩放，仅改 viewRect，不反算 dataBounds）=====
    /// 平移 viewRect（dx/dy 在 View Cartesian 空间）
    void panViewCartesian(qreal dx, qreal dy);
    /// 以 (cx,cy) 为中心缩放 viewRect。factorX/factorY 独立控制两维
    /// （禁交互轴所在维度传 1.0 = 不缩放）。factor<1=放大，>1=缩小
    void zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY);

    // ===== fit 策略 =====
    ViewRectFitMode fitMode() const { return m_fitMode; }
    void setFitMode(ViewRectFitMode mode) { m_fitMode = mode; }
    qreal scale() const { return m_scale; }
    void setScale(qreal ratio) { m_scale = ratio; }

    // ===== fit 几何 =====
    enum class FitStrategy { KeepCenter, 
        KeepLeft, KeepRight, KeepTop, KeepBottom, 
        KeepTopLeft, KeepTopRight, KeepBottomLeft, KeepBottomRight 
    };
    /// 调整 viewRect 使长宽比匹配 plotArea（只做几何，不反算 dataBounds）。
    /// 返回 true 表示 viewRect 实际被修改（调用方据此决定是否重算 dataBounds）。
    bool fitViewRectToPlotArea(const QRectF& plotArea, FitStrategy strategy);

    // ===== 坐标转换（唯一实现，供 Widget / DrawContext / Layer 复用）=====
    /// View Cartesian → Pixel：线性映射 viewRect → plotArea（纯函数）
    static QPointF cartesianToPixel(const QRectF& viewRect, const QRectF& plotArea,
                                    qreal cx, qreal cy);
    /// Pixel → View Cartesian：逆线性映射（纯函数）
    static QPointF pixelToCartesian(const QRectF& viewRect, const QRectF& plotArea,
                                    const QPointF& pixel);

    /// 实例版：使用 m_viewRect（plotArea 由调用方传入）
    QPointF cartesianToPixel(const QRectF& plotArea, qreal cx, qreal cy) const {
        return cartesianToPixel(m_viewRect, plotArea, cx, cy);
    }
    QPointF pixelToCartesian(const QRectF& plotArea, const QPointF& pixel) const {
        return pixelToCartesian(m_viewRect, plotArea, pixel);
    }

private:
    QRectF m_viewRect;
    ViewRectFitMode m_fitMode = ViewRectFitMode::Preserve; // 默认保持 viewRect 不变的同时保证其面积也不变，依此保证数据的完整性和比例不变
    qreal m_scale = 1.0; // 提供放缩因子以提供修改长宽绝对值的功能
};

#endif // QCHARTCAMERA_H
