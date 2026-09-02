// QChartCamera.cpp —— 2D 相机实现
#include "QChartCamera.h"
#include <QDebug>
#include <QLoggingCategory>
#include <QtMath>

Q_LOGGING_CATEGORY(logCamera, "chart.camera")

// ===== 构造 =====
QChartCamera::QChartCamera(QObject* parent) : QChartAbstractCamera(parent) {}

// ===== 视窗状态 =====
void QChartCamera::setViewRect(const QRectF& r) {
    if (m_viewRect == r) return;
    m_viewRect = r;
    emit viewChanged();
}

// ===== center / zoom =====
void QChartCamera::setCenter(const QPointF& c) {
    if (m_viewRect.center() == c) return;
    m_viewRect.moveCenter(c);
    emit viewChanged();
}

void QChartCamera::setZoom(qreal z) {
    if (z <= 0.0) {
        qWarning() << "QChartCamera::setZoom: zoom must be > 0, ignoring" << z;
        return;
    }
    if (qFuzzyCompare(m_viewRect.width(), z)) return;
    qreal factor = z / m_viewRect.width();
    zoomViewCartesian(m_viewRect.center().x(), m_viewRect.center().y(), factor, factor);
}

// ===== 交互操作 =====
void QChartCamera::panViewCartesian(qreal dx, qreal dy) {
    m_viewRect.translate(dx, dy);
    emit viewChanged();
}

void QChartCamera::zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY) {
    if (factorX <= 0.0 || factorY <= 0.0) return;
    qreal newW = m_viewRect.width()  * factorX;
    qreal newH = m_viewRect.height() * factorY;
    qreal newLeft = cx - (cx - m_viewRect.left()) * factorX;
    qreal newTop  = cy - (cy - m_viewRect.top())  * factorY;
    m_viewRect = QRectF(newLeft, newTop, newW, newH);
    emit viewChanged();
}

// ===== fit 几何（内部使用 m_fitStrategy） =====
bool QChartCamera::fitToPlotArea(const QRectF& plotArea) {
    if (m_fitMode == ViewRectFitMode::Stretch) return false;
    if (plotArea.width() <= 0.0 || plotArea.height() <= 0.0) return false;

    qreal viewW = m_viewRect.width();
    qreal viewH = m_viewRect.height();
    qreal plotW = plotArea.width();
    qreal plotH = plotArea.height();

    QPointF oldCenter = m_viewRect.center();
    qreal oldLeft = m_viewRect.left();
    qreal oldTop = m_viewRect.top();
    qreal oldRight = m_viewRect.right();
    qreal oldBottom = m_viewRect.bottom();

    qreal viewAspect = viewW / viewH;
    qreal plotAspect = plotW / plotH;
    bool sameAspect = qFuzzyCompare(viewAspect, plotAspect);

    qreal newW = viewW;
    qreal newH = viewH;

    if (!sameAspect) {
        if (viewAspect > plotAspect) { // view 更宽
            if (m_fitMode == ViewRectFitMode::Expand)
                newH = newW / plotAspect;
            else
                newW = newH * plotAspect;
        } else { // view 更高
            if (m_fitMode == ViewRectFitMode::Expand)
                newW = newH * plotAspect;
            else
                newH = newW / plotAspect;
        }
    }

    // 额外缩放
    if (m_fitMode == ViewRectFitMode::Preserve) {
        qreal oldArea = viewW * viewH;
        qreal newArea = newW * newH;
        if (newArea > 0.0) {
            qreal areaScale = qSqrt(oldArea / newArea);
            newW *= areaScale;
            newH *= areaScale;
        }
    } else if (m_scale > 0.0 && !qFuzzyCompare(m_scale, 1.0)) {
        newW *= m_scale;
        newH *= m_scale;
    }

    qCDebug(logCamera) << "fitViewRectToPlotArea: viewRect" << m_viewRect
                       << "plotArea" << plotArea
                       << "newW" << newW << "newH" << newH
                       << "fitMode" << static_cast<int>(m_fitMode)
                       << "strategy" << static_cast<int>(m_fitStrategy)
                       << "scale" << m_scale;

    if (qFuzzyCompare(newW, m_viewRect.width()) && qFuzzyCompare(newH, m_viewRect.height()))
        return false;

    // 根据 m_fitStrategy 锚点定位
    qreal left, top;
    switch (m_fitStrategy) {
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

// ============================================================
// 实现统一矩阵接口（CPU/GPU 双路）
// ============================================================

QMatrix4x4 QChartCamera::viewMatrix() const {
    QMatrix4x4 mat;
    // 平移使 viewRect 中心位于原点
    mat.translate(-m_viewRect.center().x(), -m_viewRect.center().y(), 0.0f);
    // 缩放映射到 [-1, 1]，Y 轴取反使世界 Y 向上与 NDC 一致
    mat.scale(2.0f / m_viewRect.width(), -2.0f / m_viewRect.height(), 1.0f);
    return mat;
}

QMatrix4x4 QChartCamera::projectionMatrix(qreal /*aspect*/) const {
    // 2D 相机视图矩阵已输出标准 NDC，投影矩阵为单位矩阵
    return QMatrix4x4();
}

QChartProjectedPoint QChartCamera::project(const QVector3D& cart, const QRectF& plotArea) const {
    QChartProjectedPoint result;
    result.cart = cart;

    // 线性映射（与 cartesianToPixel 逻辑一致）
    qreal nx = (cart.x() - m_viewRect.left()) / m_viewRect.width();
    qreal ny = (cart.y() - m_viewRect.top())  / m_viewRect.height();
    qreal px = plotArea.left() + nx * plotArea.width();
    qreal py = plotArea.bottom() - ny * plotArea.height();

    result.screen = QPointF(px, py);
    result.depth = 0.0; // 纯 2D 深度为 0
    return result;
}

Ray QChartCamera::unproject(const QPointF& pixel, const QRectF& plotArea) const {
    // 2D 反投影：像素 → 世界坐标（z=0）
    qreal nx = (pixel.x() - plotArea.left()) / plotArea.width();
    qreal ny = (plotArea.bottom() - pixel.y()) / plotArea.height(); // Y 翻转
    qreal wx = m_viewRect.left() + nx * m_viewRect.width();
    qreal wy = m_viewRect.top()  + ny * m_viewRect.height();

    Ray ray;
    ray.origin = QVector3D(wx, wy, 0.0f);
    ray.direction = QVector3D(0.0f, 0.0f, 1.0f); // 垂直于屏幕平面
    return ray;
}