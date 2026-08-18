// QChartCamera3D.cpp —— viewCube 主状态相机实现（R5）
// 主状态：viewCube + orientation(yaw/pitch) + fovY；position/lookAt/up/near/far 全部派生只读。
// 矩阵纯映射；交互（orbit/dolly/panViewCube）操作 viewCube/orientation 状态。
// 投影纯函数复用 QChartMath.h；viewChanged 信号来自基类 QChartCamera。
#include "QChartCamera3D.h"
#include <QDebug>
#include <QVariantAnimation>
#include <QtMath>

QChartCamera3D::QChartCamera3D(QObject* parent) : QChartCamera(parent) {
    // D-3D-3：QVector3D 若 Qt 未内建 QVariantAnimation 插值器则补齐（线性插值）。
    // 在首个相机构造时注册（库初始化处的实际落点）；重复注册幂等（覆盖为同一线性实现）。
    static const bool registered = []() {
        qRegisterAnimationInterpolator<QVector3D>([](const QVector3D& from,
                                                     const QVector3D& to,
                                                     qreal progress) {
            return QVariant::fromValue(from + (to - from) * progress);
        });
        return true;
    }();
    Q_UNUSED(registered);
}

// ===== 主状态：viewCube =====
void QChartCamera3D::setViewCube(const QChartWorldBox& box) {
    if (m_viewCube.min == box.min && m_viewCube.max == box.max) return;
    m_viewCube = box;
    emit viewChanged();
}

void QChartCamera3D::setViewCubeCenter(const QVector3D& c) {
    const QVector3D delta = c - viewCubeCenter();
    if (delta.lengthSquared() < 1e-12f) return;
    m_viewCube.min += delta;
    m_viewCube.max += delta;
    emit viewChanged();
}

void QChartCamera3D::setViewCubeSize(const QVector3D& s) {
    if (s.x() < 0.0f || s.y() < 0.0f || s.z() < 0.0f) {
        qWarning() << "QChartCamera3D::setViewCubeSize: size 分量必须 >= 0，忽略" << s;
        return;
    }
    const QVector3D center = viewCubeCenter();
    const QVector3D half = s * 0.5f;
    const QChartWorldBox box{ center - half, center + half };
    if (box.min == m_viewCube.min && box.max == m_viewCube.max) return;
    m_viewCube = box;
    emit viewChanged();
}

// ===== 主状态：orientation / 镜头参数 =====
void QChartCamera3D::setYaw(qreal deg) {
    if (m_yaw == deg) return;
    m_yaw = deg;
    emit viewChanged();
}

void QChartCamera3D::setPitch(qreal deg) {
    const qreal clamped = qBound<qreal>(-89.0, deg, 89.0);   // clamp ±89°（防万向锁）
    if (m_pitch == clamped) return;
    m_pitch = clamped;
    emit viewChanged();
}

void QChartCamera3D::setFovY(qreal deg) {
    if (deg <= 1.0 || deg > 179.0) {
        qWarning() << "QChartCamera3D::setFovY: fov must be in (1, 179], ignoring" << deg;
        return;
    }
    if (m_fovY == deg) return;
    m_fovY = deg;
    emit viewChanged();
}

// ===== 投影模式 =====
void QChartCamera3D::setProjectionMode(ProjectionMode m) {
    if (m_projectionMode == m) return;
    m_projectionMode = m;
    emit viewChanged();
}

// ===== 派生辅助 =====
qreal QChartCamera3D::radius() const {
    return viewCubeSize().length() * 0.5f;   // 半对角线
}

qreal QChartCamera3D::distance() const {
    const qreal halfFov = qDegreesToRadians(m_fovY) * 0.5;
    return radius() / qMax<qreal>(qTan(halfFov), 1e-6);   // 保守拟合（定案），见头注释精确拟合备将来
}

