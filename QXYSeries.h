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

    // ===== 数据操作（改数据后发 dataChanged）=====
    void append(qreal x, qreal y);                    // 便捷：qreal 版本
    void append(const QDataPoint& pt);
    void insert(int index, const QDataPoint& pt);
    void remove(int index);
    void replace(int index, const QDataPoint& pt);
    void clear();
    void setPoints(const QVector<QDataPoint>& pts);   // 整批替换

signals:
    void dataChanged(); // 任何数据改动都发

protected:
    QVector<QDataPoint> m_points;
};
