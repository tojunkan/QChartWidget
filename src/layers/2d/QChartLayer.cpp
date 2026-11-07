// QChartLayer.cpp —— 图层基类实现
#include "QChartLayer.h"
#include "QChartCamera.h"
#include "QChartSeries.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logLayer, "chart.layer")

QChartLayer::QChartLayer(QChartAxis* axisX, QChartAxis* axisY, QObject* parent)
    : QChartAbstractLayer(parent)
{
    // ★ layer = 相机 + 两根轴 + 图元：**轴在构造时绑定，运行期不可更换**（换轴请重建 layer）。
    //   理由：换轴会同时改变 Data→Numeric 映射、range 的权威、相机窗口与外边距，
    //   而且既有系列数据未必还能被新轴映射（分类轴 → 数值轴会得到 NaN）。见 QChartWidget::removeAxis。
    m_axisX = axisX;
    m_axisY = axisY;
    // 轴的一切属性变化（含 rangeChanged 这个"域变了"的信号）→ 轴段重建（脏表 axis 行）。
    // ★ rangeChanged 在这里是**脏表输入**，不是"相机域同步"——那条通路在 widget 侧。
    for (QChartAxis* a : { m_axisX, m_axisY }) {
        if (!a) continue;
        connect(a, &QChartAxis::rangeChanged, this, &QChartLayer::invalidateAxis);
        connect(a, &QChartAxis::visibleChanged, this, &QChartLayer::invalidateAxis);
        connect(a, &QChartAxis::styleChanged, this, &QChartLayer::invalidateAxis);
        connect(a, &QChartAxis::tickCountChanged, this, &QChartLayer::invalidateAxis);
        connect(a, &QChartAxis::subTickCountChanged, this, &QChartLayer::invalidateAxis);
        connect(a, &QChartAxis::alignmentChanged, this, &QChartLayer::invalidateAxis);
    }
    // 网格样式变化 → 轴段重建（网格脊属于轴段）
    connect(this, &QChartLayer::gridChanged, this, &QChartLayer::invalidateAxis);
    // ★ 批次 A：相机归 layer——scene.camera 恒指向本层相机值成员
    //（相机 viewRect 由 widget/viewRect 驱动链或调用方显式设置后再渲染）
    m_scene.camera = &m_camera;
    // 相机变化 → 转发给 widget（setViewRect 期间由 m_rangeTriggered 挡住，防回环）。
    // ★ 相机本身不再直接置脏表：它只负责"域可能变"，真正决定重建的是 rangeChanged → invalidateAxis。
    connect(&m_camera, &QChartCamera::viewChanged, this, [this] {
        if (!m_rangeTriggered) emit viewChanged(m_camera.viewRect());
    });
}
QChartLayer::~QChartLayer() = default;

// ===== 相机窗口（widget 算好 viewRect 后直接给；本类不做 projection 换算）=====
void QChartLayer::setViewRect(const QRectF& viewRect)
{
    m_rangeTriggered = true;
    m_camera.setViewRect(viewRect);
    m_rangeTriggered = false;
}

void QChartLayer::fitToPlotArea(const QRectF& plotArea)
{
    m_camera.fitToPlotArea(plotArea);   // 真·相机变化 → 转发 viewChanged
}

// ===== 轴绑定 =====
// 4b：setNumericBounds(QRectF) 已删除——轴是范围的唯一持有者（范围写入由 QChartWidget
// 直接对轴 setRange 完成）；本类需要范围时临时组装。
QRectF QChartLayer::numericBoundsFromAxes() const
{
    // 正常 QRectF（Y-up）：top = yMin（数值小端）、height > 0；翻转只由相机负责。
    QRectF b;
    if (m_axisX) { b.setLeft(m_axisX->min()); b.setRight(m_axisX->max()); }
    if (m_axisY) { b.setTop(m_axisY->min()); b.setBottom(m_axisY->max()); }
    return b;
}

bool QChartLayer::validateAxes() const {
    if (!m_axisX) {
        qWarning() << "QChartLayer::validateAxes: axisX is null";
        return false;
    }
    if (!m_axisY) {
        qWarning() << "QChartLayer::validateAxes: axisY is null";
        return false;
    }
    return true;
}

// ===== Grid 样式 =====
void QChartLayer::setGridVisible(bool v) {
    if (m_gridVisible == v) return;
    m_gridVisible = v;
    emit gridChanged();
}

void QChartLayer::setGridColor(const QColor& c) {
    if (m_gridColorOverride && *m_gridColorOverride == c) return;
    m_gridColorOverride = c;
    emit gridChanged();
}

