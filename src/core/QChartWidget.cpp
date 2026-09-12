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
    // 4a：便捷轴绑定只对二维层生效（三维层无 axisX/axisY 语义；迁移前 3D 层虽继承二维层，但三维
    // widget 的 m_axes 恒为空 → 本分支对其等价于空操作，行为不变）
    QChartLayer* l2 = dynamic_cast<QChartLayer*>(layer);
    if (l2 && m_layers.size() == 1) {
        // 便捷轴挂到首个图层（多图层时其余图层轴由调用方经 layer->setAxisX/Y 自绑）
        for (QChartAxis* a : m_axes) {
            if (!isAxisAttached(a)) attachToLayer(l2, a);
        }
        markNumericDirty();   // 4e：图层/轴绑定变化 → 数值侧待 fit（渲染前同步相机）
    }
}

void QChartWidget::removeLayer(QChartAbstractLayer* layer)
{
    QChartAbstractWidget::removeLayer(layer);
    if (m_layers.isEmpty())
        markNumericDirty();   // 4e：无层 → 数值侧待 fit（保持既有触发形状）
}

// 4a：二维层视图（动态转换筛选；列表内无三维层时与旧「直接遍历 m_layers」逐位等价）
QList<QChartLayer*> QChartWidget::layers2D() const
{
    QList<QChartLayer*> out;
    for (QChartAbstractLayer* l : m_layers)
        if (auto* l2 = dynamic_cast<QChartLayer*>(l)) out.append(l2);
    return out;
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
        for (QChartLayer* l : layers2D())
            if (l) l->invalidateData();
        scheduleRepaint();
    };
    connect(a, &QChartAxis::tickCountChanged, this, [markContentDirty]() { markContentDirty(); });
    connect(a, &QChartAxis::subTickCountChanged, this, [markContentDirty]() { markContentDirty(); });
    connect(a, &QChartAxis::styleChanged, this, [markContentDirty]() { markContentDirty(); });
    connect(a, &QChartAxis::visibleChanged, this, [markContentDirty]() { markContentDirty(); });
    const QList<QChartLayer*> ls = layers2D();
    if (!ls.isEmpty()) {
        attachToLayer(ls.first(), a);
        markNumericDirty();
    }
}

