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
    // Stretch 模式：不调整 viewRect，直接拉伸
    if (m_fitMode == ViewRectFitMode::Stretch) return false;

    if (plotArea.width() <= 0.0 || plotArea.height() <= 0.0) return false;

    qreal plotAspect = plotArea.width() / plotArea.height();
    qreal viewAspect = m_viewRect.width() / m_viewRect.height();
    // 目标长宽比：Fixed 模式用用户指定的，否则用 plotArea 的
    qreal targetAspect = (m_fitMode == ViewRectFitMode::Fixed)
        ? m_fixedAspectRatio
        : viewAspect;

    // 长宽比已经匹配（1% 容差）→ 跳过
    if (qAbs(targetAspect - viewAspect) < 0.01 * targetAspect) return false;

    qCDebug(logCamera) << "fitViewRectToPlotArea: before" << m_viewRect
                       << "mode=" << (int)m_fitMode
                       << "strategy=" << (int)strategy
                       << "targetAspect=" << targetAspect << "viewAspect=" << viewAspect;

    bool expand; // true=扩张，false=收缩
    if (m_fitMode == ViewRectFitMode::Crop) {
        // Crop：收缩较大维度，裁掉超出部分
        expand = false;
    } else {
        // Fit / Fixed：扩张较小维度，数据完整
        expand = true;
    }

    if (expand) {
        switch (strategy) {
        case FitStrategy::KeepWidth:
            // 用户设了 dim0 → 锁宽度，只调高度
            {
                qreal newH = m_viewRect.width() / targetAspect;
                qreal d = (newH - m_viewRect.height()) / 2.0;
                m_viewRect.adjust(0.0, -d, 0.0, d);
            }
            break;
        case FitStrategy::KeepHeight:
            // 用户设了 dim1 → 锁高度，只调宽度
            {
                qreal newW = m_viewRect.height() * targetAspect;
                qreal d = (newW - m_viewRect.width()) / 2.0;
                m_viewRect.adjust(-d, 0.0, d, 0.0);
            }
            break;
        case FitStrategy::KeepCenter:
            // 初始化/布局变化 → 双向均等扩张
            if (targetAspect > viewAspect) {
                qreal newW = m_viewRect.height() * targetAspect;
                qreal d = (newW - m_viewRect.width()) / 2.0;
                m_viewRect.adjust(-d, 0.0, d, 0.0);
            } else {
                qreal newH = m_viewRect.width() / targetAspect;
                qreal d = (newH - m_viewRect.height()) / 2.0;
                m_viewRect.adjust(0.0, -d, 0.0, d);
            }
            break;
        }
    } else {
        // Crop：收缩较大维度
        if (viewAspect > targetAspect) {
            // 太宽 → 收缩宽度
            qreal newW = m_viewRect.height() * targetAspect;
            qreal d = (m_viewRect.width() - newW) / 2.0;
            m_viewRect.adjust(d, 0.0, -d, 0.0);
        } else {
            // 太高 → 收缩高度
            qreal newH = m_viewRect.width() / targetAspect;
            qreal d = (m_viewRect.height() - newH) / 2.0;
            m_viewRect.adjust(0.0, d, 0.0, -d);
        }
    }

    qCDebug(logCamera) << "fitViewRectToPlotArea: after" << m_viewRect;

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
