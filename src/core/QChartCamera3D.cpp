// QChartCamera3D.cpp
#include "QChartCamera3D.h"
#include <QDebug>
#include <QtMath>
#include <QVariantAnimation>

QChartCamera3D::QChartCamera3D(QObject* parent)
    : QChartAbstractCamera(parent)
{
    // 注册 QVector3D 插值器（动画需要）
    static const bool registered = []() {
        qRegisterAnimationInterpolator<QVector3D>([](const QVector3D& from,
                                                     const QVector3D& to,
                                                     qreal progress) {
            return QVariant::fromValue(from + (to - from) * progress);
        });
        return true;
    }();
    Q_UNUSED(registered);

    // 根据初始 viewCube 和 fov 计算合理的 distance，并初始化 near/far
    updateCachedRadius();
    m_distance = radius() / qSin(qDegreesToRadians(m_fov) * 0.5);
    resetNearFar();
}

// ---- viewCube 操作 ----
void QChartCamera3D::setViewCube(const QCube& box) {
    if (m_viewCube.min == box.min && m_viewCube.max == box.max)
        return;
    m_viewCube = box;
    updateCachedRadius();
    emit viewChanged();
}

void QChartCamera3D::setViewCubeCenter(const QVector3D& c) {
    const QVector3D delta = c - viewCubeCenter();
    if (delta.lengthSquared() < 1e-12f)
        return;
    m_viewCube.min += delta;
    m_viewCube.max += delta;
    emit viewChanged();
}

void QChartCamera3D::setViewCubeSize(const QVector3D& s) {
    if (s.x() < 0.0f || s.y() < 0.0f || s.z() < 0.0f) {
        qWarning() << "QChartCamera3D::setViewCubeSize: size must be >= 0";
        return;
    }
    const QVector3D center = viewCubeCenter();
    const QVector3D half = s * 0.5f;
    const QCube box{ center - half, center + half };
    if (box.min == m_viewCube.min && box.max == m_viewCube.max)
        return;
    m_viewCube = box;
    updateCachedRadius();
    emit viewChanged();
}

// ---- 姿态 ----
void QChartCamera3D::setYaw(qreal deg) {
    if (qFuzzyCompare(m_yaw, deg))
        return;
    m_yaw = deg;
    emit viewChanged();
}

void QChartCamera3D::setPitch(qreal deg) {
    // 4d：直接超范围调用也钳制到 ±89°（与 orbit 的增量钳制同一语义，避免万向节退化）
    const qreal clamped = qBound<qreal>(-89.0, deg, 89.0);
    if (qFuzzyCompare(m_pitch, clamped))
        return;
    m_pitch = clamped;
    emit viewChanged();
}

void QChartCamera3D::setRoll(qreal deg) {
    if (qFuzzyCompare(m_roll, deg))
        return;
    m_roll = deg;
    emit viewChanged();
}

// ---- 镜头参数 ----
void QChartCamera3D::setFov(qreal deg) {
    if (deg <= 1.0 || deg > 179.0) {
        qWarning() << "QChartCamera3D::setFov: fov must be in (1, 179]";
        return;
    }
    if (qFuzzyCompare(m_fov, deg))
        return;
    m_fov = deg;
    emit viewChanged();
}

void QChartCamera3D::setDistance(qreal d) {
    if (d <= 0.0) {
        qWarning() << "QChartCamera3D::setDistance: distance must be > 0";
        return;
    }
    if (qFuzzyCompare(m_distance, d))
        return;
    m_distance = d;
    emit viewChanged();
}

// ---- 近/远裁面（用户可覆盖） ----
void QChartCamera3D::setNearPlane(qreal val) {
    if (val <= 0.0 || val >= m_far) {
        qWarning() << "QChartCamera3D::setNearPlane: invalid value (must be > 0 and < far)";
        return;
    }
    m_near = val;
    m_nearFarOverride = true;
    emit viewChanged();
}

void QChartCamera3D::setFarPlane(qreal val) {
    if (val <= m_near) {
        qWarning() << "QChartCamera3D::setFarPlane: invalid value (must be > near)";
        return;
    }
    m_far = val;
    m_nearFarOverride = true;
    emit viewChanged();
}