void QChartWidget::removeAxis(QChartAxis* a)
{
    disconnect(a, nullptr, this, nullptr);   // 4e：解除范围变化监听（轴非持有，防悬挂连接）
    m_axes.removeAll(a);
    for (QChartLayer* l : layers2D()) {
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
    for (QChartLayer* l : layers2D()) {
        if (l->axisX() == a || l->axisY() == a) return true;
    }
    return false;
}

// ===== Projection =====

void QChartWidget::setProjection(std::unique_ptr<QChartProjection> proj)
{
    if (!proj) return;
    m_projection = std::move(proj);
    markNumericDirty();   // 4e：投影变化 = 数值侧驱动（轴范围需重新映射到相机窗口）
    emit projectionChanged(m_projection.get());
    m_layoutDirty = true;
    scheduleRepaint();
}

// ===== viewRect/dataBounds 驱动链（4b：轴是范围的唯一持有者，dataBounds 按需临时组装）=====

QRectF QChartWidget::dataBounds() const
{
    // 按需从首个二维层的轴范围组装（legacy 取向：left/right=dim0、bottom/top=dim1；无层/无轴→空）
    for (QChartLayer* l : layers2D()) {
        if (l && l->axisX() && l->axisY())
            return l->numericBoundsFromAxes();
    }
    return QRectF();
}

QRectF QChartWidget::viewRect() const
{
    for (QChartLayer* l : layers2D()) {
        if (l && l->camera() && l->camera()->viewRect().width() > 0.0)
            return l->camera()->viewRect();
    }
    return QRectF();
}

void QChartWidget::setViewRect(const QRectF& r)
{
    if (r.width() <= 0.0 || r.height() <= 0.0) return;
    for (QChartLayer* l : layers2D())
        if (l) l->camera()->setViewRect(r);
    // 4e：显式相机窗口操作 → 相机成为真值来源：覆盖先前数值侧请求，并立即反算写轴
    m_numericDirty = false;
    m_cameraDirty = true;
    recomputeDataBounds();
}

// ===== 4e：驱动链助手（方向状态驱动；写轴带抑制深度，防 rangeChanged 回环）=====

void QChartWidget::onAxisRangeChanged(qreal min, qreal max)
{
    Q_UNUSED(min); Q_UNUSED(max);
    if (m_axisWriteDepth > 0) return;   // 本类写轴（反算/fit 回写）不得反过来触发数值侧 fit
    markNumericDirty();
}

void QChartWidget::markNumericDirty()
{
    m_numericDirty = true;
    scheduleRepaint();
}

void QChartWidget::writeAxisRanges(const QRectF& legacy)
{
    // 写回各轴范围（轴是范围的唯一持有者；守卫与迁移前 layer->setNumericBounds 内的判定逐字一致：
    // X=left→right、Y=bottom→top，数值增序；legacy 取向 rect 的 top>bottom）
    ++m_axisWriteDepth;                      // 4e：抑制 rangeChanged → 数值侧脏（防回环）
    for (QChartLayer* l : layers2D()) {
        if (!l) continue;
        if (l->axisX() && legacy.left() <= legacy.right())
            l->axisX()->setRange(legacy.left(), legacy.right());
        if (l->axisY() && legacy.bottom() <= legacy.top())
            l->axisY()->setRange(legacy.bottom(), legacy.top());
    }
    --m_axisWriteDepth;
}

bool QChartWidget::backCalcAxesFromCameraWindow()
{
    // 相机侧反算：相机窗口 → projection.computeDataBounds → 临时 legacy dataBounds（用完即弃）→ 写轴
    if (!m_projection) return false;
    const QRectF vr = viewRect();
    if (vr.width() <= 0.0 || vr.height() <= 0.0) return false;

    const QRectF math = m_projection->computeDataBounds(vr);
    QRectF legacy;
    legacy.setLeft(math.left());
    legacy.setRight(math.left() + math.width());
    legacy.setBottom(math.top());                 // dim1 数值 min
    legacy.setTop(math.top() + math.height());    // dim1 数值 max
    writeAxisRanges(legacy);
    ++m_backCalcCount;
    scheduleRepaint();
    return true;
}

bool QChartWidget::applyNumericFit()
{
    // 数值侧 fit：轴范围 → 数值域 → projection.computeViewRect → 相机窗口 → 以**实际可见范围**回写轴
    // （不另存请求值：轴即可见范围持有者；线性投影下回写值 == 入参，非线性投影下为真实可见范围）
    if (!m_projection) return false;
    QChartLayer* source = nullptr;
    for (QChartLayer* l : layers2D()) {
        if (l && l->axisX() && l->axisY()) { source = l; break; }
    }
    if (!source) return false;

    const qreal x0 = source->axisX()->min(), x1 = source->axisX()->max();
    const qreal y0 = source->axisY()->min(), y1 = source->axisY()->max();
    if (x1 <= x0 || y1 <= y0) return false;   // 退化范围（含 0,0 未设置）：不 fit，保持相机窗口与轴不动

    QRectF legacy;
    legacy.setLeft(x0); legacy.setRight(x1);
    legacy.setBottom(y0); legacy.setTop(y1);

    // 数值域(math) → Cartesian 视图窗
    const QRectF mathRect(legacy.left(), legacy.bottom(),
                          legacy.right() - legacy.left(),
                          legacy.top() - legacy.bottom());
    const QRectF cart = m_projection->computeViewRect(mathRect);
    for (QChartLayer* l : layers2D())
        if (l) l->camera()->setViewRect(cart);

    ++m_fitCount;
    return backCalcAxesFromCameraWindow();   // 以 fit 结果的实际可见范围回写轴（线性下为幂等写）
}

void QChartWidget::recomputeDataBounds()
{
    m_cameraDirty = false;                                  // 显式反算即消费相机侧脏标记
    if (backCalcAxesFromCameraWindow())
        m_numericDirty = false;                             // 相机侧反算覆盖待 fit 请求（相机为真值来源）
}

void QChartWidget::panViewCartesian(qreal dx, qreal dy)
{
    for (QChartLayer* l : layers2D())
        if (l) l->camera()->panViewCartesian(dx, dy);
    m_numericDirty = false;   // 4e：相机侧操作覆盖待 fit 请求
    recomputeDataBounds();
}

void QChartWidget::zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY)
{
    for (QChartLayer* l : layers2D())
        if (l) l->camera()->zoomViewCartesian(cx, cy, factorX, factorY);
    m_numericDirty = false;   // 4e：相机侧操作覆盖待 fit 请求
    recomputeDataBounds();
}

// ===== 4f：鼠标交互接线（像素 → 视图坐标；全部经既有相机/视窗入口，不改 4a–4e 机制）=====

// 像素位移 → 视图坐标位移：比例 = 视图尺寸 / plotArea 尺寸；方向取反（“抓住内容拖动”，
// 且二维相机 Y 轴在屏幕上是翻转的：cart.y 增大 → 屏幕向上，故 Y 位移取正号）。
QPointF QChartWidget::pixelDeltaToViewDelta(const QPointF& pixelDelta) const
{
    const QRectF pa = m_plotArea;
    const QRectF vr = viewRect();
    if (pa.width() <= 0.0 || pa.height() <= 0.0 || vr.width() <= 0.0 || vr.height() <= 0.0)
        return QPointF(0.0, 0.0);
    return QPointF(-pixelDelta.x() * vr.width() / pa.width(),
                   pixelDelta.y() * vr.height() / pa.height());
}

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
    const QPointF viewDelta = pixelDeltaToViewDelta(pos - m_lastPixel);
    m_lastPixel = pos;
    if (viewDelta.isNull()) return;
    panViewCartesian(viewDelta.x(), viewDelta.y());   // 既有入口（相机侧反算链；绝不 fit）
}