// ===== makeToPixel：组装完整坐标变换链 =====
// 同时注入 toNumeric0/toNumeric1 到 DrawContext（Series 画曲线边用）
// std::function<QPointF(QVariant,QVariant)> QChartLayer::makeToPixel(DrawContext& ctx) const {
//     // 注入 Numeric 转换闭包——Series 不需要知道 Axis 类型
//     const_cast<DrawContext&>(ctx).toNumeric0 = [this](QVariant d) -> qreal {
//         return m_axisX ? m_axisX->toNumeric(d) : d.toDouble();
//     };
//     const_cast<DrawContext&>(ctx).toNumeric1 = [this](QVariant d) -> qreal {
//         return m_axisY ? m_axisY->toNumeric(d) : d.toDouble();
//     };
//     // 链: Data(QVariant) → toNumeric → toCartesian → cartesianToPixel
//     // 返回的函数将 Data 空间的 (x,y) 直接映射到 Pixel
//     // Series 不知道 Axis 类型——toNumeric 由 Layer 在此注入
//     return [this, &ctx](QVariant dataX, QVariant dataY) -> QPointF {
//         // Data → Numeric（Axis 负责类型转换：qreal/QDateTime/QString → qreal）
//         qreal num0 = m_axisX ? m_axisX->toNumeric(dataX) : dataX.toDouble();
//         qreal num1 = m_axisY ? m_axisY->toNumeric(dataY) : dataY.toDouble();

//         // NaN check: toNumeric 返回 NaN 表示非法输入
//         if (!std::isfinite(num0) || !std::isfinite(num1))
//             return QPointF(qQNaN(), qQNaN());

//         // Numeric → View Cartesian
//         if (!ctx.projection) return QPointF(qQNaN(), qQNaN());
//         QPointF cartesian = ctx.projection->toCartesian(num0, num1);

//         if (!std::isfinite(cartesian.x()) || !std::isfinite(cartesian.y()))
//             return QPointF(qQNaN(), qQNaN());

//         // View Cartesian → Pixel（线性映射，唯一实现在 QChartCamera2D）
//         return QChartCamera2D::cartesianToPixel(ctx.viewRect, ctx.plotArea,
//                                                 cartesian.x(), cartesian.y());
//     };
// }

// ===== drawGrid：用轴 drawAtPosition 画网格线 =====
// 批次2（A）"二维网格线单标签"：
//   每条网格脊只生成 1 个标签，且以**自由标签**提交（numericAnchor 全 NaN、refPrimitiveId = -1、
//   sourceId = 本脊组号）→ 位置由 renderer 的"同 sourceId 组尾最后可见图元"机制给出（脊尾）。
//   几何仍由扫动轴经 drawAtPosition 提交；标签由图层按"值轴配对"改写文字：
//   水平脊（y=t）文字取 y 轴对 t 的刻度文字，垂直脊（x=t）文字取 x 轴对 t 的刻度文字。
//   "单标签"口径冻结：由轴 QChartAxis::LabelMode::Single 生成——生成下标 = 该脊扫动轴
//   刻度列表下标 size/2（偶数取偏上中位）；该位置文字为空则轴不出标签（本函数随即跳过）。
//   值轴对应刻度文字为空 → 本函数删除该标签（同样不生成）。
//   图层决定哪些脊使用单标签模式：见 gridSpineLabelMode()（默认全部网格脊）。
// ★ 后端差异（既定契约，t31 恢复）：GL（纯 GPU 后端）无能力渲染自由标签 → 二维网格脊标签
//   在 GL 后端不显示（已知缺陷、接受差异）；tier1 显式坐标 / tier2 指向图元两类标签不受影响。
//   未来"混合后端（CPU 前置 projection+裁剪）"的预研旁路入口：
//   QChartRenderer::hybridResolveFreeLabelAnchor —— 当前不启用，正常渲染路径不得调用。

// ===== 边框轴外边距（4a：维度无关布局查询的二维实现）=====
// 与迁移前 QChartWidget::calculatePlotArea 内的内联逻辑逐字一致（Bottom/Top 占上下、Left/Right 占左右；
// HCenter/VCenter（数据主脊）不占外边距）。
void QChartLayer::borderAxisSizeHint(const QFont& font, qreal& left, qreal& top,
                                     qreal& right, qreal& bottom) const
{
    const QChartAxis* axes[2] = { m_axisX, m_axisY };
    for (const QChartAxis* a : axes) {
        if (!a) continue;
        // 隐藏轴不占外边距（历史缺口：原本没判可见性，隐藏轴仍然挤占绘图区）
        if (!a->isVisible()) continue;
        const Qt::Alignment al = a->alignment();
        if (al == Qt::AlignBottom || al == Qt::AlignTop) {
            const qreal h = a->sizeHint(font).height();
            if (al == Qt::AlignBottom) bottom += h; else top += h;
        } else if (al == Qt::AlignLeft || al == Qt::AlignRight) {
            const qreal w = a->sizeHint(font).width();
            if (al == Qt::AlignLeft) left += w; else right += w;
        }
        // HCenter/VCenter（数据主脊）不占外边距
    }
}

