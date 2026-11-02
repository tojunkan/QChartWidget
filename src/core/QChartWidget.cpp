// QChartWidget.cpp —— 2D 图表控件实现（S0+批次 A：纯容器）
#include "QChartWidget.h"
#include "QChartCamera.h"
#include "QCartesianProjection.h"
#include <QPainter>
#include <QLoggingCategory>
#include <QMouseEvent>     // 4f：交互接线
#include <QWheelEvent>
#include <QtMath>
#include <cmath>

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

void QChartWidget::addLayer(QChartAbstractLayer* layer)
{
    QChartAbstractWidget::addLayer(layer);
    // 便捷轴绑定只对二维层生效（三维层无 axisX/axisY 语义）
    QChartLayer* l2 = dynamic_cast<QChartLayer*>(layer);
    if (!l2) return;
    connect(l2, &QChartLayer::viewChanged, this, &QChartWidget::updateAxes);
    for (QChartAxis* a : m_axes) {
        if (!isAxisAttached(a)) attachToLayer(l2, a);
    }
    // 图层/轴绑定变化 → 数值侧同步一次（轴范围 → 相机窗口）
    if (m_projection) {
        const QRectF db = dataBounds();
        if (!db.isEmpty()) l2->setViewRect(m_projection->computeViewRect(db));
    }
    scheduleRepaint();
}

QChartLayer* QChartWidget::layer2D() const
{
    return dynamic_cast<QChartLayer*>(m_layer);
}

// ===== 轴管理 =====

void QChartWidget::addAxis(QChartAxis* a)
{
    if (!a || m_axes.contains(a)) return;
    m_axes.append(a);
    // 4e：轴范围变化 = 数值侧驱动（首帧后同样生效——取代 4c 的一次性闩锁语义）
    connect(a, &QChartAxis::rangeChanged, this, &QChartWidget::onAxisRangeChanged, Qt::UniqueConnection);
    // 4g-fix（t58 F1）：轴样式类变化 → 同样必须重收集（网格脊/刻度/标签由轴刻度与样式生成）。
    // 逐信号接线 + 内容指纹（下方 onBeforePaint 注入）双保险；addAxis 对同一轴只连一次（m_axes 去重）。
    // （lambda 内联：QChartWidget.h 不在本任务 inScope，故不新增成员函数）
    auto markContentDirty = [this]() {
        if (QChartLayer* l = layer2D()) l->invalidateData();
        scheduleRepaint();
    };
    connect(a, &QChartAxis::tickCountChanged, this, [markContentDirty]() { markContentDirty(); });
    connect(a, &QChartAxis::subTickCountChanged, this, [markContentDirty]() { markContentDirty(); });
    connect(a, &QChartAxis::styleChanged, this, [markContentDirty]() { markContentDirty(); });
    connect(a, &QChartAxis::visibleChanged, this, [markContentDirty]() { markContentDirty(); });
    if (QChartLayer* l = layer2D()) {
        attachToLayer(l, a);
        if (m_projection) {
            const QRectF db = dataBounds();
            if (!db.isEmpty()) l->setViewRect(m_projection->computeViewRect(db));
        }
        scheduleRepaint();
    }
}

void QChartWidget::removeAxis(QChartAxis* a)
{
    disconnect(a, nullptr, this, nullptr);   // 4e：解除范围变化监听（轴非持有，防悬挂连接）
    m_axes.removeAll(a);
    if (QChartLayer* l = layer2D()) {
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
    QChartLayer* l = layer2D();
    return l && (l->axisX() == a || l->axisY() == a);
}

// ===== Projection =====

void QChartWidget::setProjection(std::unique_ptr<QChartProjection> proj)
{
    if (!proj) return;
    m_projection = std::move(proj);
    emit projectionChanged(m_projection.get());
    m_layoutDirty = true;
    // 投影变化 → 用新投影重算相机窗口
    if (QChartLayer* l = layer2D()) {
        const QRectF db = dataBounds();
        if (!db.isEmpty()) l->setViewRect(m_projection->computeViewRect(db));
    }
    scheduleRepaint();
}

// ===== viewRect/dataBounds 驱动链（4b：轴是范围的唯一持有者，dataBounds 按需临时组装）=====

QRectF QChartWidget::dataBounds() const
{
    // 按需从二维层的轴范围组装（正常 QRectF：left/right=dim0、top/bottom=dim1；无层/无轴→空）
    QChartLayer* l = layer2D();
    if (l && l->axisX() && l->axisY())
        return l->numericBoundsFromAxes();
    return QRectF();
}

QRectF QChartWidget::viewRect() const
{
    QChartLayer* l = layer2D();
    if (l && l->camera() && l->camera()->viewRect().width() > 0.0)
        return l->camera()->viewRect();
    return QRectF();
}

void QChartWidget::setViewRect(const QRectF& r)
{
    if (r.width() <= 0.0 || r.height() <= 0.0) return;
    if (QChartLayer* l = layer2D())
        l->camera()->setViewRect(r);   // 相机变化 → layer 转发 viewChanged → updateAxes
    scheduleRepaint();
}

// ===== 驱动链（轴 ↔ 相机）=====

void QChartWidget::onAxisRangeChanged(qreal min, qreal max)
{
    Q_UNUSED(min); Q_UNUSED(max);
    if (m_cameraTriggered) return;   // 本类写轴引起的 rangeChanged，忽略（防回环）
    QChartLayer* l = layer2D();
    if (!l || !m_projection) return;
    const QRectF db = dataBounds();
    if (db.isEmpty()) return;        // 轴未绑/范围退化：不 fit
    l->setViewRect(m_projection->computeViewRect(db));
    scheduleRepaint();
}

void QChartWidget::updateAxes(const QRectF& viewRect)
{
    if (!m_projection) return;
    m_cameraTriggered = true;
    const QRectF db = m_projection->computeDataBounds(viewRect);
    if (QChartLayer* l = layer2D()) {
        if (l->axisX()) l->axisX()->setRange(db.left(), db.right());
        if (l->axisY()) l->axisY()->setRange(db.top(), db.top() + db.height());
    }
    m_cameraTriggered = false;
    scheduleRepaint();
}

void QChartWidget::panViewCartesian(qreal dx, qreal dy)
{
    if (QChartLayer* l = layer2D())
        l->camera()->panViewCartesian(dx, dy);   // 相机变化 → viewChanged → updateAxes
    scheduleRepaint();
}

void QChartWidget::zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY)
{
    if (QChartLayer* l = layer2D())
        l->camera()->zoomViewCartesian(cx, cy, factorX, factorY);
    scheduleRepaint();
}