// 4d：内切 near/far 的唯一实现（near = max(0.01, distance − r)、far = distance + r）。
// 只写值、不发射信号、不动 override 标志——resetNearFar()（复位路径）与 fitCameraConfig()（收尾路径）
// 共用本函数，消除此前的双份实现/语义重叠。
bool QChartCamera3D::applyAutoNearFar() {
    const qreal r = radius();
    const qreal newNear = qMax<qreal>(0.01, m_distance - r);
    const qreal newFar = m_distance + r;
    if (qFuzzyCompare(m_near, newNear) && qFuzzyCompare(m_far, newFar))
        return false;
    m_near = newNear;
    m_far = newFar;
    return true;
}

// 用户/构造显式复位路径：内切值 + 清除 override + 无条件通知（既有语义保持）
void QChartCamera3D::resetNearFar() {
    applyAutoNearFar();
    m_nearFarOverride = false;
    emit viewChanged();
}

// ---- 投影模式 ----
void QChartCamera3D::setProjectionMode(ProjectionMode m) {
    if (m_projectionMode == m)
        return;
    m_projectionMode = m;
    emit viewChanged();
}

// ---- 辅助函数 ----
void QChartCamera3D::updateCachedRadius() {
    m_radius = (m_viewCube.max - m_viewCube.min).length() * 0.5;
}

void QChartCamera3D::frame(QVector3D& outForward, QVector3D& outUp, QVector3D& outRight) const {
    const QVector3D worldUp(0, 1, 0);

    // 1. Yaw (绕世界 Y)
    QQuaternion yawQ = QQuaternion::fromAxisAndAngle(worldUp, float(m_yaw));
    QVector3D f = yawQ.rotatedVector(QVector3D(0, 0, -1));
    QVector3D u = yawQ.rotatedVector(worldUp);

    // 2. Pitch (绕右轴)
    QVector3D right = QVector3D::crossProduct(f, u);
    if (right.lengthSquared() < 1e-12f) {
        // 退化（不应发生，因为 pitch 被 clamp）
        outForward = f.normalized();
        outUp = u;
        outRight = QVector3D(1, 0, 0);
        return;
    }
    right.normalize();
    QQuaternion pitchQ = QQuaternion::fromAxisAndAngle(right, float(m_pitch));
    f = pitchQ.rotatedVector(f).normalized();
    u = pitchQ.rotatedVector(u).normalized();

    // 3. Roll (绕前向轴)
    if (!qFuzzyIsNull(m_roll)) {
        QQuaternion rollQ = QQuaternion::fromAxisAndAngle(f, float(m_roll));
        u = rollQ.rotatedVector(u).normalized();
        right = rollQ.rotatedVector(right).normalized();
    }

    outForward = f;
    outUp = u;
    outRight = right;
}

// ---- 派生值 ----
QVector3D QChartCamera3D::position() const {
    QVector3D f, u, r;
    frame(f, u, r);
    return lookAt() - f * m_distance;
}

QVector3D QChartCamera3D::up() const {
    QVector3D f, u, r;
    frame(f, u, r);
    return u;
}

// ---- 矩阵 ----
QMatrix4x4 QChartCamera3D::viewMatrix() const {
    QMatrix4x4 m;
    m.lookAt(position(), lookAt(), up());
    return m;
}

QMatrix4x4 QChartCamera3D::projectionMatrix(qreal aspect) const {
    if (m_projectionMode == ProjectionMode::Perspective) {
        return QChartMath::perspectiveMatrix(m_fov, aspect, m_near, m_far);
    } else {
        const QVector3D half = viewCubeSize() * 0.5f;
        return QChartMath::orthographicMatrix(-half.x(), half.x(),
                                              -half.y(), half.y(),
                                              m_near, m_far);
    }
}

QMatrix4x4 QChartCamera3D::viewProjectionMatrix(qreal aspect) const {
    return projectionMatrix(aspect) * viewMatrix();
}

// ---- 投影 / 反投影 ----
QChartProjectedPoint QChartCamera3D::project(const QVector3D& cart, const QRectF& plotArea) const {
    qreal aspect = (plotArea.height() > 0) ? plotArea.width() / plotArea.height() : 1.0;
    const QMatrix4x4 view = viewMatrix();
    const QVector4D clip = viewProjectionMatrix(aspect) * QVector4D(cart, 1.0f);
    QChartProjectedPoint result;
    result.screen = QChartMath::clipToScreen(clip, plotArea);
    result.depth = QChartMath::viewDepth(view, cart);
    result.cart = cart;
    return result;
}