void QChartWidget::onMouseRelease(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) m_panning = false;
}

void QChartWidget::onWheel(QWheelEvent* e)
{
    const qreal f = wheelZoomFactor(e->angleDelta().y());
    if (qFuzzyCompare(f, 1.0)) return;
    const QPointF center = pixelToCartesian(e->position());   // 光标处数据坐标（缩放中心）
    if (!std::isfinite(center.x()) || !std::isfinite(center.y())) return;
    zoomViewCartesian(center.x(), center.y(), f, f);          // 既有入口（相机侧反算链；绝不 fit）
}

// ===== 坐标转换（转发层相机）=====

QPointF QChartWidget::cartesianToPixel(qreal cx, qreal cy) const
{
    for (QChartLayer* l : layers2D()) {
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
    for (QChartLayer* l : layers2D()) {
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
    // 边框轴 sizeHint 占用外边距（4a：维度无关钩子——二维层用 axisX/axisY，三维层用其自身轴绑定；
    // 算术与迁移前内联逻辑逐字一致）
    for (QChartAbstractLayer* layer : m_layers) {
        if (!layer) continue;
        layer->borderAxisSizeHint(f, l, t, r, b);
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

// ===== 4e：渲染前驱动链收口（方向状态，取代 4c 的一次性闩锁 m_viewSynced）=====
//  ①相机侧脏（相机窗口变化）→ 反算写轴（绝不 fit，防回环）；
//  ②数值侧脏（轴范围/投影/绑定变化）→ fit（轴范围 → 相机窗口）+ 以实际可见范围回写轴；
//  两者互不触发（writeAxisRanges 带抑制深度）→ 无回环、幂等；重复同状态不产生额外重算。

void QChartWidget::onBeforePaint()
{
    if (!m_projection) return;
    if (m_cameraDirty) {
        m_cameraDirty = false;
        if (backCalcAxesFromCameraWindow())
            m_numericDirty = false;
    }
    if (m_numericDirty) {
        m_numericDirty = false;
        if (!applyNumericFit()) {
            // 4g-fix（t58 F2）：退化范围/未 fit（相机窗口未变 → 视图指纹不变）时轴状态仍已变化，
            // 必须显式置脏——否则绘图区内网格/刻度保持陈旧，而边框轴外带已更新（内外不一致）。
            for (QChartLayer* l : layers2D())
                if (l) l->invalidateData();
        }
    }
    // 4g-fix（t58 F4）：内容贡献指纹注入（轴范围/刻度/子刻度/可见性/颜色 + 网格样式 + 数据版本）
    for (QChartLayer* l : layers2D()) {
        if (!l) continue;
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

// ===== 4e：像素侧驱动（2D）——plotArea 变化 → 相机 fit 模式适配宽比 → 相机侧反算 =====

void QChartWidget::onPlotAreaChanged(const QRectF& newPlotArea)
{
    // 2D 的“重 fit”= 相机按既有 fit 模式（Stretch/Preserve/Expand/Crop）适配新 plotArea 宽比：
    //  · Stretch（拉伸铺满；Cartesian 常态，见旧 demo_bar 用法）→ QChartCamera::fitToPlotArea 内部直接返回
    //    false（不触碰窗口）→ 零视觉变化、零额外重算；
    //  · Preserve/Expand/Crop → 按模式与 fitStrategy 调整窗口，随即转相机侧反算（渲染前写轴，绝不 fit）。
    // 窗口未变（含零尺寸/等宽比）时本钩子完全无副作用 → 幂等。
    bool windowChanged = false;
    for (QChartLayer* l : layers2D()) {
        if (!l || !l->camera()) continue;
        if (l->camera()->fitToPlotArea(newPlotArea)) windowChanged = true;
    }
    if (!windowChanged) return;
    m_numericDirty = false;   // 窗口已按模式适配 → 相机为真值来源，覆盖待 fit 请求
    m_cameraDirty = true;     // → 渲染前反算写轴（不触发 fit）
    scheduleRepaint();
}

// ===== 外部内容：边框轴 drawAtEdge（plotArea 外，CPU/GL 共用）=====

void QChartWidget::drawExternalContent(QPainter& painter)
{
    if (m_layers.isEmpty() || m_plotArea.width() <= 0.0) return;

    DrawContext ctx;
    ctx.plotArea = m_plotArea;
    ctx.dataBounds = dataBounds();   // 4b：按需从层/轴组装（等价于迁移前的缓存值）
    ctx.viewRect = viewRect();
    ctx.projection = m_projection.get();

    painter.save();
    for (QChartLayer* layer : layers2D()) {   // 边框轴 drawAtEdge 属二维专属路径
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