// ===== Series 管理（非持有，用户管理生命周期）=====
// 数据/可见性/颜色变化 → 重收集（图元在收集时固化，改样式也必须重收集）
void QChartLayer::addSeries(QChartSeries* s) {
    if (!s || m_series.contains(s)) return;
    m_series.append(s);
    // 系列信号 → 脏表对应行（只有这一行，不再全量置脏）
    connect(s, &QChartSeries::dataChanged, this, [this, s]() { invalidateSeries(s); });
    connect(s, &QChartSeries::visibleChanged, this, [this, s]() { invalidateSeries(s); });
    connect(s, &QChartSeries::colorChanged, this, [this, s]() { invalidateSeries(s); });
    invalidateSeries(s);   // 新增 = 该段需要建
}

void QChartLayer::removeSeries(QChartSeries* s) {
    if (!s || !m_series.removeOne(s)) return;
    disconnect(s, nullptr, this, nullptr);
    m_dirtySeries.remove(s);
    // 段直接移除（归并规则：不在 m_series 里的段一律丢弃）
    for (int i = m_scene.seriesSegments.size() - 1; i >= 0; --i)
        if (m_scene.seriesSegments[i].owner == s) m_scene.seriesSegments.removeAt(i);
    if (scheduleRepaint) scheduleRepaint();
}

void QChartLayer::clearSeries() {
    for (QChartSeries* s : m_series)
        disconnect(s, nullptr, this, nullptr);
    m_series.clear();
    m_dirtySeries.clear();
    m_scene.seriesSegments.clear();
    if (scheduleRepaint) scheduleRepaint();
}

void QChartLayer::collectAxisPrimitives() {
    // 只清轴段：系列段不在本次重建范围内（脏表只标了 axis 行）
    m_scene.axisSegments.clear();
    drawGrid(m_scene);
    QCHART_DIRTY_MSG(QStringLiteral("  collectAxis: rebuilt=%1 段").arg(m_scene.axisSegments.size()));
}

void QChartLayer::collectSeriesPrimitives() {
    int rebuilt = 0;
    // 1) 只重建脏系列对应的段（未脏的段一个字节不动）
    for (QChartSeries* s : m_series) {
        if (!s) continue;
        const bool dirty = m_dirtySeries.contains(s);
        int idx = -1;
        for (int i = 0; i < m_scene.seriesSegments.size(); ++i)
            if (m_scene.seriesSegments[i].owner == s) { idx = i; break; }

        // 契约 S6①：作者意图在收集期过滤——不可见的系列压根不入场景
        if (!s->isVisible()) {
            if (idx >= 0) m_scene.seriesSegments.removeAt(idx);
            continue;
        }
        if (!dirty && idx >= 0) continue;   // 未脏且已有段 → 保持原样

        QCHART_DIRTY_MSG(QStringLiteral("  series<'%1'> 被重新收集").arg(s->name()));
        QChartSegment seg;
        seg.owner = s;
        s->collectPrimitives(seg, m_axisX, m_axisY);
        if (seg.prims.isEmpty()) {          // 空段丢弃（不产生"零图元段"）
            if (idx >= 0) m_scene.seriesSegments.removeAt(idx);
            continue;
        }
        if (idx >= 0) m_scene.seriesSegments[idx] = seg;
        else m_scene.seriesSegments.append(seg);
        ++rebuilt;
    }

    // 2) 按 m_series 顺序归并：缺的补、多余的删、顺序对齐（增删系列走同一条路）
    QVector<QChartSegment> ordered;
    ordered.reserve(m_scene.seriesSegments.size());
    for (QChartSeries* s : m_series) {
        for (const QChartSegment& seg : m_scene.seriesSegments)
            if (seg.owner == s) { ordered.append(seg); break; }
    }
    m_scene.seriesSegments = ordered;
    QCHART_DIRTY_MSG(QStringLiteral("  collectSeries: rebuilt=%1 / %2 段")
                         .arg(rebuilt).arg(m_scene.seriesSegments.size()));
}

