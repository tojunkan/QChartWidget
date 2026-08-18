// QChartHitTester.cpp —— 统一反向映射引擎实现（纯重构：逻辑原样搬入）
#include "QChartHitTester.h"
#include "QChartSeries.h"
#include "QChartRenderer.h"   // QChartPrimitive（Layer/Type）
#include <cmath>

// ===== 2D：顶层可见系列优先 + series->hitTest =====
QChartHitTester::HitResult QChartHitTester::hitTest(
    const QPointF& pixel,
    const QList<QChartSeries*>& seriesList,
    std::function<QPointF(QVariant, QVariant)> toPixel,
    const struct DrawContext* ctx) {
    for (int i = seriesList.size() - 1; i >= 0; --i) {   // 顶层优先（自后向前）
        QChartSeries* s = seriesList[i];
        if (!s || !s->isVisible()) continue;             // 可见性过滤（原逻辑）
        const int idx = s->hitTest(pixel, toPixel, ctx);
        if (idx >= 0)
            return { s, idx, idx };                      // 2D 下 dataIndex == index
    }
    return {};                                           // 未命中
}

// ===== 3D：屏幕近邻（Series 层过滤 + 点/线段距离 + 阈值取最近 + dataIndex 透传）=====
QChartHitTester::HitResult QChartHitTester::hitTest(
    const QPointF& pixel,
    const QVector<QChartPrimitive>& primitives,
    qreal maxDistPx) {
    HitResult best;
    qreal bestDist = maxDistPx;
    for (const QChartPrimitive& prim : primitives) {
        if (prim.dataIndex < 0 || prim.layer != QChartPrimitive::Layer::Series)
            continue;   // 只扫 Series 层（Grid/ForegroundDecor 排除；dataIndex<0 排除）
        const qreal d = distanceToPrimitive(pixel, prim);
        if (d < bestDist) {
            bestDist = d;
            best.dataIndex = prim.dataIndex;
        }
    }
    return best;   // 未命中：dataIndex == -1（空 HitResult）
}

// ===== 像素到图元距离 =====
qreal QChartHitTester::distanceToPrimitive(const QPointF& pos, const QChartPrimitive& prim) {
    if (prim.type == QChartPrimitive::Type::Point) {
        const QPointF d = pos - prim.a;
        return std::sqrt(QPointF::dotProduct(d, d));
    }
    // LineSegment：点到线段距离
    const QPointF ab = prim.b - prim.a;
    const qreal len2 = QPointF::dotProduct(ab, ab);
    if (len2 < 1e-12) {
        const QPointF d = pos - prim.a;
        return std::sqrt(QPointF::dotProduct(d, d));
    }
    qreal t = QPointF::dotProduct(pos - prim.a, ab) / len2;
    t = qBound<qreal>(0.0, t, 1.0);
    const QPointF proj = prim.a + ab * t;
    const QPointF d = pos - proj;
    return std::sqrt(QPointF::dotProduct(d, d));
}
