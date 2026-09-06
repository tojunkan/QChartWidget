// QChartWidget.cpp —— 2D 图表控件实现（S0+批次 A：纯容器）
#include "QChartWidget.h"
#include "QChartCamera.h"
#include "QCartesianProjection.h"
#include <QPainter>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logWidget, "chart.widget")

// ===== 构造 / 析构 =====

QChartWidget::QChartWidget(QWidget* parent)
    : QChartAbstractWidget(parent)
{
    // 容器唯一投影默认 Cartesian（恒等 Numeric↔View）；可经 setProjection 替换
    m_projection = std::make_unique<QCartesianProjection>();
}

QChartWidget::~QChartWidget() = default;

// ===== 图层管理 =====

void QChartWidget::addLayer(QChartLayer* layer)
{
    QChartAbstractWidget::addLayer(layer);
    if (layer && m_layers.size() == 1) {
        // 便捷轴挂到首个图层（多图层时其余图层轴由调用方经 layer->setAxisX/Y 自绑）
        for (QChartAxis* a : m_axes) {
            if (!isAxisAttached(a)) attachToLayer(layer, a);
        }
        m_viewSynced = false;   // 下次渲染前从轴范围同步相机（axis→camera→viewRect）
    }
}

void QChartWidget::removeLayer(QChartLayer* layer)
{
    QChartAbstractWidget::removeLayer(layer);
    if (m_layers.isEmpty())
        m_viewSynced = false;
}

// ===== 轴管理 =====

void QChartWidget::addAxis(QChartAxis* a)
{
    if (!a || m_axes.contains(a)) return;
    m_axes.append(a);
    if (!m_layers.isEmpty()) {
        attachToLayer(m_layers.first(), a);
        m_viewSynced = false;
    }
}

void QChartWidget::removeAxis(QChartAxis* a)
{
    m_axes.removeAll(a);
    for (QChartLayer* l : m_layers) {
        if (l->axisX() == a) l->setAxisX(nullptr);
        if (l->axisY() == a) l->setAxisY(nullptr);
    }
}

void QChartWidget::attachToLayer(QChartLayer* layer, QChartAxis* a)
{
    if (a->isHorizontal()) {
        if (!layer->axisX()) layer->setAxisX(a);
    } else {
        if (!layer->axisY()) layer->setAxisY(a);
    }
}

bool QChartWidget::isAxisAttached(QChartAxis* a) const
{
    for (QChartLayer* l : m_layers) {
        if (l->axisX() == a || l->axisY() == a) return true;
    }
    return false;
}

// ===== Projection =====

void QChartWidget::setProjection(std::unique_ptr<QChartProjection> proj)
{
    if (!proj) return;
    m_projection = std::move(proj);
    m_viewSynced = false;
    emit projectionChanged(m_projection.get());
    m_layoutDirty = true;
    scheduleRepaint();
}

// ===== viewRect/dataBounds 驱动链 =====

QRectF QChartWidget::viewRect() const
{
    for (QChartLayer* l : m_layers) {
        if (l && l->camera() && l->camera()->viewRect().width() > 0.0)
            return l->camera()->viewRect();
    }
    return QRectF();
}

void QChartWidget::setViewRect(const QRectF& r)
{
    if (r.width() <= 0.0 || r.height() <= 0.0) return;
    for (QChartLayer* l : m_layers)
        if (l) l->camera()->setViewRect(r);
    m_viewSynced = true;
    recomputeDataBounds();
}

void QChartWidget::recomputeDataBounds()
{
    if (!m_projection) return;
    const QRectF vr = viewRect();
    if (vr.width() <= 0.0 || vr.height() <= 0.0) return;

    // projection 输出数学取向 rect（x=dim0, y=dim1 数值域）
    const QRectF math = m_projection->computeDataBounds(vr);
    QRectF legacy;
    legacy.setLeft(math.left());
    legacy.setRight(math.left() + math.width());
    legacy.setBottom(math.top());                 // dim1 数值 min
    legacy.setTop(math.top() + math.height());    // dim1 数值 max
    m_dataBounds = legacy;

    for (QChartLayer* l : m_layers)
        if (l) l->setNumericBounds(m_dataBounds);
    scheduleRepaint();
}

void QChartWidget::panViewCartesian(qreal dx, qreal dy)
{
    for (QChartLayer* l : m_layers)
        if (l) l->camera()->panViewCartesian(dx, dy);
    recomputeDataBounds();
}

void QChartWidget::zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY)
{
    for (QChartLayer* l : m_layers)
        if (l) l->camera()->zoomViewCartesian(cx, cy, factorX, factorY);
    recomputeDataBounds();
}

// ===== 坐标转换（转发层相机）=====

QPointF QChartWidget::cartesianToPixel(qreal cx, qreal cy) const
{
    for (QChartLayer* l : m_layers) {
        if (l && l->camera()) {
            const QChartProjectedPoint pp =
                l->camera()->project(QVector3D(cx, cy, 0.0f), m_plotArea);
            return pp.screen;
        }
    }
    return QPointF(qQNaN(), qQNaN());
}