Ray QChartCamera3D::unproject(const QPointF& pixel, const QRectF& plotArea) const {
    qreal ndcX = (pixel.x() - plotArea.left()) / plotArea.width() * 2.0 - 1.0;
    qreal ndcY = 1.0 - (pixel.y() - plotArea.top()) / plotArea.height() * 2.0;

    const QVector4D nearNDC(ndcX, ndcY, -1.0, 1.0);
    const QVector4D farNDC(ndcX, ndcY,  1.0, 1.0);

    qreal aspect = plotArea.width() / plotArea.height();
    QMatrix4x4 invVP = viewProjectionMatrix(aspect).inverted();

    QVector4D worldNear = invVP * nearNDC;
    QVector4D worldFar  = invVP * farNDC;
    if (qFuzzyIsNull(worldNear.w()) || qFuzzyIsNull(worldFar.w())) {
        return Ray{ QVector3D(), QVector3D(0,0,1) };
    }
    QVector3D origin = QVector3D(worldNear.x() / worldNear.w(),
                                 worldNear.y() / worldNear.w(),
                                 worldNear.z() / worldNear.w());
    QVector3D farPoint = QVector3D(worldFar.x() / worldFar.w(),
                                   worldFar.y() / worldFar.w(),
                                   worldFar.z() / worldFar.w());
    QVector3D direction = (farPoint - origin).normalized();
    return Ray{ origin, direction };
}

