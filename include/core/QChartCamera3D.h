// QChartCamera3D.h —— viewCube 主状态相机（R5，用户拍板；D-3D-3 的 Q_PROPERTY 条款随此修订）
// 状态：viewCube（World 空间轴对齐盒 {min,max}，2D viewRect 的 3D 对标物，与相机无关）
//      + orientation（yaw/pitch，绕盒中心）+ fovY（固定用户参数，默认 45°）。
// 派生（相机 = 纯映射器）：lookAt=盒中心；d=radius/tan(fovY/2)（radius=半对角线，保守拟合）；
//      forward/up = R(yaw,pitch)·(0,0,−1)/(0,1,0)；position = lookAt − forward·d；
//      near = max(0.01, d − 1.5·radius)、far = d + 1.5·radius；
//      viewProjectionMatrix = perspective(fovY,aspect,near,far) · lookAt(position,lookAt,up)。
// ⚠ R5：删除 orthographicBox 独立状态——正交模式 viewCube 即投影盒（D-3D-2 直接成立，§2.3）。
// ⚠ R6 硬约束：orbit 只旋转 orientation（viewCube 不动）；平移无鼠标手势（panViewCube 仅 API）。
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

/// 投影结果：屏幕点 + 深度（排序键）；w<=0 时 screen 为 NaN。
/// ★ Phase 3 GL（t42）：world 携带 World 点（A5：VBO 顶点源；GL 路径图元由此打包 16B 顶点）。
struct QChartProjectedPoint {
    QPointF screen;
    qreal depth;
    QVector3D world{0, 0, 0};   // 输入 World 点原样回传（默认零值 → 既有 2 元初始化零改动）
};

class QChartCamera3D : public QChartCamera {
    Q_OBJECT
    Q_PROPERTY(QVector3D viewCubeCenter READ viewCubeCenter WRITE setViewCubeCenter NOTIFY viewChanged)
    Q_PROPERTY(QVector3D viewCubeSize READ viewCubeSize WRITE setViewCubeSize NOTIFY viewChanged)
    Q_PROPERTY(qreal yaw READ yaw WRITE setYaw NOTIFY viewChanged)
    Q_PROPERTY(qreal pitch READ pitch WRITE setPitch NOTIFY viewChanged)
    Q_PROPERTY(qreal fovY READ fovY WRITE setFovY NOTIFY viewChanged)
public:
    explicit QChartCamera3D(QObject* parent = nullptr);

    // ===== 主状态：viewCube（World 轴对齐盒；默认 {0,0,0}-{10,10,10}）=====
    QChartWorldBox viewCube() const { return m_viewCube; }
    void setViewCube(const QChartWorldBox& box);
    QVector3D viewCubeCenter() const { return (m_viewCube.min + m_viewCube.max) * 0.5f; }
    void setViewCubeCenter(const QVector3D& c);   // 平移（pan）
    QVector3D viewCubeSize() const { return m_viewCube.max - m_viewCube.min; }
    void setViewCubeSize(const QVector3D& s);     // 缩放（dolly）

    // ===== 主状态：orientation（绕盒中心；默认 yaw 45°/pitch 30° 3/4 视角）=====
    qreal yaw() const { return m_yaw; }
    void setYaw(qreal deg);                       // 绕世界 up 轴
    qreal pitch() const { return m_pitch; }
    void setPitch(qreal deg);                     // 绕右轴；clamp ±89°（防万向锁）

    // ===== 主状态：镜头参数 =====
    qreal fovY() const { return m_fovY; }
    void setFovY(qreal deg);                      // (1, 179]，默认 45°

    // ===== 派生（只读；setter 移除，R5）=====
    QVector3D position() const;   // = lookAt − forward·d
    QVector3D lookAt() const { return viewCubeCenter(); }   // = viewCube 中心
    QVector3D up() const;         // = R(yaw,pitch)·(0,1,0)
    qreal nearPlane() const;      // = max(0.01, d − 1.5·radius)
    qreal farPlane() const;       // = d + 1.5·radius

    // ===== 投影模式 =====
    enum class ProjectionMode { Perspective, Orthographic };
    ProjectionMode projectionMode() const { return m_projectionMode; }
    void setProjectionMode(ProjectionMode m);

    // ===== 矩阵（纯映射；Phase 3 预留：直接产出合并矩阵，D-3D-10）=====
    QMatrix4x4 viewMatrix() const;                    // QMatrix4x4::lookAt(position, lookAt, up)
    QMatrix4x4 projectionMatrix(qreal aspect) const;  // 透视 perspective(fovY,aspect,near,far) / 正交 ortho(±盒半尺寸)
    QMatrix4x4 viewProjectionMatrix(qreal aspect) const;  // World→Clip 合并

    // ===== 交互几何运算（Widget 事件层调用；操作 viewCube 状态；Camera 不碰事件，D-3D-4）=====
    /// orbit：绕盒中心旋转 orientation（yaw 绕世界 up、pitch 绕右轴；pitch clamp ±89°；
    /// viewCube 不动——R6 硬约束）；viewCube 零尺寸 → no-op（防除零）
    void orbit(qreal deltaYawDeg, qreal deltaPitchDeg);
    /// dolly：缩放 viewCube（factor<1 = 盒缩小 = 内容放大；距离随盒尺寸重派生 → 内容 zoom，2D zoom 同构）
    void dolly(qreal factor);
    /// pan：平移 viewCube（dx/dy World 单位；lookAt/position 跟随；仅 API/动画驱动，R6 无鼠标手势）
    void panViewCube(qreal dxWorld, qreal dyWorld);

    // ===== fit：初始取景框（A3 链终点）=====
    /// 设置 viewCube = 目标盒（中心=盒中心），orientation/fovY 保持
    void setViewCubeToFit(const QChartWorldBox& box);

    // ===== 投影（供 Layer3D 组装闭包 / Renderer）=====
    /// = viewProjectionMatrix(aspect)*world → clipToScreen + viewDepth
    QChartProjectedPoint project(const QVector3D& world, const QRectF& plotArea) const;

private:
    // ===== 派生辅助 =====
    /// 半对角线（radius）；零尺寸 → 0
    qreal radius() const;
    /// 保守拟合距离 d = radius / tan(fovY/2)
    // 精确拟合（备将来实现，当前不做）：
    //   d = max(hx / tan(fovX/2), hy / tan(fovY/2)) − hz
    //   其中 hx/hy/hz = 盒半尺寸（x/y/z），fovX = fovY·aspect；考虑盒最近点（−hz 面）到相机距离，
    //   使盒恰好填满视口且最近角点贴近近平面；保守拟合已保证盒整体在视锥内，工程够用。
    qreal distance() const;
    /// 派生帧：forward/up = R(yaw,pitch)·(0,0,−1)/(0,1,0)；right = normalize(cross(forward, up))
    void frame(QVector3D& outForward, QVector3D& outUp, QVector3D& outRight) const;

    QChartWorldBox m_viewCube{ QVector3D(0, 0, 0), QVector3D(10, 10, 10) };
    qreal m_yaw = 45.0, m_pitch = 30.0;
    qreal m_fovY = 45.0;
    ProjectionMode m_projectionMode = ProjectionMode::Perspective;
};

#endif // QCHARTCAMERA3D_H
