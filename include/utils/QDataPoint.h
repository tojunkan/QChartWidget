// QDataPoint.h —— 数据点
// 五空间链中：Data 空间的点。存 QVariant——兼容 QValueAxis(qreal)、
// QDateTimeAxis(QDateTime)、QBarCategoryAxis(QString) 等所有 Axis 类型。
// Series 不关心 Data 的具体类型，由 Axis::toNumeric 在渲染时转换。
#pragma once
#include <QVariant>

class QDataPoint {
public:
    QDataPoint(QVariant x = {}, QVariant y = {}) : m_x(std::move(x)), m_y(std::move(y)) {}

    QVariant x() const { return m_x; }
    QVariant y() const { return m_y; }
    void setX(QVariant v) { m_x = std::move(v); }
    void setY(QVariant v) { m_y = std::move(v); }

private:
    QVariant m_x;
    QVariant m_y;
};