QPointF QChartWidget::pixelToCartesian(const QPointF& pixel) const
{
    for (QChartLayer* l : m_layers) {
        if (l && l->camera()) {
            const Ray ray = l->camera()->unproject(pixel, m_plotArea);
            return QPointF(ray.origin.x(), ray.origin.y());
        }
    }
    return QPointF(qQNaN(), qQNaN());
}

// ===== 布局 =====

QRectF QChartWidget::calculatePlotArea() const
{
    QRectF area = QRectF(rect());
    if (area.width() <= 0.0 || area.height() <= 0.0) return QRectF();

    qreal l = m_marginLeft, t = m_marginTop, r = m_marginRight, b = m_marginBottom;
    const QFont f = font();
    // 边框轴 sizeHint 占用外边距（Bottom/Top 水平轴占高度；Left/Right 垂直轴占宽度）
    for (QChartLayer* layer : m_layers) {
        if (!layer) continue;
        const QChartAxis* axes[2] = { layer->axisX(), layer->axisY() };
        for (const QChartAxis* a : axes) {
            if (!a) continue;
            const Qt::Alignment al = a->alignment();
            if (al == Qt::AlignBottom || al == Qt::AlignTop) {
                const qreal h = a->sizeHint(f).height();
                if (al == Qt::AlignBottom) b += h; else t += h;
            } else if (al == Qt::AlignLeft || al == Qt::AlignRight) {
                const qreal w = a->sizeHint(f).width();
                if (al == Qt::AlignLeft) l += w; else r += w;
            }
            // HCenter/VCenter（数据主脊）不占外边距
        }
    }

    // 防止边距吞掉整个画布
    const qreal minSide = 40.0;
    if (l + r > area.width() - minSide) {
        const qreal k = (area.width() - minSide) / (l + r);
        l *= k; r *= k;
    }
    if (t + b > area.height() - minSide) {
        const qreal k = (area.height() - minSide) / (t + b);
        t *= k; b *= k;
    }
    return area.adjusted(l, t, -r, -b);
}

void QChartWidget::layoutAxes()
{
    relayout();   // calculatePlotArea → 广播 plotAreaChanged + 摆放 GL 子控件
}

// ===== 渲染前同步（相机归 layer：axis sugar → camera viewRect 单次同步）=====

void QChartWidget::onBeforePaint()
{
    if (m_viewSynced) return;
    if (!m_projection) return;

    // 以首个带轴图层的轴范围为准（Axis sugar → 数值域 → projection.computeViewRect → 相机）
    QChartLayer* source = nullptr;
    for (QChartLayer* l : m_layers) {
        if (l && l->axisX() && l->axisY()) { source = l; break; }
    }
    if (!source) return;

    const qreal x0 = source->axisX()->min(), x1 = source->axisX()->max();
    const qreal y0 = source->axisY()->min(), y1 = source->axisY()->max();
    if (x1 <= x0 || y1 <= y0) return;

    QRectF legacy;
    legacy.setLeft(x0); legacy.setRight(x1);
    legacy.setBottom(y0); legacy.setTop(y1);

    // 数值域(math) → Cartesian 视图窗
    const QRectF mathRect(legacy.left(), legacy.bottom(),
                          legacy.right() - legacy.left(),
                          legacy.top() - legacy.bottom());
    const QRectF cart = m_projection->computeViewRect(mathRect);
    for (QChartLayer* l : m_layers)
        if (l) l->camera()->setViewRect(cart);

    m_dataBounds = legacy;
    for (QChartLayer* l : m_layers)
        if (l) l->setNumericBounds(legacy);
    m_viewSynced = true;
}

// ===== 外部内容：边框轴 drawAtEdge（plotArea 外，CPU/GL 共用）=====

void QChartWidget::drawExternalContent(QPainter& painter)
{
    if (m_layers.isEmpty() || m_plotArea.width() <= 0.0) return;

    DrawContext ctx;
    ctx.plotArea = m_plotArea;
    ctx.dataBounds = m_dataBounds;
    ctx.viewRect = viewRect();
    ctx.projection = m_projection.get();

    painter.save();
    for (QChartLayer* layer : m_layers) {
        if (!layer) continue;
        const QChartAxis* axes[2] = { layer->axisX(), layer->axisY() };
        for (const QChartAxis* a : axes) {
            if (!a) continue;
            const Qt::Alignment al = a->alignment();
            if (al != Qt::AlignBottom && al != Qt::AlignTop
                && al != Qt::AlignLeft && al != Qt::AlignRight)
                continue;   // 数据主脊（HCenter/VCenter）不进外部边框
            painter.save();
            a->drawAtEdge(&painter, ctx, true, true, true);
            painter.restore();
        }
    }
    painter.restore();
}
