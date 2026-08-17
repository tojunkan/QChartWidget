// QChartCamera3D.h —— 3D 相机
// 职责：position/lookAt/up/FOV/near/far 状态 + 透视/正交投影模式 + 视图/投影/合并矩阵
//       + orbit/dolly/panTarget 交互几何运算（Widget 事件层调用；Camera 不碰事件，D-3D-4）
//       + fitToBounds 初始定位 + project 单点投影。
// 继承基类 QChartCamera（QObject + viewChanged 信号，2D/3D 共用）。
// 3D 链路：World ─[viewProjectionMatrix]─► Clip ─[QChartMath::clipToNdc]─► NDC
//          ─[QChartMath::ndcToScreen]─► Pixel（+ viewDepth 深度排序键）
#ifndef QCHARTCAMERA3D_H
#define QCHARTCAMERA3D_H

#include "QChartCamera.h"
#include "QChartProjection3D.h"  // QChartWorldBox（t5 定义于此，禁止重复定义）
#include "QChartMath.h"          // clipToScreen / viewDepth / perspectiveMatrix / orthographicMatrix
#include <QVector3D>
#include <QMatrix4x4>
#include <QRectF>
#include <QPointF>
#include <QQuaternion>

/// 投影结果：屏幕点 + 深度（排序键）；w<=0 时 screen 为 NaN
struct QChartProjectedPoint {
    QPointF screen;
    qreal depth;
};

class QChartCamera3D : public QChartCamera {
    Q_OBJECT
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY viewChanged)
    Q_PROPERTY(QVector3D lookAt READ lookAt WRITE setLookAt NOTIFY viewChanged)
    Q_PROPERTY(qreal fovY READ fovY WRITE setFovY NOTIFY viewChanged)
public:
    explicit QChartCamera3D(QObject* parent = nullptr);

    // ===== 状态（默认：position(0,0,10)、lookAt(0,0,0)、up(0,1,0)、fovY 45°、near 0.1、far 1000）=====
    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D& p);
    QVector3D lookAt() const { return m_lookAt; }
    void setLookAt(const QVector3D& t);
    QVector3D up() const { return m_up; }
    void setUp(const QVector3D& u);
    qreal fovY() const { return m_fovY; }
    void setFovY(qreal deg);        // (1, 179] 有效
    qreal nearPlane() const { return m_nearPlane; }
    void setNearPlane(qreal n);     // > 0
    qreal farPlane() const { return m_farPlane; }
    void setFarPlane(qreal f);      // > nearPlane

    // ===== 投影模式 =====
    enum class ProjectionMode { Perspective, Orthographic };
    ProjectionMode projectionMode() const { return m_projectionMode; }
    void setProjectionMode(ProjectionMode m);

    // ===== 正交盒（Orthographic 模式下投影盒；默认 (0,0,10,10)）=====
    /// 非 Q_PROPERTY（§4.2 只定义 position/lookAt/fovY 三个属性）。
    /// 正交俯视退化一致性（§2.3 硬验收）：把盒设为 2D viewRect，正交俯视投影 ≡ cartesianToPixel。
    QRectF orthographicBox() const { return m_orthographicBox; }
    void setOrthographicBox(const QRectF& box);

    // ===== 矩阵（Phase 3 预留：直接产出合并矩阵，D-3D-10）=====
    QMatrix4x4 viewMatrix() const;                    // World→Camera（QMatrix4x4::lookAt）
    QMatrix4x4 projectionMatrix(qreal aspect) const;  // Camera→Clip（透视/正交按模式）
    QMatrix4x4 viewProjectionMatrix(qreal aspect) const;  // World→Clip 合并

    // ===== 交互几何运算（Widget 事件层调用；Camera 不碰事件，D-3D-4）=====
    /// 绕 lookAt 目标旋转：yaw 绕 up 轴、pitch 绕右轴；pitch clamp 到 ±89°（防万向锁，验收项）
    void orbit(qreal deltaYawDeg, qreal deltaPitchDeg);
    /// 沿视线方向缩放距离：factor<1 靠近；lookAt 不变
    void dolly(qreal factor);
    /// 平移目标（相机平面内 dx/dy，World 单位）：lookAt 与 position 同步平移
    void panTarget(qreal dxWorld, qreal dyWorld);

    // ===== fit：初始定位 =====
    /// 以包围盒中心为 lookAt，按 fov 与 aspect 定距离（正交模式距离覆盖半径×2）
    void fitToBounds(const QChartWorldBox& box, qreal aspect);

    // ===== 投影（供 Layer3D 组装闭包 / Renderer）=====
    /// = viewProjectionMatrix(aspect)*world → clipToScreen + viewDepth
    QChartProjectedPoint project(const QVector3D& world, const QRectF& plotArea) const;

private:
    QVector3D m_position = QVector3D(0, 0, 10);
    QVector3D m_lookAt = QVector3D(0, 0, 0);
    QVector3D m_up = QVector3D(0, 1, 0);
    qreal m_fovY = 45.0;
    qreal m_nearPlane = 0.1;
    qreal m_farPlane = 1000.0;
    ProjectionMode m_projectionMode = ProjectionMode::Perspective;
    QRectF m_orthographicBox = QRectF(0, 0, 10, 10);
};

#endif // QCHARTCAMERA3D_H
