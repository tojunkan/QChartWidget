// QChartCamera3D.h
#ifndef QCHARTCAMERA3D_H
#define QCHARTCAMERA3D_H

#include "QChartAbstractCamera.h"
#include "QChartProjection3D.h"
#include "QChartMath.h"
#include <QVector3D>
#include <QMatrix4x4>
#include <QRectF>
#include <QPointF>
#include <QQuaternion>
#include <QFlags>

// 约束掩码：指定哪些参数在 fit 中被视为固定（不可变）
enum class FitConstraint {
    None      = 0,
    FixedFov  = 1 << 0,   // 固定 fov，调整 distance
    FixedDist = 1 << 1,   // 固定 distance，调整 fov
    FixedNear = 1 << 2,   // 固定 near，调整 far
    FixedFar  = 1 << 3    // 固定 far，调整 near
};
Q_DECLARE_FLAGS(FitConstraints, FitConstraint)

class QChartCamera3D : public QChartAbstractCamera {
    Q_OBJECT
    Q_PROPERTY(QVector3D viewCubeCenter READ viewCubeCenter WRITE setViewCubeCenter NOTIFY viewChanged)
    Q_PROPERTY(QVector3D viewCubeSize READ viewCubeSize WRITE setViewCubeSize NOTIFY viewChanged)
    Q_PROPERTY(qreal yaw READ yaw WRITE setYaw NOTIFY viewChanged)
    Q_PROPERTY(qreal pitch READ pitch WRITE setPitch NOTIFY viewChanged)
    Q_PROPERTY(qreal roll READ roll WRITE setRoll NOTIFY viewChanged)
    Q_PROPERTY(qreal fov READ fov WRITE setFov NOTIFY viewChanged)
    Q_PROPERTY(qreal distance READ distance WRITE setDistance NOTIFY viewChanged)
    Q_PROPERTY(qreal nearPlane READ nearPlane WRITE setNearPlane NOTIFY viewChanged)
    Q_PROPERTY(qreal farPlane READ farPlane WRITE setFarPlane NOTIFY viewChanged)

public:
    explicit QChartCamera3D(QObject* parent = nullptr);

    // ---- 数据锚点 (ViewCube) ----
    ViewCube viewCube() const { return m_viewCube; }
    void setViewCube(const ViewCube& box);
    QVector3D viewCubeCenter() const { return (m_viewCube.min + m_viewCube.max) * 0.5f; }
    void setViewCubeCenter(const QVector3D& c);
    QVector3D viewCubeSize() const { return m_viewCube.max - m_viewCube.min; }
    void setViewCubeSize(const QVector3D& s);

    // ---- 姿态 (Yaw/Pitch/Roll) ----
    qreal yaw() const { return m_yaw; }
    void setYaw(qreal deg);
    qreal pitch() const { return m_pitch; }
    void setPitch(qreal deg);
    qreal roll() const { return m_roll; }
    void setRoll(qreal deg);          // 新增

    // ---- 镜头参数 ----
    qreal fov() const { return m_fov; }          // 主视野角（垂直方向基准）
    void setFov(qreal deg);                      // (1°, 179°]
    qreal distance() const { return m_distance; }
    void setDistance(qreal d);                   // 直接设置站位距离

    // ---- 近/远裁面（用户可覆盖） ----
    qreal nearPlane() const { return m_near; }
    void setNearPlane(qreal val);
    qreal farPlane() const { return m_far; }
    void setFarPlane(qreal val);
    void resetNearFar();                         // 恢复为自动内切值 (distance ± radius)

    // ---- 投影模式 ----
    enum class ProjectionMode { Perspective, Orthographic };
    ProjectionMode projectionMode() const { return m_projectionMode; }
    void setProjectionMode(ProjectionMode m);

    // ---- 自动适配开关 ----
    bool autoFit() const { return m_autoFit; }
    void setAutoFit(bool on) { m_autoFit = on; }

    // ---- 基类接口实现 ----
    QMatrix4x4 viewMatrix() const override;
    QMatrix4x4 projectionMatrix(qreal aspect) const override;
    QMatrix4x4 viewProjectionMatrix(qreal aspect) const override;
    QChartProjectedPoint project(const QVector3D& cart, const QRectF& plotArea) const override;
    Ray unproject(const QPointF& pixel, const QRectF& plotArea) const override;

    // ---- Fit 到绘图区（支持约束掩码） ----
    bool fitToPlotArea(const QRectF& plotArea) override { return fitCameraConfig(plotArea, FitConstraint::FixedFov); }

    bool fitCameraConfig(const QRectF& plotArea, FitConstraints constraints = FitConstraint::None);

    // ---- 交互快捷操作 ----
    void orbit(qreal deltaYawDeg, qreal deltaPitchDeg);   // 旋转视角
    void dolly(qreal factor);                             // 缩放（保持 viewCube 中心）
    void panViewCube(qreal dxcart, qreal dycart);         // 平移

    // ---- 辅助：获取当前位置/up/forward等（派生） ----
    QVector3D position() const;   // = lookAt - forward * distance
    QVector3D lookAt() const { return viewCubeCenter(); }
    QVector3D up() const;         // 由姿态派生

private:
    // ---- 内部辅助 ----
    qreal radius() const { return m_radius; }          // 缓存值
    void updateCachedRadius();                         // viewCube 变化时调用
    void frame(QVector3D& outForward, QVector3D& outUp, QVector3D& outRight) const;
    bool isNearFarOverridden() const { return m_nearFarOverride; }

    // ---- 核心状态 ----
    ViewCube m_viewCube{ QVector3D(0,0,0), QVector3D(10,10,10) };
    qreal m_yaw = 45.0;
    qreal m_pitch = 30.0;
    qreal m_roll = 0.0;
    qreal m_distance = 24.14;     // 默认 45° fov 下的保守距离（将被构造函数重算）
    qreal m_fov = 45.0;           // 主视野角（垂直方向基准）
    qreal m_near = 0.01;
    qreal m_far = 100.0;
    bool m_nearFarOverride = false;
    ProjectionMode m_projectionMode = ProjectionMode::Perspective;

    // ---- 缓存 ----
    qreal m_radius = 5.0;         // viewCube 半对角线

    // ---- 策略 ----
    bool m_autoFit = true;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FitConstraints)

#endif // QCHARTCAMERA3D_H