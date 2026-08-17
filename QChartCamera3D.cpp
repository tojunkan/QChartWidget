// QChartCamera3D.cpp —— 3D 相机实现
// 状态 + 矩阵 + 交互几何（orbit/dolly/panTarget）+ fit + 单点投影。
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

// ===== 状态 =====
void QChartCamera3D::setPosition(const QVector3D& p) {
    if (m_position == p) return;
    m_position = p;
    emit viewChanged();
}

void QChartCamera3D::setLookAt(const QVector3D& t) {
    if (m_lookAt == t) return;
    m_lookAt = t;
    emit viewChanged();
}

void QChartCamera3D::setUp(const QVector3D& u) {
    if (m_up == u) return;
    m_up = u;
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

void QChartCamera3D::setNearPlane(qreal n) {
    if (n <= 0.0) {
        qWarning() << "QChartCamera3D::setNearPlane: near must be > 0, ignoring" << n;
        return;
    }
    if (m_nearPlane == n) return;
    m_nearPlane = n;
    emit viewChanged();
}

void QChartCamera3D::setFarPlane(qreal f) {
    if (f <= m_nearPlane) {
        qWarning() << "QChartCamera3D::setFarPlane: far must be > nearPlane, ignoring" << f;
        return;
    }
    if (m_farPlane == f) return;
    m_farPlane = f;
    emit viewChanged();
}

// ===== 投影模式 / 正交盒 =====
void QChartCamera3D::setProjectionMode(ProjectionMode m) {
    if (m_projectionMode == m) return;
    m_projectionMode = m;
    emit viewChanged();
}

void QChartCamera3D::setOrthographicBox(const QRectF& box) {
    if (m_orthographicBox == box) return;
    m_orthographicBox = box;
    emit viewChanged();
}

// ===== 矩阵 =====
QMatrix4x4 QChartCamera3D::viewMatrix() const {
    QMatrix4x4 m;
    m.lookAt(m_position, m_lookAt, m_up);
    return m;
}

QMatrix4x4 QChartCamera3D::projectionMatrix(qreal aspect) const {
    if (m_projectionMode == ProjectionMode::Perspective) {
        return QChartMath::perspectiveMatrix(m_fovY, aspect, m_nearPlane, m_farPlane);
    }
    // Orthographic：正交盒 = m_orthographicBox（box.top()=世界最小 y → ortho bottom，y 翻转约定）
    const QRectF& b = m_orthographicBox;
    return QChartMath::orthographicMatrix(b.left(), b.right(), b.top(), b.bottom(),
                                          m_nearPlane, m_farPlane);
}

QMatrix4x4 QChartCamera3D::viewProjectionMatrix(qreal aspect) const {
    return projectionMatrix(aspect) * viewMatrix();
}

// ===== 交互几何运算 =====
void QChartCamera3D::orbit(qreal deltaYawDeg, qreal deltaPitchDeg) {
    // position == lookAt → no-op（防除零）
    if (m_position == m_lookAt) return;

    const qreal distance = (m_position - m_lookAt).length();
    QVector3D d = (m_position - m_lookAt) / distance;   // 目标→相机（单位方向）

    // yaw：绕 up 轴（世界 up = m_up）旋转
    const QVector3D upAxis = m_up.normalized();
    if (upAxis.lengthSquared() > 0.5f) {
        d = QQuaternion::fromAxisAndAngle(upAxis, float(deltaYawDeg)).rotatedVector(d);
    }

    // pitch：绕 right = normalize(cross(forward, up))，forward = -d
    const QVector3D forward = -d;
    QVector3D right = QVector3D::crossProduct(forward, upAxis);
    if (right.lengthSquared() > 1e-12f) {
        right.normalize();
        d = QQuaternion::fromAxisAndAngle(right, float(deltaPitchDeg)).rotatedVector(d);

        // pitch clamp ±89°（防万向锁：forward ∥ up 时 right 退化）：
        // 仰角 = asin(d·up)，超限则保持水平朝向、把仰角压回 ±89°
        const qreal pitchLimitRad = qDegreesToRadians(89.0);
        const qreal sinPitch = qBound<qreal>(-1.0, QVector3D::dotProduct(d, upAxis), 1.0);
        const qreal pitch = qAsin(sinPitch);
        if (qAbs(pitch) > pitchLimitRad) {
            const qreal clamped = (pitch > 0.0 ? 1.0 : -1.0) * pitchLimitRad;
            QVector3D horiz = d - upAxis * QVector3D::dotProduct(d, upAxis);
            if (horiz.lengthSquared() < 1e-12f)
                horiz = right;              // 恰好落到极点：用右向量（水平）兜底
            else
                horiz.normalize();
            d = horiz * qCos(clamped) + upAxis * qSin(clamped);
        }
    }

    // 保持距离不变，更新位置（lookAt 不动）
    m_position = m_lookAt + d * distance;
    emit viewChanged();
}

void QChartCamera3D::dolly(qreal factor) {
    // factor<=0 忽略；position == lookAt → no-op（防除零）
    if (factor <= 0.0) return;
    if (m_position == m_lookAt) return;

    const qreal newDistance = (m_position - m_lookAt).length() * factor;
    QVector3D dir = (m_position - m_lookAt).normalized();
    m_position = m_lookAt + dir * newDistance;
    emit viewChanged();
}

void QChartCamera3D::panTarget(qreal dxWorld, qreal dyWorld) {
    // 相机平面内平移：right·dx + 相机上轴·dy；lookAt 与 position 同步平移
    QVector3D forward = m_lookAt - m_position;
    if (forward.lengthSquared() < 1e-12f) forward = QVector3D(0, 0, -1); // 退化兜底
    forward.normalize();
    QVector3D right = QVector3D::crossProduct(forward, m_up.normalized());
    if (right.lengthSquared() < 1e-12f) return;
    right.normalize();
    QVector3D upv = QVector3D::crossProduct(right, forward).normalized();

    const QVector3D delta = right * dxWorld + upv * dyWorld;
    m_lookAt += delta;
    m_position += delta;
    emit viewChanged();
}

// ===== fit：初始定位 =====
void QChartCamera3D::fitToBounds(const QChartWorldBox& box, qreal aspect) {
    const QVector3D center = (box.min + box.max) * 0.5f;
    const qreal radius = (box.max - box.min).length() * 0.5f;  // 包围球半径
    if (radius <= 0.0f) return;   // 空盒

    // 保持当前视线方向（旧 lookAt → position）；退化时兜底俯视
    QVector3D dir = m_lookAt - m_position;
    if (dir.lengthSquared() < 1e-12f) dir = QVector3D(0, 0, 1);
    dir.normalize();

    m_lookAt = center;

    qreal distance;
    if (m_projectionMode == ProjectionMode::Perspective) {
        // 以包围球适配视锥：取垂直/水平视场较小者
        const qreal fovYRad = qDegreesToRadians(m_fovY);
        const qreal halfFovX = qAtan(qTan(fovYRad * 0.5) * qMax<qreal>(aspect, 1e-3));
        const qreal minHalfFov = qMin(fovYRad * 0.5, halfFovX);
        distance = radius / qMax<qreal>(qSin(minHalfFov), 1e-6);
    } else {
        distance = radius * 2.0;   // 正交模式距离覆盖半径×2
    }

    m_position = m_lookAt + dir * distance;
    emit viewChanged();
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
    return result;
}
