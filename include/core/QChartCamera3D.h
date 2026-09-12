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
//
// 4d：四约束语义 + 优先级 + 退化策略钉死（实现见 QChartCamera3D::fitCameraConfig）
//   FixedFov  → 固定 fov，解 distance：d = r / max(0.05, sin(criticalHalfAngle))
//               （criticalHalfAngle = aspect ≥ 1 时取垂直半视角 fov/2，否则取水平半视角
//                 atan(tan(fov/2)·aspect)；plotArea 退化时 aspect 视为 1.0 → 等价于 fov/2）
//   FixedDist → 固定 distance，解 fov：fov = 2·asin(r/d)（r/d > 1 即距离过近时取 fov = 179°）
//   FixedNear → 先由 near 定 distance：d = near + r，再按 FixedDist 公式解 fov
//   FixedFar  → 先由 far 定 distance：d = far − r，再按 FixedDist 公式解 fov
//   多约束并存：优先级 Fov > Dist > Near > Far——只解最高优先级项；其余掩码位仅参与
//               near/far 保护判定（见下），不改变被求解的项。
//   退化输入：r ≤ 0（零尺寸盒）→ 不改动任何参数并返回 false；
//             r/d > 1 → fov 钳到 179°；极端 aspect → 水平半视角自然趋近 90°，fov 仍钳 [1,179]；
//             零/负/非有限尺寸 plotArea → aspect 按 1.0 继续求解（不因退化变成 no-op）
//
// 4d 记录（供外部集成参考；t46 审查裁定项）：
//  §1 表现可观测变更（有意改进）：aspect < 1 的**竖高视口**下，FixedFov 的距离按横向临界半视角
//     解算（d = r / max(0.05, sin(atan(tan(fov/2)·aspect)))）——4c 的 fitWorld 内联式忽略 aspect，
//     故竖高视口下 4d 的 d 比 4c 更大（相机后退，完整包容数据）；aspect ≥ 1 与 plotArea 退化
//     （aspect=1）两种情况仍与 4c 逐位一致。
//  §2 退化输入语义变更：零/负尺寸 plotArea 在 4c 走“提前 return false（no-op）”，4d 起改为
//     aspect=1 继续求解（理由：4c fitWorld 内联式从不查 plotArea，只有这样才与 4c 数值等价；
//     仅 r ≤ 0 仍是 no-op 并返回 false）。
//  §3 信号语义：fitCameraConfig 收尾把镜头解算与 near/far 更新合并为**一次** viewChanged，
//     且**无任何变更时不发射**；resetNearFar() 保持无条件发射（用户显式复位路径，语义不变）。
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
    QCube viewCube() const { return m_viewCube; }
    void setViewCube(const QCube& box);
    QVector3D viewCubeCenter() const { return (m_viewCube.min + m_viewCube.max) * 0.5f; }
    void setViewCubeCenter(const QVector3D& c);
    QVector3D viewCubeSize() const { return m_viewCube.max - m_viewCube.min; }
    void setViewCubeSize(const QVector3D& s);

    // ---- 姿态 (Yaw/Pitch/Roll) ----
    qreal yaw() const { return m_yaw; }
    void setYaw(qreal deg);
    qreal pitch() const { return m_pitch; }
    void setPitch(qreal deg);         // 4d：直接传入超范围值同样钳制到 ±89°（与 orbit 一致）
    qreal roll() const { return m_roll; }
    void setRoll(qreal deg);          // 新增

    // ---- 镜头参数 ----
    qreal fov() const { return m_fov; }          // 主视野角（垂直方向基准）
    void setFov(qreal deg);                      // (1°, 179°]
    qreal distance() const { return m_distance; }
    void setDistance(qreal d);                   // 直接设置站位距离

    // ---- 近/远裁面（用户可覆盖） ----
    // 4d 分工（消除双份实现/语义重叠）：
    //   · applyAutoNearFar()（private）：**唯一**的内切计算实现——near = max(0.01, distance − r)、
    //     far = distance + r；只写值，不发射信号、不动 override 标志。
    //   · resetNearFar()（public）：用户/构造显式复位路径 = applyAutoNearFar() + 清除 override 标志
    //     + emit viewChanged（无条件发射，保持既有语义）。
    //   · fitCameraConfig()：fit 收尾复用 applyAutoNearFar()（数值与 resetNearFar 完全一致），
    //     是否覆盖由 override 保护策略决定；信号合并为收尾一次 viewChanged（不产生双份发射）。
    qreal nearPlane() const { return m_near; }
    void setNearPlane(qreal val);                // 用户显式设置 → 打开 override 保护（见下）
    qreal farPlane() const { return m_far; }
    void setFarPlane(qreal val);                 // 用户显式设置 → 打开 override 保护（见下）
    /// 恢复为自动内切值 (distance ± r) 并清除用户 override
    void resetNearFar();

    // ---- 投影模式 ----
    enum class ProjectionMode { Perspective, Orthographic };
    ProjectionMode projectionMode() const { return m_projectionMode; }
    void setProjectionMode(ProjectionMode m);

    // ---- 自动适配开关 ----
    /// 自动适配开关（4d 接线）：
    ///  true（默认）：QChartWidget3D 的三条触发路径（显式 fitWorld / 域盒变化 setDomainBox·clearDomainBox
    ///    / 投影设置 setProjection3D）会按掩码自动解算镜头（distance/fov/near/far）并跟随数据锚点。
    ///  false：上述路径**完全不触碰相机**（viewCube 与镜头参数一并保持），保留用户手调参数；
    ///    开关再次打开后于下一次触发（或显式调用 fitWorld/fitCameraConfig）生效。
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

    /// 4d：用户是否显式覆盖过 near/far（setNearPlane/setFarPlane 置位；resetNearFar/被 fit 接管时清除）。
    /// fit 的保护策略：override==true 且本次未显式指定 FixedNear/FixedFar ⇒ fit 不覆盖 near/far。
    bool isNearFarOverridden() const { return m_nearFarOverride; }

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
    bool applyAutoNearFar();    // 4d：内切 near/far 的唯一实现（只写值；语义见 public 段说明）

    // ---- 核心状态 ----
    QCube m_viewCube{ QVector3D(0,0,0), QVector3D(10,10,10) };
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