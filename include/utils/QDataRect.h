// QDataRect.h
#pragma once
#include "QDataPoint.h"
#include <QRectF>

class QDataRect {
public:
    QDataRect() = default;

    // 构造：左下角和右上角
    QDataRect(const QDataPoint& bottomLeft, const QDataPoint& topRight)
        : m_bottomLeft(bottomLeft), m_topRight(topRight) {
    }

    // 构造：四个分量（left, bottom, right, top）——注意顺序与 QRectF 不同
    QDataRect(const QVariant& left, const QVariant& bottom,
        const QVariant& right, const QVariant& top)
        : m_bottomLeft(QDataPoint(left, bottom)),
        m_topRight(QDataPoint(right, top)) {
    }

    // 从 QRectF 转换（假定所有分量均为 qreal）
    explicit QDataRect(const QRectF& rect)
        : m_bottomLeft(QDataPoint(rect.left(), rect.bottom())),
        m_topRight(QDataPoint(rect.right(), rect.top())) {
    }

    // ===== 获取四个角的 DataPoint =====
    QDataPoint bottomLeft()  const { return m_bottomLeft; }
    QDataPoint bottomRight() const { return QDataPoint(m_topRight.x(), m_bottomLeft.y()); }
    QDataPoint topLeft()     const { return QDataPoint(m_bottomLeft.x(), m_topRight.y()); }
    QDataPoint topRight()    const { return m_topRight; }

    // ===== 获取四个边的 Data =====
    QVariant left()   const { return m_bottomLeft.x(); }
    QVariant right()  const { return m_topRight.x(); }
    QVariant bottom() const { return m_bottomLeft.y(); }
    QVariant top()    const { return m_topRight.y(); }

    // ===== 设置四个角 =====
    void setBottomLeft(const QDataPoint& p) { m_bottomLeft = p; }
    void setTopRight(const QDataPoint& p) { m_topRight = p; }
    void setBottomRight(const QDataPoint& p) {
        // 只更新 x（right），保留 bottom 的 y
        m_topRight.setX(p.x());
        m_bottomLeft.setY(p.y());
    }
    void setTopLeft(const QDataPoint& p) {
        // 只更新 x（left），保留 top 的 y
        m_bottomLeft.setX(p.x());
        m_topRight.setY(p.y());
    }

    // ===== 设置四个边 =====
    void setLeft(const QVariant& left) { m_bottomLeft.setX(left); }
    void setRight(const QVariant& right) { m_topRight.setX(right); }
    void setBottom(const QVariant& bottom) { m_bottomLeft.setY(bottom); }
    void setTop(const QVariant& top) { m_topRight.setY(top); }

    // ===== 一次性设置所有边（顺序：left, bottom, right, top） =====
    void setRect(const QVariant& left, const QVariant& bottom,
        const QVariant& right, const QVariant& top) {
        m_bottomLeft = QDataPoint(left, bottom);
        m_topRight = QDataPoint(right, top);
    }

    // 从 QRectF 设置（数值版本）
    void setRect(const QRectF& rect) {
        setRect(rect.left(), rect.bottom(), rect.right(), rect.top());
    }

    // ===== 检查是否有效（所有分量均非空且有限，但具体有效性由调用者判断） =====
    bool isValid() const {
        return !left().isNull() && !right().isNull()
            && !bottom().isNull() && !top().isNull();
    }

private:
    QDataPoint m_bottomLeft;
    QDataPoint m_topRight;
};