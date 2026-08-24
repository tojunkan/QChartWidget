// QDataPoint3D.h —— Data 空间 3D 点（QVariant 三元组，任意 Axis 类型可用）
// ★定案（design_3d.md §6.1）：新建类，不扩展 QDataPoint（扩展会改变 2D 数据类
//   的内存/语义并触碰 2D 代码；本类与 QDataPoint 对称、零影响 2D）。
// Data 层组织：Series 只存 Data（QVariant）；QVector3D 仅渲染时经投影产生。
#pragma once
#include <QVariant>
#include <utility>

class QDataPoint3D {
public:
    QDataPoint3D(QVariant x = {}, QVariant y = {}, QVariant z = {})
        : m_x(std::move(x)), m_y(std::move(y)), m_z(std::move(z)) {}

    QVariant x() const { return m_x; }
    QVariant y() const { return m_y; }
    QVariant z() const { return m_z; }

    void setX(QVariant v) { m_x = std::move(v); }
    void setY(QVariant v) { m_y = std::move(v); }
    void setZ(QVariant v) { m_z = std::move(v); }

private:
    QVariant m_x, m_y, m_z;
};
