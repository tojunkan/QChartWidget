// QChartSeries3D.cpp —— 3D 系列基类实现
// Data 层存储/变更 + 2D draw 桩（名字隐藏红线的可实例化兜底）。
#include "QChartSeries3D.h"
#include <QDebug>

QChartSeries3D::QChartSeries3D(const QString& name, QObject* parent)
    : QChartSeries(name, parent) {}

// ===== 数据 =====
int QChartSeries3D::count() const {
    return m_points.size();
}

QDataPoint3D QChartSeries3D::at(int i) const {
    if (i < 0 || i >= m_points.size()) return QDataPoint3D();
    return m_points.at(i);
}

void QChartSeries3D::append(const QDataPoint3D& pt) {
    m_points.append(pt);
    emit dataChanged();
}

void QChartSeries3D::append(qreal x, qreal y, qreal z) {
    append(QDataPoint3D(QVariant(x), QVariant(y), QVariant(z)));
}

void QChartSeries3D::append(QVariant x, QVariant y, QVariant z) {
    append(QDataPoint3D(std::move(x), std::move(y), std::move(z)));
}

void QChartSeries3D::insert(int index, const QDataPoint3D& pt) {
    index = qBound(0, index, m_points.size());
    m_points.insert(index, pt);
    emit dataChanged();
}

void QChartSeries3D::remove(int index) {
    if (index < 0 || index >= m_points.size()) return;
    m_points.remove(index);
    emit dataChanged();
}

void QChartSeries3D::replace(int index, const QDataPoint3D& pt) {
    if (index < 0 || index >= m_points.size()) return;
    m_points.replace(index, pt);
    emit dataChanged();
}

void QChartSeries3D::clear() {
    if (m_points.isEmpty()) return;
    m_points.clear();
    emit dataChanged();
}

void QChartSeries3D::setPoints(const QVector<QDataPoint3D>& pts) {
    m_points = pts;
    emit dataChanged();
}

// ===== 3D draw（基类通用实现：collectPrimitives 后按图元类型直绘；子类可覆写特化）=====
void QChartSeries3D::draw(QPainter* painter,
                          const ProjectFn3D& projectFn,
                          const DrawContext3D* ctx) const {
    Q_UNUSED(ctx);
    if (!painter || !projectFn) return;
    QVector<QChartPrimitive> items;
    collectPrimitives(projectFn, items);
    painter->save();
    painter->setOpacity(opacity());
    for (const QChartPrimitive& prim : items) {
        painter->setPen(QPen(prim.color, prim.penWidth));
        if (prim.type == QChartPrimitive::Type::Point) {
            painter->setBrush(prim.color);
            const qreal r = prim.markerSize * 0.5;
            painter->drawEllipse(prim.a, r, r);          // 无排序直绘
        } else {
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(prim.a, prim.b);
        }
    }
    painter->restore();
}

// ===== 2D draw 桩（⚠ 名字隐藏红线兜底）：3D 系列不可走 2D 绘制路径 =====
void QChartSeries3D::draw(QPainter* painter,
                          std::function<QPointF(QVariant, QVariant)> toPixel,
                          const struct DrawContext* ctx) const {
    Q_UNUSED(painter); Q_UNUSED(toPixel); Q_UNUSED(ctx);
    qWarning() << "QChartSeries3D::draw(2D 签名): 3D 系列必须经 QChartSeries3D* 走 3D draw（ProjectFn3D 全链闭包），"
                  "禁止通过 QChartSeries* 多态调用（design_3d.md §6.2 红线）";
}
