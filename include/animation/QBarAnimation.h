// QBarAnimation.h —— 柱集矩形动画
// 在 Numeric 空间驱动 QBarSeries 的临时渲染矩形集（排序演示等）
// 模式 A（lerp）：setTargetRects → 从当前矩形插值到目标
// 模式 B（Generator）：setGenerator → 每帧回调产生矩形集
// 结构与 QNumericSeriesAnimation 完全镜像，只是点→矩形
#pragma once
#include "QChartAnimation.h"
#include <QVector>
#include <QRectF>
#include <functional>

class QBarSeries;

class QBarAnimation : public QChartAnimation {
    Q_OBJECT
public:
    explicit QBarAnimation(QObject* parent = nullptr);

    void setTargetSeries(QBarSeries* series) { m_series = series; }
    QBarSeries* targetSeries() const { return m_series; }

    // ── 模式 A：终点插值 ──
    void setSourceRects(const QVector<QRectF>& numericRects) { m_srcRects = numericRects; }
    void setTargetRects(const QVector<QRectF>& numericRects);

    // ── 模式 B：生成器（物理模拟、排序算法逐步状态等）──
    using Generator = std::function<void(qreal alpha, QVector<QRectF>& out)>;
    void setGenerator(Generator gen);

    const QVector<QRectF>& currentRects() const { return m_tempRects; }

protected:
    void animate(qreal alpha) override;

private:
    QBarSeries* m_series = nullptr;
    QVector<QRectF> m_srcRects;    // alpha=0 的快照矩形（Numeric 空间）
    QVector<QRectF> m_dstRects;    // alpha=1 的目标矩形
    QVector<QRectF> m_tempRects;   // 当前帧的插值矩形
    Generator m_gen;
    bool m_useGenerator = false;
};
