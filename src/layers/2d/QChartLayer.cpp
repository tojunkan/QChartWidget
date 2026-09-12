// QChartLayer.cpp —— 图层基类实现
#include "QChartLayer.h"
#include "QChartCamera.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logLayer, "chart.layer")

QChartLayer::QChartLayer(QObject* parent) : QChartAbstractLayer(parent) {
    connect(this, &QChartLayer::gridChanged, this, &QChartLayer::invalidateData);
    // ★ 批次 A：相机归 layer——scene.camera 恒指向本层相机值成员
    //（相机 viewRect 由 widget/viewRect 驱动链或调用方显式设置后再渲染）
    m_scene.camera = &m_camera;
    // S0：QChartWidget/QChartAbstractWidget 尚未纳入本阶段子集，原「父对象为
    // QChartAbstractWidget 时自动注入 plotArea/projection」分支随 Widget 阶段一并恢复；
    // series 信号连接待 Series 阶段随 QChartSeries.cpp 恢复。
}
QChartLayer::~QChartLayer() = default;

// ===== 轴绑定 =====
// 4b：setNumericBounds(QRectF) 已删除——轴是范围的唯一持有者（范围写入由 QChartWidget
// 在 recomputeDataBounds/onBeforePaint 中直接对轴 setRange 完成）；本类需要范围时临时组装。
QRectF QChartLayer::numericBoundsFromAxes() const
{
    QRectF b;   // 默认 (0,0,0,0)：未绑定/未设置的维度保持 0（与迁移前 m_dataBounds 初值一致）
    if (m_axisX) { b.setLeft(m_axisX->min()); b.setRight(m_axisX->max()); }
    if (m_axisY) { b.setBottom(m_axisY->min()); b.setTop(m_axisY->max()); }
    return b;
}

void QChartLayer::setAxisX(QChartAxis* a) {
    m_axisX = a;   // 4b：不再维护 m_dataBounds 缓存（范围由轴持有，按需组装）
    qCDebug(logLayer) << "setAxisX:" << (a ? "set" : "null");
}

void QChartLayer::setAxisY(QChartAxis* a) {
    m_axisY = a;
    qCDebug(logLayer) << "setAxisY:" << (a ? "set" : "null");
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

void QChartLayer::collectPrimitives() {
    // 批次 A：每次收集前复位场景负载（widget 每帧重收集；sourceId 从 0 起重新分配）
    m_scene.primitives.clear();
    m_scene.labels.clear();
    m_scene.maxSourceId = 0;
    m_scene.PrimitiveIdPrefixSum.clear();
    m_scene.PrimitiveIdPrefixSum.append(0);
    // ---- 绘制网格（S0：Series 阶段前只收集网格；drawAllSeries 届时恢复）----
    drawGrid(m_scene);
}

void QChartLayer::drawGrid(QChartScene& scene) {
    if (!m_gridVisible) return;
    if (!m_axisX || !m_axisY ) return;

    const int segments = m_scene.projection ? m_scene.projection->samplingSegmentsHint() : 72;
    const QChartAxis::LabelMode spineLabelMode = gridSpineLabelMode();

    // ----- 辅助 lambda：添加一条网格脊（几何 + 单标签改写）-----
    auto addSpine = [&](QChartAxis* scanAxis, int dimIndex, qreal dimMin, qreal dimMax,
                        qreal off0, qreal off1, const QString& valueText, QChartScene& out) {
        if (!scanAxis) return;
        const int primBefore = out.primitives.size();
        const int labelBefore = out.labels.size();
        out.maxSourceId++;
        const int groupId = out.maxSourceId;
        // 几何 + 轴侧单标签（Single：下标 size/2 代表标签；该位置文字为空则轴不出标签）
        scanAxis->drawAtPosition(dimMin, dimMax, off0, off1, dimIndex,
                                 out, segments, spineLabelMode);
        for (int i = primBefore; i < out.primitives.size(); ++i) {
            auto& prim = out.primitives[i];
            prim.color = gridColor();
            prim.penWidth = 1.0;
            prim.sourceId = groupId;
        }
        out.PrimitiveIdPrefixSum.push_back(out.primitives.size());

        // 单标签改写仅对"单标签"策略生效：
        //   None → 轴侧不出标签；Tickwise → 保留轴侧逐刻度标签（旧语义，不改写）
        if (spineLabelMode != QChartAxis::LabelMode::Single) return;
        if (out.labels.size() <= labelBefore) return;   // 轴侧未生成（该位置文字为空）

        // 改写为自由标签：文字取固定坐标所属值轴；值轴文字为空则不生成
        QChartTextLabel& lbl = out.labels[labelBefore];
        if (valueText.isEmpty()) {
            out.labels.removeAt(labelBefore);
            return;
        }
        lbl.text = valueText;
        lbl.numericAnchor = QChartTextLabel{}.numericAnchor;   // 全 NaN 哨兵：无显式锚点
        lbl.refPrimitiveId = -1;                               // 自由标签（不指向图元）
        lbl.sourceId = groupId;                                // 同组组尾可见图元定位
    };

    // 4b：范围按需从轴临时组装（数值与迁移前 m_dataBounds 逐位一致），用完即弃
    const QRectF bounds = numericBoundsFromAxes();

    // ── 水平网格脊：dim0 扫动、固定 y = y 轴刻度（文字取 y 轴对该刻度的文字）──
    const QVector<qreal> ticksY = m_axisY->tickValues(bounds.bottom(), bounds.top());
    const QStringList labelsY = m_axisY->tickLabels(ticksY);

    for (int i = 0; i < ticksY.size(); ++i) {
        const qreal tickVal = ticksY[i];
        addSpine(m_axisX, 0, bounds.left(), bounds.right(),
                 tickVal, tickVal, labelsY.value(i), scene);
    }

    // ── 垂直网格脊：dim1 扫动、固定 x = x 轴刻度（文字取 x 轴对该刻度的文字）──
    const QVector<qreal> ticksX = m_axisX->tickValues(bounds.left(), bounds.right());
    const QStringList labelsX = m_axisX->tickLabels(ticksX);

    for (int i = 0; i < ticksX.size(); ++i) {
        const qreal tickVal = ticksX[i];
        addSpine(m_axisY, 1, bounds.bottom(), bounds.top(),
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
