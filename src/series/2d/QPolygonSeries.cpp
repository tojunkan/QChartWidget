// QPolygonSeries.cpp —— 多边形系列实现（收集 Numeric 图元，绘制统一由 renderer 完成）
#include "QPolygonSeries.h"
#include "QChartScene.h"
#include "QChartPrimitive.h"
#include "QChartAxis.h"
#include <cmath>

QPolygonSeries::QPolygonSeries(const QString& name, QObject* parent)
    : QXYSeries(name, parent) {
    m_themeColor = QColor("#42A5F5");
}

void QPolygonSeries::setFillColor(const QColor& c) {
    if (m_fillColor == c) return;
    m_fillColor = c;
    emit fillColorChanged();
}

void QPolygonSeries::setStrokeVisible(bool v) {
    if (m_strokeVisible == v) return;
    m_strokeVisible = v;
    emit strokeChanged();
}

// ===== 图元收集 =====
// 每段连续有效顶点 → 一个 Polygon（填充；renderer 负责三角化）；
// strokeVisible 时再补一条闭合 Path（额外首点收尾）。NaN/Inf 断段，各段独立成多边形。
void QPolygonSeries::collectPrimitives(QChartScene& scene,
                                       const QChartAxis* ax, const QChartAxis* ay,
                                       const QChartAxis* az) const {
    Q_UNUSED(az);   // 二维系列不用第三轴

    QVector<QVector3D> run;
    auto flush = [&]() {
        if (run.size() < 3) { run.clear(); return; }   // 少于 3 点构不成多边形

        // 填充色：未显式设色 → 系列色半透明
        QColor fill = m_fillColor;
        if (fill.alpha() == 0) {
            fill = color();
            fill.setAlpha(120);
        }

        // ① 填充：Polygon（隐式闭合）
        QChartPrimitive poly;
        poly.type = QChartPrimitive::Type::Polygon;
        poly.numVerts = run;
        poly.fillColor = fill;
        poly.color = fill;              // 供不区分 fill/border 的后端兜底
        scene.primitives.append(poly);

        // ② 描边：额外一条闭合 Path
        if (m_strokeVisible) {
            QChartPrimitive stroke;
            stroke.type = QChartPrimitive::Type::Path;
            stroke.numVerts = run;
            stroke.numVerts.append(run.first());   // 闭合
            stroke.color = color();
            stroke.penWidth = 1.0f;               // TODO(stroke-width)
            scene.primitives.append(stroke);
        }
        run.clear();
    };

    for (const QDataPoint& pt : m_points) {
        // Data(QVariant) → Numeric(qreal)：轴负责数值化；无轴时按 toDouble 直通
        const qreal nx = ax ? ax->toNumeric(pt.x()) : pt.x().toDouble();
        const qreal ny = ay ? ay->toNumeric(pt.y()) : pt.y().toDouble();
        if (!std::isfinite(nx) || !std::isfinite(ny)) {   // 断段
            flush();
            continue;
        }
        run.append(QVector3D(static_cast<float>(nx), static_cast<float>(ny), 0.0f));
    }
    flush();
}

// ===== 命中检测：像素落在哪个多边形内（旧签名；拾取轮替换）=====
int QPolygonSeries::hitTest(const QPointF& pixel,
                            std::function<QPointF(QVariant,QVariant)> toPixel,
                            const DrawContext* ctx) const {
    Q_UNUSED(ctx);
    if (!toPixel || !m_visible) return -1;

    QPolygonF poly;
    int runStart = 0;
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF p = toPixel(m_points[i].x(), m_points[i].y());
        if (!std::isfinite(p.x()) || !std::isfinite(p.y())) {   // 断段：判定当前环
            if (poly.size() >= 3 && poly.containsPoint(pixel, Qt::OddEvenFill))
                return runStart;
            poly.clear();
            runStart = i + 1;
            continue;
        }
        poly.append(p);
    }
    if (poly.size() >= 3 && poly.containsPoint(pixel, Qt::OddEvenFill))
        return runStart;
    return -1;
}
