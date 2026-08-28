// QChartCamera.cpp —— 相机实现（基类 QChartCamera + 2D 相机 QChartCamera2D）
// 2D 只做 viewRect 几何：fit / pan / zoom / center / zoom / View↔Pixel 线性映射。
#include "QChartCamera.h"
#include <QDebug>
#include <QLoggingCategory>
#include <QtMath>

Q_LOGGING_CATEGORY(logCamera, "chart.camera")

// ===== 基类 =====
QChartCamera::QChartCamera(QObject* parent) : QObject(parent) {}
QChartCamera::~QChartCamera() = default;

// ===== 2D 相机 =====
QChartCamera2D::QChartCamera2D(QObject* parent) : QChartCamera(parent) {}

// ===== 视窗状态 =====
void QChartCamera2D::setViewRect(const QRectF& r) {
    m_viewRect = r;
    emit viewChanged();
}

// ===== center / zoom =====
void QChartCamera2D::setCenter(const QPointF& c) {
    if (m_viewRect.center() == c) return;
    m_viewRect.moveCenter(c);
    emit viewChanged();
}

void QChartCamera2D::setZoom(qreal z) {
    if (z <= 0.0) {
        qWarning() << "QChartCamera2D::setZoom: zoom must be > 0, ignoring" << z;
        return;
    }
    if (qFuzzyCompare(m_viewRect.width(), z)) return;
    // 以 center 为中心、两维等比例缩放 → center 不变、长宽比不变、宽度变为 z
    qreal factor = z / m_viewRect.width();
    zoomViewCartesian(m_viewRect.center().x(), m_viewRect.center().y(),
                      factor, factor);
}

// ===== 视窗几何操作 =====
void QChartCamera2D::panViewCartesian(qreal dx, qreal dy) {
    m_viewRect.translate(dx, dy);
    emit viewChanged();
}

void QChartCamera2D::zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY) {
    if (factorX <= 0.0 || factorY <= 0.0) return;
    // 以 (cx, cy) 为中心缩放 viewRect，两维独立（禁交互维度传 1.0）
    qreal newW = m_viewRect.width()  * factorX;
    qreal newH = m_viewRect.height() * factorY;
    qreal newLeft = cx - (cx - m_viewRect.left()) * factorX;
    qreal newTop  = cy - (cy - m_viewRect.top())  * factorY;
    m_viewRect = QRectF(newLeft, newTop, newW, newH);
    emit viewChanged();
}