void QChartCamera3D::frame(QVector3D& outForward, QVector3D& outUp, QVector3D& outRight) const {
    const QVector3D worldUp(0, 1, 0);
    // yaw 绕世界 up：R(yaw)·(0,0,−1)/(0,1,0)
    QVector3D f = QQuaternion::fromAxisAndAngle(worldUp, float(m_yaw))
                      .rotatedVector(QVector3D(0, 0, -1));
    const QVector3D u0 = QQuaternion::fromAxisAndAngle(worldUp, float(m_yaw))
                             .rotatedVector(worldUp);   // 绕 up 旋转不改变 up → = worldUp
    // pitch 绕右轴：right = normalize(cross(forward_yawed, up))
    QVector3D right = QVector3D::crossProduct(f, u0);
    if (right.lengthSquared() < 1e-12f) {   // 退化兜底（pitch clamp 已防，理论不可达）
        outForward = f.normalized();
        outUp = u0;
        outRight = QVector3D(1, 0, 0);
        return;
    }
    right.normalize();
    f = QQuaternion::fromAxisAndAngle(right, float(m_pitch)).rotatedVector(f).normalized();
    const QVector3D u = QQuaternion::fromAxisAndAngle(right, float(m_pitch))
                            .rotatedVector(u0).normalized();
    outForward = f;
    outUp = u;
    outRight = right;
}

// ===== 派生（只读）=====
QVector3D QChartCamera3D::position() const {
    QVector3D forward, upv, right;
    frame(forward, upv, right);
    return lookAt() - forward * distance();
}

QVector3D QChartCamera3D::up() const {
    QVector3D forward, upv, right;
    frame(forward, upv, right);
    return upv;
}

qreal QChartCamera3D::nearPlane() const {
    return qMax<qreal>(0.01, distance() - 1.5 * radius());
}

qreal QChartCamera3D::farPlane() const {
    return distance() + 1.5 * radius();
}

// ===== 矩阵 =====
QMatrix4x4 QChartCamera3D::viewMatrix() const {
    QMatrix4x4 m;
    m.lookAt(position(), lookAt(), up());
    return m;
}

QMatrix4x4 QChartCamera3D::projectionMatrix(qreal aspect) const {
    if (m_projectionMode == ProjectionMode::Perspective) {
        return QChartMath::perspectiveMatrix(m_fovY, aspect, nearPlane(), farPlane());
    }
    // Orthographic：viewCube 即投影盒——视图空间内以盒中心为原点、半尺寸为盒（§2.3 R5 论证）
    const QVector3D half = viewCubeSize() * 0.5f;
    return QChartMath::orthographicMatrix(-half.x(), half.x(), -half.y(), half.y(),
                                          nearPlane(), farPlane());
}

QMatrix4x4 QChartCamera3D::viewProjectionMatrix(qreal aspect) const {
    return projectionMatrix(aspect) * viewMatrix();
}

// ===== 交互几何运算（操作 viewCube/orientation；R6）=====
void QChartCamera3D::orbit(qreal deltaYawDeg, qreal deltaPitchDeg) {
    if (viewCubeSize().lengthSquared() <= 0.0f) return;   // 零尺寸 no-op（防除零）
    m_yaw += deltaYawDeg;
    m_pitch = qBound<qreal>(-89.0, m_pitch + deltaPitchDeg, 89.0);   // clamp ±89°
    emit viewChanged();   // viewCube 不动（R6 硬约束：只转 orientation）
}

void QChartCamera3D::dolly(qreal factor) {
    if (factor <= 0.0) return;
    if (viewCubeSize().lengthSquared() <= 0.0f) return;   // 零尺寸 no-op
    setViewCubeSize(viewCubeSize() * factor);             // 绕中心缩放（内容 zoom，2D 同构）
}

void QChartCamera3D::panViewCube(qreal dxWorld, qreal dyWorld) {
    if (dxWorld == 0.0 && dyWorld == 0.0) return;
    setViewCubeCenter(viewCubeCenter() + QVector3D(dxWorld, dyWorld, 0.0f));   // World x/y 平移
}

// ===== fit：初始取景框（A3 链终点）=====
void QChartCamera3D::setViewCubeToFit(const QChartWorldBox& box) {
    setViewCube(box);   // orientation/fovY 保持
}

// ===== 投影 =====
QChartProjectedPoint QChartCamera3D::project(const QVector3D& world, const QRectF& plotArea) const {
    qreal aspect = 1.0;
    if (plotArea.height() > 0.0) aspect = plotArea.width() / plotArea.height();

    const QMatrix4x4 view = viewMatrix();
    const QVector4D clip = viewProjectionMatrix(aspect) * QVector4D(world, 1.0f);
    QChartProjectedPoint result;
    result.screen = QChartMath::clipToScreen(clip, plotArea);
    result.depth = QChartMath::viewDepth(view, world);
    result.world = world;   // GL 顶点源（t42，§3.2）
    return result;
}
