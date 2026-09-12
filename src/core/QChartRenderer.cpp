// QChartRenderer.cpp
#include "QChartRenderer.h"
#include "QChartCamera.h"
#include "QChartAbstractProjection.h"
#include <QRectF>
#include <QPainter>
#include <cmath>

void QChartRenderer::render(QChartScene& scene, QPaintDevice* device)
{
    if (!device || !scene.camera || !scene.projection) {
        return;
    }

    onRenderBegin(device);

    // ---- 步骤 2：变换与裁剪（4g：仅在场景快照重建后执行——widget 在层重收集时置 viewDirty；
    //      视图变化必须重收集背景（网格/刻度/标签随可见 numeric 范围更新），
    //      当前为粗粒度全量更新（重收集 + 变换 + 裁剪），背景/前景分级留待后续批次） ----
    if (m_viewDirty) {
        transformNumericToCartesian(scene);
        cullAndResolveLabels(scene);
        m_viewDirty = false;
    }

    // ---- 步骤 3：图元绘制 ----
    drawPrimitives(scene, device, m_visibilityCache);

    // ---- 步骤 4：标签绘制 ----
    drawLabels(scene, device);

    onRenderEnd(device);
}

// 标签锚点像素可见性判定（批次2 收尾：两后端共用单一实现，见 QChartRenderer.h）
bool QChartRenderer::anchorVisibleInPlotArea(const QChartScene& scene, const QVector3D& cart) const
{
    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return false;

    const QRectF& plotArea = scene.plotArea;
    if (plotArea.isEmpty()) return false;   // 零面积画布：显式锚点一律不可见

    const QChartProjectedPoint pp = camera->project(cart, plotArea);
    return std::isfinite(pp.screen.x()) && std::isfinite(pp.screen.y())
           && plotArea.contains(pp.screen);
}

// ============================================================================
// 【未来混合后端预研 · 当前不启用】自由标签 CPU 前置解析旁路（hybrid 预研）
// ----------------------------------------------------------------------------
// 见 include/core/QChartRenderer.h 中 hybridResolveFreeLabelAnchor 的长注释：
// 既定契约 = GL 不渲染自由标签；本组代码是 t29 验证过的"CPU 前置 projection+裁剪"实现
// 的收拢存放处，默认不被任何正常渲染路径调用（属旁路 / 死代码，grep "hybrid" 可定位）。
// ============================================================================
namespace {
/// hybrid 预研旁路内部辅助：组尾图元的数值锚点（Point=numA；顶点型=末顶点；Rect/Ellipse=中心）
QVector3D hybridGroupTailNumeric(const QChartPrimitive& p)
{
    switch (p.type) {
    case QChartPrimitive::Type::Point:
        return p.numA;
    case QChartPrimitive::Type::Line:
        return p.numB;
    case QChartPrimitive::Type::Polygon:
    case QChartPrimitive::Type::Path:
    case QChartPrimitive::Type::TriangleMesh:
    case QChartPrimitive::Type::TriangleFan:
    case QChartPrimitive::Type::TriangleStrip:
        return p.numVerts.isEmpty() ? p.numA : p.numVerts.last();
    case QChartPrimitive::Type::Rect:
    case QChartPrimitive::Type::Ellipse:
        return QVector3D(static_cast<float>(p.numRect.center().x()),
                         static_cast<float>(p.numRect.center().y()), 0.0f);
    }
    return p.numA;
}
} // namespace

bool QChartRenderer::hybridResolveFreeLabelAnchor(const QChartScene& scene,
                                                 QChartTextLabel& label) const
{
    // ★ 旁路函数：默认不被正常渲染路径调用（见头文件注释）。启用属"混合后端"阶段内容。
    if (label.hasExplicitAnchor() || label.refPrimitiveId != -1) return false;
    if (label.sourceId < 0) return false;

    const QChartAbstractProjection* proj = scene.projection;
    if (!proj) return false;

    // 组尾图元：同 sourceId 的最后一个（GL 粗裁=全可见 → "最后一个"即"最后可见"）
    int tail = -1;
    for (int i = 0; i < scene.primitives.size(); ++i)
        if (scene.primitives[i].sourceId == label.sourceId) tail = i;
    if (tail < 0) return false;

    label.cartesianAnchor = proj->toCartesian(hybridGroupTailNumeric(scene.primitives[tail]));
    label.visible = anchorVisibleInPlotArea(scene, label.cartesianAnchor);
    return true;
}