// ===== 4f：鼠标交互接线（像素级操作由相机完成，widget 只传像素）=====

// 滚轮 → 缩放因子：120/格 → 1.15 倍；上滚（正值）放大（视图窗口收缩 → factor < 1）
qreal QChartWidget::wheelZoomFactor(int angleDeltaY)
{
    if (angleDeltaY == 0) return 1.0;
    return qPow(1.15, -qreal(angleDeltaY) / 120.0);
}

void QChartWidget::onMousePress(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    m_panning = true;
    m_lastPixel = e->position();
}

void QChartWidget::onMouseMove(QMouseEvent* e)
{
    if (!m_panning) return;
    const QPointF pos = e->position();
    const QPointF delta = pos - m_lastPixel;
    m_lastPixel = pos;
    if (delta.isNull()) return;
    if (QChartLayer* l = layer2D())
        l->camera()->panByPixels(delta, m_plotArea);   // 像素 → 数据的换算在相机内
}

void QChartWidget::onMouseRelease(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) m_panning = false;
}

void QChartWidget::onWheel(QWheelEvent* e)
{
    const qreal f = wheelZoomFactor(e->angleDelta().y());
    if (qFuzzyCompare(f, 1.0)) return;
    if (QChartLayer* l = layer2D())
        l->camera()->zoomAtPixel(e->position(), f, m_plotArea);   // 像素 → 数据的换算在相机内
}

// ===== 坐标转换（转发层相机）=====

QPointF QChartWidget::cartesianToPixel(qreal cx, qreal cy) const
{
    if (QChartLayer* l = layer2D()) {
        if (l->camera()) {
            const QChartProjectedPoint pp =
                l->camera()->project(QVector3D(cx, cy, 0.0f), m_plotArea);
            return pp.screen;
        }
    }
    return QPointF(qQNaN(), qQNaN());
}

QPointF QChartWidget::pixelToCartesian(const QPointF& pixel) const
{
    if (QChartLayer* l = layer2D()) {
        if (l->camera()) {
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
    // 边框轴 sizeHint 占用外边距（4a：维度无关钩子——二维层用 axisX/axisY，三维层用其自身轴绑定；
    // 算术与迁移前内联逻辑逐字一致）
    if (m_layer)
        m_layer->borderAxisSizeHint(f, l, t, r, b);

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

// ===== 渲染前：内容指纹注入（视图变化由相机 viewChanged 驱动收集，不再靠指纹）=====

void QChartWidget::onBeforePaint()
{
    if (!m_projection) return;
    // 4g-fix（t58 F4）：内容贡献指纹注入（轴范围/刻度/子刻度/可见性/颜色 + 网格样式 + 数据版本）
    if (QChartLayer* l = layer2D()) {
        quint64 key = 0;
        for (const QChartAxis* a : { l->axisX(), l->axisY() }) {
            if (!a) { key = QChartAbstractLayer::contentHash(key, 0); continue; }
            key = QChartAbstractLayer::contentHashReal(key, a->min());
            key = QChartAbstractLayer::contentHashReal(key, a->max());
            key = QChartAbstractLayer::contentHash(key, quint64(a->tickCount()));
            key = QChartAbstractLayer::contentHash(key, quint64(a->subTickCount()));
            key = QChartAbstractLayer::contentHash(key, a->isVisible() ? 1u : 2u);
            key = QChartAbstractLayer::contentHash(key, quint64(a->color().rgba()));
        }
        key = QChartAbstractLayer::contentHash(key, l->isGridVisible() ? 1u : 2u);
        key = QChartAbstractLayer::contentHash(key, quint64(l->gridColor().rgba()));
        key = QChartAbstractLayer::contentHash(key, l->contentRevision());
        l->setSceneContentState(key);
    }
}

// ===== 像素侧驱动（2D）：plotArea 变化 → layer 相机按 fit 模式适配宽比 =====

void QChartWidget::onPlotAreaChanged(const QRectF& newPlotArea)
{
    if (QChartLayer* l = layer2D())
        l->fitToPlotArea(newPlotArea);   // 相机变化 → 转发 viewChanged → updateAxes
}

// ===== 外部内容：边框轴 drawAtEdge（plotArea 外，CPU/GL 共用）=====

void QChartWidget::drawExternalContent(QPainter& painter)
{
    if (!m_layer || m_plotArea.width() <= 0.0) return;

    DrawContext ctx;
    ctx.plotArea = m_plotArea;
    ctx.dataBounds = dataBounds();   // 4b：按需从层/轴组装（等价于迁移前的缓存值）
    ctx.viewRect = viewRect();
    ctx.projection = m_projection.get();

    painter.save();
    if (QChartLayer* layer = layer2D()) {   // 边框轴 drawAtEdge 属二维专属路径
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
