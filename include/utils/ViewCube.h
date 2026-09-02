#pragma once

#include <QVector3D>
#include <QDebug>
#include <limits>
#include <cmath>

struct ViewCube
{
public:
    // 默认构造：生成一个无效立方体（min = +inf, max = -inf）
    ViewCube()
        : min( std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity() ),
          max(-std::numeric_limits<float>::infinity(),
              -std::numeric_limits<float>::infinity(),
              -std::numeric_limits<float>::infinity())
    {}

    // 显式复制构造函数（也可用 = default，但这里显式写出来）
    ViewCube(const ViewCube &other)
        : min(other.min), max(other.max)
    {}

    // 从两个角点构造（自动归一化）
    ViewCube(const QVector3D &p1, const QVector3D &p2)
    {
        min.setX(std::min(p1.x(), p2.x()));
        min.setY(std::min(p1.y(), p2.y()));
        min.setZ(std::min(p1.z(), p2.z()));
        max.setX(std::max(p1.x(), p2.x()));
        max.setY(std::max(p1.y(), p2.y()));
        max.setZ(std::max(p1.z(), p2.z()));
    }

    // 从中心和尺寸构造（尺寸取绝对值）
    ViewCube(const QVector3D &center, const QVector3D &size)
    {
        QVector3D half = QVector3D(std::abs(size.x()), std::abs(size.y()), std::abs(size.z())) * 0.5f;
        min = center - half;
        max = center + half;
    }

    // 有效性：所有轴方向 min <= max
    bool isValid() const {
        return min.x() <= max.x() && min.y() <= max.y() && min.z() <= max.z();
    }

    // 是否为空（体积为零）
    bool isNull() const {
        return !isValid() ||
               qFuzzyIsNull(width()) ||
               qFuzzyIsNull(height()) ||
               qFuzzyIsNull(depth());
    }

    // 中心
    QVector3D center() const {
        return (min + max) * 0.5f;
    }

    // 尺寸（宽、高、深）
    QVector3D size() const {
        return max - min;
    }

    qreal width()  const { return max.x() - min.x(); }
    qreal height() const { return max.y() - min.y(); }
    qreal depth()  const { return max.z() - min.z(); }

    // 返回归一化后的副本（确保 min <= max）
    ViewCube normalized() const {
        if (isValid())
            return *this;
        ViewCube c;
        c.min.setX(std::min(min.x(), max.x()));
        c.min.setY(std::min(min.y(), max.y()));
        c.min.setZ(std::min(min.z(), max.z()));
        c.max.setX(std::max(min.x(), max.x()));
        c.max.setY(std::max(min.y(), max.y()));
        c.max.setZ(std::max(min.z(), max.z()));
        return c;
    }

    // 平移（原地）
    void translate(const QVector3D &offset) {
        min += offset;
        max += offset;
    }

    // 平移（返回新对象）
    ViewCube translated(const QVector3D &offset) const {
        ViewCube c = *this;
        c.translate(offset);
        return c;
    }

    // 缩放（相对于中心）
    void scale(qreal factor) {
        QVector3D c = center();
        QVector3D half = (max - min) * 0.5f * factor;
        min = c - half;
        max = c + half;
    }

    // 移动中心（保持尺寸不变）
    void moveCenter(const QVector3D &newCenter) {
        QVector3D half = (max - min) * 0.5f;
        min = newCenter - half;
        max = newCenter + half;
    }

    // 类似 QRect::adjust：分别调整左、上、前、右、下、后边界
    void adjust(qreal left, qreal top, qreal front, qreal right, qreal bottom, qreal back) {
        min.setX(min.x() + left);
        min.setY(min.y() + top);
        min.setZ(min.z() + front);
        max.setX(max.x() + right);
        max.setY(max.y() + bottom);
        max.setZ(max.z() + back);
    }

    // 点是否在立方体内（包含边界）
    bool contains(const QVector3D &point) const {
        return point.x() >= min.x() && point.x() <= max.x() &&
               point.y() >= min.y() && point.y() <= max.y() &&
               point.z() >= min.z() && point.z() <= max.z();
    }

    // 包含另一个立方体
    bool contains(const ViewCube &other) const {
        return contains(other.min) && contains(other.max);
    }

    // 是否相交（包括边界接触也算相交）
    bool intersects(const ViewCube &other) const {
        return !(min.x() > other.max.x() || max.x() < other.min.x() ||
                 min.y() > other.max.y() || max.y() < other.min.y() ||
                 min.z() > other.max.z() || max.z() < other.min.z());
    }

    // 交集（若不相交则返回无效立方体）
    ViewCube intersected(const ViewCube &other) const {
        if (!intersects(other))
            return ViewCube(); // 无效
        ViewCube result;
        result.min.setX(std::max(min.x(), other.min.x()));
        result.min.setY(std::max(min.y(), other.min.y()));
        result.min.setZ(std::max(min.z(), other.min.z()));
        result.max.setX(std::min(max.x(), other.max.x()));
        result.max.setY(std::min(max.y(), other.max.y()));
        result.max.setZ(std::min(max.z(), other.max.z()));
        return result;
    }

    // 并集（包含两者的最小包围盒）
    ViewCube united(const ViewCube &other) const {
        ViewCube result;
        result.min.setX(std::min(min.x(), other.min.x()));
        result.min.setY(std::min(min.y(), other.min.y()));
        result.min.setZ(std::min(min.z(), other.min.z()));
        result.max.setX(std::max(max.x(), other.max.x()));
        result.max.setY(std::max(max.y(), other.max.y()));
        result.max.setZ(std::max(max.z(), other.max.z()));
        return result;
    }

    // 赋值运算符（默认即可，但为了完整性显式写出）
    ViewCube &operator=(const ViewCube &other) {
        if (this != &other) {
            min = other.min;
            max = other.max;
        }
        return *this;
    }

    // 比较运算符
    bool operator==(const ViewCube &other) const {
        return min == other.min && max == other.max;
    }

    bool operator!=(const ViewCube &other) const {
        return !(*this == other);
    }

    // 公开成员（与 QR 风格一致）
    QVector3D min;
    QVector3D max;
};

// 流输出调试
inline QDebug operator<<(QDebug debug, const ViewCube &cube) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "ViewCube(min=" << cube.min << ", max=" << cube.max << ')';
    return debug;
}