// QChartAbstractCamera.h
#pragma once

#include <QObject>
#include <QMatrix4x4>
#include <QRectF>
#include <QVector3D>
#include <QPointF>

/// 投影结果：屏幕点 + 深度 + 世界坐标（用于拾取/排序）
struct QChartProjectedPoint {
    QPointF screen;      // 像素坐标（可能在 plotArea 外）
    qreal depth;         // 视图空间深度（用于排序）
    QVector3D cart;     // 输入的世界坐标原样回传
};

struct Ray {
    QVector3D origin;
    QVector3D direction;   // 归一化方向
};

class QChartAbstractCamera : public QObject {
    Q_OBJECT
public:
    explicit QChartAbstractCamera(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~QChartAbstractCamera() = default;

    // ---- 核心矩阵接口（供 GPU 使用） ----
    virtual QMatrix4x4 viewMatrix() const = 0;                     // 世界 → 视图
    virtual QMatrix4x4 projectionMatrix(qreal aspect) const = 0;   // 视图 → 裁剪
    virtual QMatrix4x4 viewProjectionMatrix(qreal aspect) const {  // 世界 → 裁剪（合并）
        return projectionMatrix(aspect) * viewMatrix();
    }

    // ---- CPU 端投影（用于拾取、标签定位） ----
    virtual QChartProjectedPoint project(const QVector3D& cart, const QRectF& plotArea) const = 0;
    /// 屏幕像素 → 世界空间射线（2D 下退化：原点在 z=0 平面，方向为 z轴正方向）
    virtual Ray unproject(const QPointF& pixel, const QRectF& plotArea) const = 0;

    // --- 根据plotArea调整相机 ----
    virtual bool fitToPlotArea(const QRectF& plotArea) = 0;  // 调整相机使视图完全覆盖绘图区
signals:
    void viewChanged();   // 任何视图状态变化均发射
};