void QChartLayer::drawGrid(QChartScene& scene) {
    if (!m_gridVisible) return;
    if (!m_axisX || !m_axisY ) return;

    const QChartAxis::LabelMode spineLabelMode = gridSpineLabelMode();

    // ----- 辅助 lambda：添加一条网格脊（一脊 = 一段）-----
    auto addSpine = [&](QChartAxis* scanAxis, int dimIndex, qreal dimMin, qreal dimMax,
                        qreal off0, qreal off1, const QString& valueText, QChartScene& out) {
        if (!scanAxis) return;
        QChartSegment seg;
        seg.owner = scanAxis;
        // 几何 + 轴侧单标签（Single：下标 size/2 代表标签；该位置文字为空则轴不出标签）
        scanAxis->drawAtPosition(dimMin, dimMax, off0, off1, dimIndex, seg, spineLabelMode);
        // 契约 S2：**先产图元，再接纳段**。
        // 历史坑：这里原本是"先发组号再调 drawAtPosition"，而 drawAtPosition 对不可见轴会立即
        // return，于是号已发出、图元一个没有 → 留下零图元段。空段一律丢弃。
        if (seg.prims.isEmpty()) return;
        for (auto& prim : seg.prims) {
            prim.color = gridColor();
            prim.penWidth = 1.0;
        }

        // 单标签改写仅对"单标签"策略生效：
        //   None → 轴侧不出标签；Tickwise → 保留轴侧逐刻度标签（旧语义，不改写）
        if (spineLabelMode == QChartAxis::LabelMode::Single && !seg.labels.isEmpty()) {
            // 改写为自由标签：文字取固定坐标所属值轴；值轴文字为空则不生成
            if (valueText.isEmpty()) {
                seg.labels.clear();
            } else {
                QChartTextLabel& lbl = seg.labels[0];
                lbl.text = valueText;
                lbl.numericAnchor = QChartTextLabel{}.numericAnchor;   // 全 NaN 哨兵：无显式锚点
                lbl.segPrim = -1;                                      // 自由标签（不指向图元）
            }
        }
        out.axisSegments.append(seg);
    };

    // 范围直接用轴自身（与 drawAtEdge 同源；不再经 numericBoundsFromAxes 的矩形取向中转）
    const qreal xMin = m_axisX->min();
    const qreal xMax = m_axisX->max();
    const qreal yMin = m_axisY->min();
    const qreal yMax = m_axisY->max();

    // ── 水平网格脊：dim0 扫动、固定 y = y 轴刻度（文字取 y 轴对该刻度的文字）──
    const QVector<qreal> ticksY = m_axisY->tickValues(yMin, yMax);
    const QStringList labelsY = m_axisY->tickLabels(ticksY);

    for (int i = 0; i < ticksY.size(); ++i) {
        const qreal tickVal = ticksY[i];
        addSpine(m_axisX, 0, xMin, xMax,
                 tickVal, tickVal, labelsY.value(i), scene);
    }

    // ── 垂直网格脊：dim1 扫动、固定 x = x 轴刻度（文字取 x 轴对该刻度的文字）──
    const QVector<qreal> ticksX = m_axisX->tickValues(xMin, xMax);
    const QStringList labelsX = m_axisX->tickLabels(ticksX);

    for (int i = 0; i < ticksX.size(); ++i) {
        const qreal tickVal = ticksX[i];
        addSpine(m_axisY, 1, yMin, yMax,
                 tickVal, tickVal, labelsX.value(i), scene);
    }
}

// ===== Series 管理（S0：随 QChartSeries.cpp 一起在 Series 阶段恢复）=====
// void QChartLayer::addSeries(QChartSeries* s) { ... }
// void QChartLayer::removeSeries(QChartSeries* s) { ... }
// void QChartLayer::clearSeries() { ... }
// void QChartLayer::hookSeriesDirty(QChartSeries* s) { ... }
// void QChartLayer::unhookSeriesDirty(QChartSeries* s) { ... }

// ===== drawAllSeries =====
// void QChartLayer::drawAllSeries(QPainter* painter, const DrawContext& ctx) {
//     if (!validateAxes()) {
//         qWarning() << "QChartLayer::drawAllSeries: axes not valid, aborting";
//         return;
//     }

//     auto toPixel = makeToPixel(const_cast<DrawContext&>(ctx));

//     for (auto* s : m_series) {
//         if (!s || !s->isVisible()) continue;
//         painter->save();
//         painter->setOpacity(s->opacity());
//         s->draw(painter, toPixel, &ctx);
//         painter->restore();
//     }
// }

void QChartLayer::recomputeDataBounds() {
    
}

// ===== 命中检测（Phase 3 任务 0：拾取整体后置）=====
// QChartLayer::HitResult QChartLayer::hitTest(const QPointF& pixel,
//                                                    const DrawContext& ctx) const {
//     // S0：旧实现调用已删除的 makeToPixel（依赖旧 DrawContext/QChartCamera2D 静态接口）。
//     // 拾取后置；恢复时经 QChartHitTester::hitTest + 现行 projection/camera 重新接入。
//     Q_UNUSED(pixel); Q_UNUSED(ctx);
//     return {};
// }