void QChartRenderer::drawLabel(QPainter& painter,
                               const QRectF& plotArea,
                               const QPointF& pixelAnchor,
                               const QString& text,
                               const QColor& color,
                               qreal fontSize,
                               Qt::Alignment alignment)
{
    if (text.isEmpty()) return;

    QFont font = painter.font();
    font.setPointSizeF(fontSize);
    painter.setFont(font);
    painter.setPen(color);

    QFontMetrics fm(font);
    qreal textW = fm.horizontalAdvance(text);
    qreal textH = fm.height();
    const qreal pad = 3.0;
    QSizeF textSize(textW + pad * 2, textH + pad * 2);

    QPointF textPos;
    bool autoAvoid = (alignment == Qt::AlignCenter);

    if (autoAvoid) {
        qreal distLeft   = pixelAnchor.x() - plotArea.left();
        qreal distRight  = plotArea.right() - pixelAnchor.x();
        qreal distTop    = pixelAnchor.y() - plotArea.top();
        qreal distBottom = plotArea.bottom() - pixelAnchor.y();

        enum Dir { Left, Right, Top, Bottom };
        Dir dir = Left;
        qreal maxDist = distLeft;
        if (distRight > maxDist) { maxDist = distRight; dir = Right; }
        if (distTop > maxDist)   { maxDist = distTop;   dir = Top; }
        if (distBottom > maxDist){ maxDist = distBottom; dir = Bottom; }

        switch (dir) {
        case Left:  textPos = pixelAnchor + QPointF(-textSize.width() - pad, -textSize.height()/2); break;
        case Right: textPos = pixelAnchor + QPointF(pad, -textSize.height()/2); break;
        case Top:   textPos = pixelAnchor + QPointF(-textSize.width()/2, -textSize.height() - pad); break;
        case Bottom:textPos = pixelAnchor + QPointF(-textSize.width()/2, pad); break;
        }
    } else {
        if (alignment & Qt::AlignLeft) {
            textPos = pixelAnchor + QPointF(-textSize.width() - pad, -textSize.height()/2);
        } else if (alignment & Qt::AlignRight) {
            textPos = pixelAnchor + QPointF(pad, -textSize.height()/2);
        } else if (alignment & Qt::AlignTop) {
            textPos = pixelAnchor + QPointF(-textSize.width()/2, -textSize.height() - pad);
        } else if (alignment & Qt::AlignBottom) {
            textPos = pixelAnchor + QPointF(-textSize.width()/2, pad);
        } else {
            textPos = pixelAnchor + QPointF(-textSize.width()/2, -textSize.height()/2);
        }
    }

    QRectF textRect(textPos, textSize);
    if (!plotArea.contains(textRect)) {
        if (textRect.left() < plotArea.left())
            textRect.moveLeft(plotArea.left() + pad);
        if (textRect.right() > plotArea.right())
            textRect.moveRight(plotArea.right() - pad);
        if (textRect.top() < plotArea.top())
            textRect.moveTop(plotArea.top() + pad);
        if (textRect.bottom() > plotArea.bottom())
            textRect.moveBottom(plotArea.bottom() - pad);
        if (textRect.width() < textSize.width() * 0.5 ||
            textRect.height() < textSize.height() * 0.5) {
            textRect = QRectF(plotArea.center() - QPointF(textSize.width()/2, textSize.height()/2),
                              textSize);
        }
    }

    painter.drawText(textRect, Qt::AlignCenter, text);
}

void QChartRenderer::drawLabels(QChartScene& scene, QPaintDevice* device)
{
    if (!device) return;

    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    const QRectF& plotArea = scene.plotArea;

    QPainter painter(device);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(plotArea);

    for (const QChartTextLabel& label : scene.labels) {
        if (!label.visible) continue;

        QChartProjectedPoint pp = camera->project(label.cartesianAnchor, plotArea);//Cartesian -> Pixel
        if (!plotArea.contains(pp.screen)) continue;

        drawLabel(painter, plotArea, pp.screen, label.text, label.color,
                  label.fontSize, label.alignment);
    }
}