// ===== fit 几何 =====
bool QChartCamera2D::fitViewRectToPlotArea(const QRectF& plotArea, FitStrategy strategy) {
    // Stretch：不做任何调整
    if (m_fitMode == ViewRectFitMode::Stretch) return false;
    if (plotArea.width() <= 0.0 || plotArea.height() <= 0.0) return false;

    qreal viewW = m_viewRect.width();
    qreal viewH = m_viewRect.height();
    qreal plotW = plotArea.width();
    qreal plotH = plotArea.height();

    // 记录旧 viewRect 的中心和四个角
    QPointF oldCenter = m_viewRect.center();
    qreal oldLeft = m_viewRect.left();
    qreal oldTop = m_viewRect.top();
    qreal oldRight = m_viewRect.right();
    qreal oldBottom = m_viewRect.bottom();

    qreal viewAspect = viewW / viewH;
    qreal plotAspect = plotW / plotH;

    // 如果长宽比已经相同，跳过比例调整（但仍然可能应用额外缩放）
    bool sameAspect = qFuzzyCompare(viewAspect, plotAspect);

    qreal newW = viewW;
    qreal newH = viewH;

    if (!sameAspect) {
        if (viewAspect > plotAspect) {
            // viewRect 更宽（瘦长）
            if (m_fitMode == ViewRectFitMode::Expand) 
                newH = newW / plotAspect;
                // Fit：宽度不变，调整高度使比例匹配 → 高度变小（留白）
            else 
                // Crop/Preserve：高度不变，调整宽度使比例匹配 → 宽度变大（裁剪）
                newW = newH * plotAspect;
        } else {
            // viewRect 更高（胖）
            if (m_fitMode == ViewRectFitMode::Expand) 
                // Fit：高度不变，调整宽度使比例匹配 → 宽度变小（留白）
                newW = newH * plotAspect;
            else 
                // Crop/Preserve：宽度不变，调整高度使比例匹配 → 高度变大（裁剪）
                newH = newW / plotAspect;
        }
    }

    // 应用额外缩放因子（如果用户设置）
    // 复用 m_scale 作为整体缩放系数，>0 有效，1.0 表示不缩放
    if(m_fitMode == ViewRectFitMode::Preserve) {
        // Preserve：保持 viewRect 不变的同时保证其面积也不变，依此保证数据的完整性和比例不变
        qreal oldArea = viewW * viewH;
        qreal newArea = newW * newH;
        if (newArea > 0.0) {
            qreal areaScale = qSqrt(oldArea / newArea);
            newW *= areaScale;
            newH *= areaScale;
        }
    }
    else {
        if (m_scale > 0.0 && !qFuzzyCompare(m_scale, 1.0)) {
            newW *= m_scale;
            newH *= m_scale;
        }
    }
    qCDebug(logCamera) << "fitViewRectToPlotArea: viewRect" << m_viewRect
                  << "plotArea" << plotArea
                  << "newW" << newW << "newH" << newH
                  << "strategy" << static_cast<int>(strategy)
                  << "fitMode" << static_cast<int>(m_fitMode)
                  << "scale" << m_scale;

    // 如果尺寸没有变化，返回
    if (qFuzzyCompare(newW, m_viewRect.width()) && qFuzzyCompare(newH, m_viewRect.height()))
        return false;

    // 根据锚点定位（你现有的 Keep* 枚举）
    qreal left, top;
    switch (strategy) {
    case FitStrategy::KeepCenter:
        left = oldCenter.x() - newW / 2.0;
        top  = oldCenter.y() - newH / 2.0;
        break;
    case FitStrategy::KeepTopLeft:
        left = oldLeft;
        top  = oldTop;
        break;
    case FitStrategy::KeepTopRight:
        left = oldRight - newW;
        top  = oldTop;
        break;
    case FitStrategy::KeepBottomLeft:
        left = oldLeft;
        top  = oldBottom - newH;
        break;
    case FitStrategy::KeepBottomRight:
        left = oldRight - newW;
        top  = oldBottom - newH;
        break;
    case FitStrategy::KeepLeft:
        left = oldLeft;
        top  = oldCenter.y() - newH / 2.0;
        break;
    case FitStrategy::KeepRight:
        left = oldRight - newW;
        top  = oldCenter.y() - newH / 2.0;
        break;
    case FitStrategy::KeepTop:
        left = oldCenter.x() - newW / 2.0;
        top  = oldTop;
        break;
    case FitStrategy::KeepBottom:
        left = oldCenter.x() - newW / 2.0;
        top  = oldBottom - newH;
        break;
    default:
        left = oldCenter.x() - newW / 2.0;
        top  = oldCenter.y() - newH / 2.0;
        break;
    }

    m_viewRect = QRectF(left, top, newW, newH);
    emit viewChanged();
    return true;
}

// ===== 坐标转换（唯一线性映射实现）=====
QPointF QChartCamera2D::cartesianToPixel(const QRectF& viewRect, const QRectF& plotArea,
                                         qreal cx, qreal cy) {
    // View Cartesian → ViewNorm → Pixel（线性）
    qreal nx = (cx - viewRect.left()) / viewRect.width();
    qreal ny = (cy - viewRect.top())  / viewRect.height();
    qreal px = plotArea.left() + nx * plotArea.width();
    qreal py = plotArea.bottom() - ny * plotArea.height();
    return QPointF(px, py);
}

QPointF QChartCamera2D::pixelToCartesian(const QRectF& viewRect, const QRectF& plotArea,
                                         const QPointF& pixel) {
    // Pixel → ViewNorm → View Cartesian（逆线性）
    qreal nx = (pixel.x() - plotArea.left()) / plotArea.width();
    qreal ny = (plotArea.bottom() - pixel.y()) / plotArea.height();
    return QPointF(viewRect.left() + nx * viewRect.width(),
                   viewRect.top()  + ny * viewRect.height());
}
