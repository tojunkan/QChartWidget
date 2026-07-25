#pragma once
#ifndef QCHARTPROJECTION_H
#define QCHARTPROJECTION_H

#include <QPointF>
#include <QRectF>
#include <QtMath>

// 纯静态工具类，禁止实例化
class QCartesianProjection
{
private:
    QCartesianProjection() = delete; // 彻底禁止构造

public:
    // 归一化坐标 [0,1] -> 像素坐标 (处理 Y 轴翻转)
    static QPointF mapToPixel(qreal normX, qreal normY, const QRectF& plotArea) {
        return QPointF(plotArea.left() + normX * plotArea.width(),
            plotArea.top() + (1.0 - normY) * plotArea.height());
    }

    // 像素坐标 -> 归一化坐标 [0,1] (用于鼠标事件)
    static QPointF mapToNormalized(const QPointF& pixel, const QRectF& plotArea) {
        return QPointF((pixel.x() - plotArea.left()) / plotArea.width(),
            (plotArea.bottom() - pixel.y()) / plotArea.height());
    }
};

class QPolarProjection
{
private:
    QPolarProjection() = delete;

public:
    // 归一化角度 [0,1] 和 归一化半径 [0,1] -> 像素坐标
    // 角度映射：0->正右方，0.25->正上方（修正 Qt Y 轴向下）
    static QPointF mapToPixel(qreal normAngle, qreal normRadius, const QRectF& plotArea) {
        QPointF center = plotArea.center();
        qreal radius = normRadius * qMin(plotArea.width(), plotArea.height()) / 2.0;
        qreal rad = normAngle * 2.0 * M_PI; // 0~1 映射到 0~2PI

        // 关键：Y轴用减法，修正 Qt 坐标系手性
        return QPointF(center.x() + radius * qCos(rad),
            center.y() - radius * qSin(rad));
    }

    // 像素坐标 -> 归一化 (用于滚轮缩放时计算鼠标指向的角度和半径)
    static QPointF mapToNormalized(const QPointF& pixel, const QRectF& plotArea) {
        QPointF center = plotArea.center();
        qreal dx = pixel.x() - center.x();
        qreal dy = center.y() - pixel.y(); // 注意这里反转 dy，恢复数学坐标系

        qreal maxRadius = qMin(plotArea.width(), plotArea.height()) / 2.0;
        qreal radius = qSqrt(dx * dx + dy * dy);
        qreal normRadius = qBound(0.0, radius / maxRadius, 1.0);

        qreal angle = qAtan2(dy, dx); // 范围 -PI ~ PI
        if (angle < 0) angle += 2.0 * M_PI;
        qreal normAngle = angle / (2.0 * M_PI);

        return QPointF(normAngle, normRadius);
    }
};

#endif // QCHARTPROJECTION_H