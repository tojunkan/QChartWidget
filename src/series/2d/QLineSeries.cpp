// QLineSeries.cpp —— 折线系列实现（收集 Numeric 直线段，绘制统一由 renderer 完成）
#include "QLineSeries.h"
#include "QChartScene.h"
#include "QChartPrimitive.h"
#include "QChartAxis.h"
#include <cmath>

QLineSeries::QLineSeries(const QString& name, QObject* parent)
    : QXYSeries(name, parent) {
    m_themeColor = QColor("#1E88E5");
}

// ===== 图元收集 =====
// 连续的**有效**点组成一段折线：2 个点 → Line 图元；>2 个点 → Path 图元（一次装进 numVerts）。
// NaN/Inf 断线（y=1/x 的两支仍属同一系列），断开后重新起一段。
void QLineSeries::collectPrimitives(QChartScene& scene,
                                    const QChartAxis* ax, const QChartAxis* ay,
                                    const QChartAxis* az) const {
    Q_UNUSED(az);   // 二维系列不用第三轴

    QVector<QVector3D> run;
    auto flush = [&]() {
        if (run.isEmpty()) return;
        QChartPrimitive p;
        if (run.size() == 2) {
            p.type = QChartPrimitive::Type::Line;
            p.numA = run.at(0);
            p.numB = run.at(1);
        } else {
            p.type = QChartPrimitive::Type::Path;
            p.numVerts = run;
        }
        p.color = color();
        p.penWidth = static_cast<float>(m_lineWidth);
        scene.primitives.append(p);
        run.clear();
    };

    for (const QDataPoint& pt : m_points) {
        // Data(QVariant) → Numeric(qreal)：轴负责数值化；无轴时按 toDouble 直通
        const qreal nx = ax ? ax->toNumeric(pt.x()) : pt.x().toDouble();
        const qreal ny = ay ? ay->toNumeric(pt.y()) : pt.y().toDouble();
        if (!std::isfinite(nx) || !std::isfinite(ny)) {   // 断线
            flush();
            continue;
        }
        run.append(QVector3D(static_cast<float>(nx), static_cast<float>(ny), 0.0f));
    }
    flush();
}

// ===== 命中检测：像素到最近线段的距离（旧签名；拾取轮替换）=====
int QLineSeries::hitTest(const QPointF& pixel,
                         std::function<QPointF(QVariant,QVariant)> toPixel,
                         const DrawContext* ctx) const {
    Q_UNUSED(ctx);
    if (!toPixel || !m_visible || m_points.size() < 2) return -1;

    const qreal kThreshold = qMax(4.0, m_lineWidth * 1.5);   // 容差：线宽的 1.5 倍（下限 4px）
    int bestIndex = -1;
    qreal bestDist = kThreshold;

    for (int i = 0; i + 1 < m_points.size(); ++i) {
        const QPointF a = toPixel(m_points[i].x(), m_points[i].y());
        const QPointF b = toPixel(m_points[i + 1].x(), m_points[i + 1].y());
        if (!std::isfinite(a.x()) || !std::isfinite(a.y())
            || !std::isfinite(b.x()) || !std::isfinite(b.y()))
            continue;

        const QPointF ab = b - a;
        const qreal len2 = QPointF::dotProduct(ab, ab);
        qreal d = 0.0;
        if (len2 < 1e-12) {
            const QPointF diff = pixel - a;
            d = std::sqrt(QPointF::dotProduct(diff, diff));
        } else {
            qreal t = QPointF::dotProduct(pixel - a, ab) / len2;
            t = qBound<qreal>(0.0, t, 1.0);
            const QPointF proj = a + ab * t;
            const QPointF diff = pixel - proj;
            d = std::sqrt(QPointF::dotProduct(diff, diff));
        }
        if (d < bestDist) {
            bestDist = d;
            bestIndex = i;
        }
    }
    return bestIndex;
}
