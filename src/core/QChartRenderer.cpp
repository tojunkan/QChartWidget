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

    // ---- 步骤 2：变换与裁剪（仅在视图脏或数据脏时执行） ----
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