// ---- fitCameraConfig（核心求解器） ----
bool QChartCamera3D::fitCameraConfig(const QRectF& plotArea, FitConstraints constraints) {
    // ── 退化输入策略（4d 明确，与头文件 FitConstraint 注释一致）──
    //  · r ≤ 0（零尺寸盒）：无几何可适配 → 不改动任何参数并返回 false
    //  · plotArea 零/负/非有限尺寸：aspect 视为 1.0（垂直基准）后**继续求解**——
    //    FixedFov 只依赖 r 与 fov，不因退化 plotArea 变成 no-op（与 4c fitWorld 内联解算逐位一致）
    qreal r = radius();
    if (r <= 0.0)
        return false;
    const qreal aspect = (plotArea.width() > 0.0 && plotArea.height() > 0.0)
                             ? plotArea.width() / plotArea.height() : 1.0;

    // 判断临界轴（窄边）
    qreal halfFovY = qDegreesToRadians(m_fov) * 0.5;
    qreal halfFovX = qAtan(qTan(halfFovY) * aspect);
    qreal criticalHalfAngle = (aspect >= 1.0) ? halfFovY : halfFovX;

    bool changed = false;

    // 处理约束冲突：按优先级 Fov > Dist > Near > Far（4d 钉死：只解最高优先级项；
    // 其余掩码位不参与求解，仅 FixedNear/FixedFar 影响上面的 near/far 保护判定）
    FitConstraint primary = FitConstraint::None;
    if (constraints & FitConstraint::FixedFov)
        primary = FitConstraint::FixedFov;
    else if (constraints & FitConstraint::FixedDist)
        primary = FitConstraint::FixedDist;
    else if (constraints & FitConstraint::FixedNear)
        primary = FitConstraint::FixedNear;
    else if (constraints & FitConstraint::FixedFar)
        primary = FitConstraint::FixedFar;

    // 如果 primary 为 None，默认使用 FixedFov 行为（与 fitToPlotArea 一致）
    if (primary == FitConstraint::None)
        primary = FitConstraint::FixedFov;

    // 根据主要约束求解
    switch (primary) {
    case FitConstraint::FixedFov: {
        // 固定 fov → 求解 distance（4c fitWorld 手写式的等价形式：halfDiag/sin(fov/2)；
        // 下限 0.05 与迁移前一致，避免极小 fov 时距离发散）
        qreal newDist = r / qMax<qreal>(0.05, qSin(criticalHalfAngle));
        if (!qFuzzyCompare(m_distance, newDist)) {
            m_distance = newDist;
            changed = true;
        }
        break;
    }
    case FitConstraint::FixedDist: {
        // 固定 distance → 求解 fov
        qreal sinVal = r / m_distance;
        if (sinVal > 1.0) {
            // 距离太近，无法完全包含球体，只能尽可能放大 fov
            if (!qFuzzyCompare(m_fov, 179.0)) {
                m_fov = 179.0;
                changed = true;
            }
        } else {
            qreal newHalfAngle = qAsin(sinVal);
            qreal newFov = qRadiansToDegrees(newHalfAngle * 2.0);
            newFov = qBound<qreal>(1.0, newFov, 179.0);
            if (!qFuzzyCompare(m_fov, newFov)) {
                m_fov = newFov;
                changed = true;
            }
        }
        break;
    }
    case FitConstraint::FixedNear: {
        // 固定 near → 由 near = distance - radius 推出 distance
        qreal newDist = m_near + r;
        if (newDist <= 0.0) {
            qWarning() << "fitCameraConfig: FixedNear leads to invalid distance, ignoring";
            break;
        }
        if (!qFuzzyCompare(m_distance, newDist)) {
            m_distance = newDist;
            changed = true;
        }
        // 然后根据新的 distance 重新计算 fov（保持 aspect）
        qreal sinVal = r / m_distance;
        if (sinVal > 1.0) {
            if (!qFuzzyCompare(m_fov, 179.0)) {
                m_fov = 179.0;
                changed = true;
            }
        } else {
            qreal newHalfAngle = qAsin(sinVal);
            qreal newFov = qRadiansToDegrees(newHalfAngle * 2.0);
            newFov = qBound<qreal>(1.0, newFov, 179.0);
            if (!qFuzzyCompare(m_fov, newFov)) {
                m_fov = newFov;
                changed = true;
            }
        }
        break;
    }
    case FitConstraint::FixedFar: {
        // 固定 far → 由 far = distance + radius 推出 distance
        qreal newDist = m_far - r;
        if (newDist <= 0.0) {
            qWarning() << "fitCameraConfig: FixedFar leads to invalid distance, ignoring";
            break;
        }
        if (!qFuzzyCompare(m_distance, newDist)) {
            m_distance = newDist;
            changed = true;
        }
        // 重新计算 fov
        qreal sinVal = r / m_distance;
        if (sinVal > 1.0) {
            if (!qFuzzyCompare(m_fov, 179.0)) {
                m_fov = 179.0;
                changed = true;
            }
        } else {
            qreal newHalfAngle = qAsin(sinVal);
            qreal newFov = qRadiansToDegrees(newHalfAngle * 2.0);
            newFov = qBound<qreal>(1.0, newFov, 179.0);
            if (!qFuzzyCompare(m_fov, newFov)) {
                m_fov = newFov;
                changed = true;
            }
        }
        break;
    }
    default:
        break;
    }

    // ── near/far 收尾（4d：复用 applyAutoNearFar，唯一实现）──
    // 保护策略：用户显式设过 near/far（override）且本次未指定 FixedNear/FixedFar ⇒ 不覆盖、保留用户值；
    // 其余情况由 fit 接管：写内切值（与 resetNearFar 数值完全一致）并清除 override 标志。
    const bool overrideProtected = m_nearFarOverride &&
                                   !(constraints & FitConstraint::FixedNear) &&
                                   !(constraints & FitConstraint::FixedFar);
    if (!overrideProtected) {
        if (applyAutoNearFar())
            changed = true;
        m_nearFarOverride = false;   // fit 显式接管 → clear override
    }

    if (changed)
        emit viewChanged();
    return changed;
}

// ---- 交互操作 ----
void QChartCamera3D::orbit(qreal deltaYawDeg, qreal deltaPitchDeg) {
    if (viewCubeSize().lengthSquared() <= 0.0f)
        return;
    m_yaw += deltaYawDeg;
    m_pitch = qBound<qreal>(-89.0, m_pitch + deltaPitchDeg, 89.0);
    emit viewChanged();
}

void QChartCamera3D::dolly(qreal factor) {
    if (factor <= 0.0)
        return;
    if (viewCubeSize().lengthSquared() <= 0.0f)
        return;
    setViewCubeSize(viewCubeSize() * factor);
}

void QChartCamera3D::panViewCube(qreal dxcart, qreal dycart) {
    if (dxcart == 0.0 && dycart == 0.0)
        return;
    setViewCubeCenter(viewCubeCenter() + QVector3D(dxcart, dycart, 0.0f));
}