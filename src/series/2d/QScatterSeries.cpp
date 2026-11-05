// QScatterSeries.cpp —— 散点系列实现（收集 Numeric 图元，绘制统一由 renderer 完成）
#include "QScatterSeries.h"
#include "QChartScene.h"
#include "QChartPrimitive.h"
#include "QChartAxis.h"   // 轴数值化：axis->toNumeric(QVariant)
#include <cmath>

QScatterSeries::QScatterSeries(const QString& name, QObject* parent)
    : QXYSeries(name, parent) {
    // 默认给个显眼颜色
    m_themeColor = QColor("#F44336");
}

// ===== 图元收集：每点一个 Point 图元（Numeric 空间），非法点跳过 =====
void QScatterSeries::collectPrimitives(QChartSegment& seg,
                                       const QChartAxis* ax, const QChartAxis* ay,
                                       const QChartAxis* az) const {
    Q_UNUSED(az);   // 二维系列不用第三轴
    for (int i = 0; i < m_points.size(); ++i) {
        const QDataPoint& pt = m_points.at(i);
        // Data(QVariant) → Numeric(qreal)：轴负责数值化；无轴时按 toDouble 直通
        const qreal nx = ax ? ax->toNumeric(pt.x()) : pt.x().toDouble();
        const qreal ny = ay ? ay->toNumeric(pt.y()) : pt.y().toDouble();
        if (!std::isfinite(nx) || !std::isfinite(ny)) continue;

        QChartPrimitive p;
        p.type = QChartPrimitive::Type::Point;
        p.numA = QVector3D(static_cast<float>(nx), static_cast<float>(ny), 0.0f);
        // 契约 C3：一个点一个图元 → 数据下标就是它在系列里的下标
        p.color = color();
        p.fillColor = m_fillColor;
        p.penWidth = static_cast<float>(m_pen.widthF());
        p.markerSize = static_cast<float>(m_markerSize);
        seg.prims.append(p);
    }
}

// ===== 命中检测：像素到最近数据点距离 < marker 半径 =====
// 旧签名（toPixel 闭包，painter 时代）——拾取改造时随 QChartHitTester 一起替换。
int QScatterSeries::hitTest(const QPointF& pixel,
                            std::function<QPointF(QVariant,QVariant)> toPixel,
                            const DrawContext* ctx) const {
    Q_UNUSED(ctx);
    if (!toPixel || !m_visible) return -1;

    qreal threshold = m_markerSize * 1.5;   // 容差：marker 尺寸的 1.5 倍
    int bestIndex = -1;
    qreal bestDist = threshold;

    for (int i = 0; i < m_points.size(); ++i) {
        QPointF p = toPixel(m_points[i].x(), m_points[i].y());
        if (!std::isfinite(p.x()) || !std::isfinite(p.y())) continue;
        qreal d = std::hypot(p.x() - pixel.x(), p.y() - pixel.y());
        if (d < bestDist) {
            bestDist = d;
            bestIndex = i;
        }
    }
    return bestIndex;
}
