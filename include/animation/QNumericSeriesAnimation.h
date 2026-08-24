// QNumericSeriesAnimation.h —— 数值点集动画
// 在 Numeric 空间驱动 Series 的临时渲染点集
// 模式 A（lerp）：setTargetPoints → 从当前数据插值到目标
// 模式 B（Generator）：setGenerator → 每帧回调产生点集（物理模拟等）
#pragma once
#include "QChartAnimation.h"
#include <QVector>
#include <QPointF>
#include <functional>

class QXYSeries;

class QNumericSeriesAnimation : public QChartAnimation {
    Q_OBJECT
public:
    explicit QNumericSeriesAnimation(QObject* parent = nullptr);

    void setTargetSeries(QXYSeries* series) { m_series = series; }
    QXYSeries* targetSeries() const { return m_series; }

    // ── 模式 A：终点插值 ──
    void setSourcePoints(const QVector<QPointF>& numericPts) { m_srcPoints = numericPts; }
    void setTargetPoints(const QVector<QPointF>& numericPts);

    // ── 模式 B：生成器（物理模拟等）──
    using Generator = std::function<void(qreal alpha, QVector<QPointF>& out)>;
    void setGenerator(Generator gen);

    const QVector<QPointF>& currentPoints() const { return m_tempPoints; }

protected:
    void animate(qreal alpha) override;

private:
    QXYSeries* m_series = nullptr;
    QVector<QPointF> m_srcPoints;    // alpha=0 的快照点集（Numeric空间）
    QVector<QPointF> m_dstPoints;    // alpha=1 的目标点集
    QVector<QPointF> m_tempPoints;   // 当前帧的插值点集
    Generator m_gen;
    bool m_useGenerator = false;
};
