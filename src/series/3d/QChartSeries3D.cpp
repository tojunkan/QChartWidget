// QChartSeries3D.cpp —— 3D 系列基类实现
// Data 层存储/变更（双存储：float3 权威（数值型）/ QDataPoint3D 权威（QVariant/混合），§9）+ 2D draw 桩。
#include "QChartSeries3D.h"
#include <QDebug>

QChartSeries3D::QChartSeries3D(const QString& name, QObject* parent)
    : QChartSeries(name, parent) {}

// ===== 数据（双存储，design_phase3.md §9；API 语义与 Phase 2 完全一致）=====
int QChartSeries3D::count() const {
    return m_numericCacheActive ? m_numericCache.size() : m_points.size();
}

QDataPoint3D QChartSeries3D::at(int i) const {
    if (m_numericCacheActive) {
        // 数值型：float3 → QDataPoint3D 单点物化（免整表物化）
        if (i < 0 || i >= m_numericCache.size()) return QDataPoint3D();
        const QVector3D& v = m_numericCache.at(i);
        return QDataPoint3D(QVariant(qreal(v.x())), QVariant(qreal(v.y())), QVariant(qreal(v.z())));
    }
    if (i < 0 || i >= m_points.size()) return QDataPoint3D();
    return m_points.at(i);
}

const QVector<QDataPoint3D>& QChartSeries3D::points() const {
    if (!m_numericCacheActive) return m_points;   // QVariant/混合：m_points 即权威
    if (!m_pointsViewValid) {
        // numeric-only：按需物化视图（§9「QVariant 仅按需物化视图」；GL 路径直读 numericCache 不走这里）
        m_pointsView.clear();
        m_pointsView.reserve(m_numericCache.size());
        for (const QVector3D& v : m_numericCache)
            m_pointsView.append(QDataPoint3D(QVariant(qreal(v.x())),
                                             QVariant(qreal(v.y())),
                                             QVariant(qreal(v.z()))));
        m_pointsViewValid = true;
    }
    return m_pointsView;
}

void QChartSeries3D::append(const QDataPoint3D& pt) {
    if (m_numericCacheActive) materializeToQVariant();   // QVariant 点注入 → 回退（顺序语义保持）
    m_points.append(pt);
    m_worldCache.clear();                                // 数据变化 → World 缓存失效（Layer3D 置脏重建）
    emit dataChanged();
}

void QChartSeries3D::append(qreal x, qreal y, qreal z) {
    if (m_numericCacheActive) {
        // 数值型路径：float3 权威存储，12B/点（append 增量维护，§9）
        m_numericCache.append(QVector3D(float(x), float(y), float(z)));
        m_pointsViewValid = false;
        m_worldCache.clear();
        emit dataChanged();
        return;
    }
    append(QDataPoint3D(QVariant(x), QVariant(y), QVariant(z)));   // 混合：物化追加（QVariant 路径）
}

void QChartSeries3D::append(QVariant x, QVariant y, QVariant z) {
    append(QDataPoint3D(std::move(x), std::move(y), std::move(z)));
}

void QChartSeries3D::insert(int index, const QDataPoint3D& pt) {
    index = qBound(0, index, count());
    if (m_numericCacheActive) materializeToQVariant();   // insert 注入 QVariant 点 → 回退（与 replace 一致）
    m_points.insert(index, pt);
    m_worldCache.clear();
    emit dataChanged();
}

void QChartSeries3D::remove(int index) {
    if (m_numericCacheActive) {
        // 数值型：缓存增量维护（免回退；§9「append 追加」同族）
        if (index < 0 || index >= m_numericCache.size()) return;
        m_numericCache.remove(index);
        m_pointsViewValid = false;
        m_worldCache.clear();
        emit dataChanged();
        return;
    }
    if (index < 0 || index >= m_points.size()) return;
    m_points.remove(index);
    m_worldCache.clear();
    emit dataChanged();
}

void QChartSeries3D::replace(int index, const QDataPoint3D& pt) {
    if (m_numericCacheActive) {
        // replace 失效（任务定）：回退到 QDataPoint3D 列表再替换
        if (index < 0 || index >= m_numericCache.size()) return;
        materializeToQVariant();
    }
    if (index < 0 || index >= m_points.size()) return;
    m_points.replace(index, pt);
    m_worldCache.clear();
    emit dataChanged();
}

void QChartSeries3D::clear() {
    const bool had = count() > 0;
    m_points.clear();
    m_numericCache.clear();
    m_numericCacheActive = true;      // 空白系列恢复数值型容量（下次 append 决定模式）
    m_pointsView.clear();
    m_pointsViewValid = false;
    m_worldCache.clear();
    if (had) emit dataChanged();
}

void QChartSeries3D::setPoints(const QVector<QDataPoint3D>& pts) {
    setPointsInternal(pts);
    emit dataChanged();
}

void QChartSeries3D::setPointsInternal(const QVector<QDataPoint3D>& pts) {
    // QVariant 路径：QDataPoint3D 列表权威（Phase 2 边界，§9 回退）
    m_points = pts;
    m_numericCache.clear();
    m_numericCacheActive = false;
    m_pointsView.clear();
    m_pointsViewValid = false;
    m_worldCache.clear();
}

void QChartSeries3D::materializeToQVariant() {
    if (!m_numericCacheActive) return;
    m_points.clear();
    m_points.reserve(m_numericCache.size());
    for (const QVector3D& v : m_numericCache)
        m_points.append(QDataPoint3D(QVariant(qreal(v.x())),
                                     QVariant(qreal(v.y())),
                                     QVariant(qreal(v.z()))));
    m_numericCache.clear();
    m_numericCacheActive = false;
    m_pointsView.clear();
    m_pointsViewValid = false;
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
