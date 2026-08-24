// QXYSeries.h —— 点集系列（数据层）
// 存储 Data 空间的点（QDataPoint），提供增删改查。
// 绘制由子类（QScatterSeries/QLineSeries/QRegionSeries）实现。
#pragma once
#include "QChartSeries.h"
#include "QDataPoint.h"
#include <QVector>

class QXYSeries : public QChartSeries
{
    Q_OBJECT
public:
    explicit QXYSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 数据查询 =====
    int count() const override { return m_points.size(); }
    const QVector<QDataPoint>& points() const { return m_points; }
    QDataPoint at(int i) const { return m_points.at(i); }

    // ===== 动画覆盖层 —— 优先于真实数据被 draw() 使用 =====
    void setRenderOverride(const QVector<QPointF>& numericPts);
    void clearRenderOverride();
    bool hasRenderOverride() const { return m_hasOverride; }
    const QVector<QPointF>& renderOverride() const { return m_overridePoints; }

    // ===== 数据操作（改数据后发 dataChanged）=====
    void append(qreal x, qreal y);                    // 便捷：qreal 版本
    void append(const QDataPoint& pt);
    void insert(int index, const QDataPoint& pt);
    void remove(int index);
    void replace(int index, const QDataPoint& pt);
    void clear();
    void setPoints(const QVector<QDataPoint>& pts);   // 整批替换

signals:
    void dataChanged();             // 任何数据改动都发
    void renderOverrideChanged();   // 动画覆盖层变化（每帧可能多次）

protected:
    QVector<QDataPoint> m_points;
    // 动画临时点集（Numeric空间），优先于 m_points 被 draw() 使用
    QVector<QPointF> m_overridePoints;
    bool m_hasOverride = false;
};
