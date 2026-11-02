// QBarSeries.cpp —— 柱状系列实现（收集 Numeric Rect 图元，绘制统一由 renderer 完成）
#include "QBarSeries.h"
#include "QChartScene.h"
#include "QChartPrimitive.h"
#include "QChartAxis.h"
#include <cmath>

QBarSeries::QBarSeries(const QString& name, QObject* parent)
    : QChartSeries(name, parent) {
    m_themeColor = QColor("#2196F3");
}

// ===== 数据操作 =====
void QBarSeries::append(qreal left, qreal top, qreal right, qreal bottom) {
    m_rects.append(QDataRect(left, bottom, right, top));   // QDataRect(left, bottom, right, top)
    emit dataChanged();
}

void QBarSeries::append(const QDataRect& rect) {
    m_rects.append(rect);
    emit dataChanged();
}

void QBarSeries::replace(int i, const QDataRect& rect) {
    if (i < 0 || i >= m_rects.size()) return;
    m_rects[i] = rect;
    emit dataChanged();
}

void QBarSeries::remove(int i) {
    if (i < 0 || i >= m_rects.size()) return;
    m_rects.removeAt(i);
    emit dataChanged();
}

void QBarSeries::clear() {
    if (m_rects.isEmpty()) return;
    m_rects.clear();
    emit dataChanged();
}

// ===== 动画覆盖层（TODO：暂未参与收集）=====
void QBarSeries::setRenderOverride(const QVector<QRectF>& numericRects) {
    m_overrideRects = numericRects;
    m_hasOverride = true;
    emit renderOverrideChanged();
}

void QBarSeries::clearRenderOverride() {
    m_overrideRects.clear();
    m_hasOverride = false;
    emit renderOverrideChanged();
}

// ===== 图元收集：每个矩形 → 一个 Rect（Numeric 轴对齐）；任何一边非法 → 跳过 =====
// 投影快路径由 renderer 决定（恒等 → cartRect 直绘；非线性 → 转 Polygon），series 不关心。
void QBarSeries::collectPrimitives(QChartScene& scene,
                                   const QChartAxis* ax, const QChartAxis* ay,
                                   const QChartAxis* az) const {
    Q_UNUSED(az);   // 二维系列不用第三轴

    const QColor fill = m_fillColor.isValid() ? m_fillColor : color();
    const bool hasBorder = (m_pen.style() != Qt::NoPen);
    const QColor border = hasBorder ? m_pen.color() : fill;
    const qreal penW = hasBorder ? qMax(1.0, m_pen.widthF()) : 1.0;

    for (const QDataRect& r : m_rects) {
        // Data(QVariant) → Numeric(qreal)：轴负责数值化；无轴时按 toDouble 直通
        const qreal l = ax ? ax->toNumeric(r.left()) : r.left().toDouble();
        const qreal rr = ax ? ax->toNumeric(r.right()) : r.right().toDouble();
        const qreal b = ay ? ay->toNumeric(r.bottom()) : r.bottom().toDouble();
        const qreal t = ay ? ay->toNumeric(r.top()) : r.top().toDouble();
        if (!std::isfinite(l) || !std::isfinite(rr) || !std::isfinite(b) || !std::isfinite(t))
            continue;

        QChartPrimitive p;
        p.type = QChartPrimitive::Type::Rect;
        // Numeric 轴上对齐：左上（min x, min y）+ 宽高
        p.numRect = QRectF(QPointF(qMin(l, rr), qMin(b, t)),
                           QPointF(qMax(l, rr), qMax(b, t))).normalized();
        p.fillColor = fill;
        p.color = border;
        p.penWidth = static_cast<float>(penW);
        scene.primitives.append(p);
    }
}

// ===== 命中检测：像素落在哪个矩形内（旧签名；拾取轮替换）=====
int QBarSeries::hitTest(const QPointF& pixel,
                        std::function<QPointF(QVariant,QVariant)> toPixel,
                        const DrawContext* ctx) const {
    Q_UNUSED(ctx);
    if (!toPixel || !m_visible) return -1;

    for (int i = 0; i < m_rects.size(); ++i) {
        const QDataRect& r = m_rects.at(i);
        const QPointF tl = toPixel(QVariant::fromValue(r.left()), QVariant::fromValue(r.top()));
        const QPointF br = toPixel(QVariant::fromValue(r.right()), QVariant::fromValue(r.bottom()));
        if (!std::isfinite(tl.x()) || !std::isfinite(tl.y())
            || !std::isfinite(br.x()) || !std::isfinite(br.y()))
            continue;
        if (QRectF(tl, br).normalized().contains(pixel)) return i;
    }
    return -1;
}
