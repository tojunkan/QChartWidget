// QChartLayer.cpp —— 图层基类实现
#include "QChartLayer.h"
#include "QChartCamera.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logLayer, "chart.layer")

QChartLayer::QChartLayer(QObject* parent) : QObject(parent) {
    connect(this, &QChartLayer::gridChanged, this, &QChartLayer::invalidateData);
    // S0：QChartWidget/QChartAbstractWidget 尚未纳入本阶段子集，原「父对象为
    // QChartAbstractWidget 时自动注入 plotArea/projection」分支随 Widget 阶段一并恢复；
    // series 信号连接待 Series 阶段随 QChartSeries.cpp 恢复。
}
QChartLayer::~QChartLayer() = default;

// ===== 轴绑定 =====
void QChartLayer::setAxisX(QChartAxis* a) {
    m_axisX = a;
    qCDebug(logLayer) << "setAxisX:" << (a ? "set" : "null");
    if(m_axisX) {
        m_dataBounds.setLeft(m_axisX->min());
        m_dataBounds.setRight(m_axisX->max());
    }
}

void QChartLayer::setAxisY(QChartAxis* a) {
    m_axisY = a;
    qCDebug(logLayer) << "setAxisY:" << (a ? "set" : "null");
    if(m_axisY) {
        m_dataBounds.setTop(m_axisY->max());
        m_dataBounds.setBottom(m_axisY->min());
    }
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
// QChartLayer.cpp —— drawGrid 修改后

void QChartLayer::collectPrimitives() {
    // ---- 绘制网格（S0：Series 阶段前只收集网格；drawAllSeries 届时恢复）----
    drawGrid(m_scene);
}

void QChartLayer::drawGrid(QChartScene& scene) {
    if (!m_gridVisible) return;
    if (!m_axisX || !m_axisY ) return;

    const int segments = m_scene.projection ? m_scene.projection->samplingSegmentsHint() : 72;

    // ----- 辅助 lambda：添加一条线段（调用 drawAtPosition，generateTicks=false）-----
    auto addLine = [&](QChartAxis* axis, int dimIndex, qreal dimMin, qreal dimMax,
                        qreal off0, qreal off1, bool drawLabels, const QColor& color, qreal penWidth, QChartScene& out) {
        if (!axis) return;
        // int segments = m_scene.projection3D ? m_scene.projection3D->samplingSegmentsHint() : 72;
        int cnt = out.primitives.size();
        out.maxSourceId++;
        axis->drawAtPosition(dimMin, dimMax, off0, off1, dimIndex,
                                out, segments, drawLabels);
        for (int i = cnt; i < out.primitives.size(); ++i) {
            auto& prim = out.primitives[i];
            prim.color = color;
            prim.penWidth = penWidth;
            prim.sourceId = out.maxSourceId;
        }
        out.PrimitiveIdPrefixSum.push_back(out.primitives.size());
    };


    // ── 画 dim0 方向的网格线（垂直数据主脊）：dim0=扫, dim1=tick ──
    QVector<qreal> ticksY = m_axisY->tickValues(m_dataBounds.bottom(), m_dataBounds.top());

    for (int i = 0; i < ticksY.size(); ++i) {
        qreal tickVal = ticksY[i];
        addLine(m_axisX, 0, m_dataBounds.left(), m_dataBounds.right(),
                tickVal, tickVal, /*drawLabels=*/true,
                gridColor(), 1.0, scene);
    }

    // ── 画 dim1 方向的网格线（水平数据主脊）：dim1=扫, dim0=tick ──
    QVector<qreal> ticksX = m_axisX->tickValues(m_dataBounds.left(), m_dataBounds.right());

    for (int i = 0; i < ticksX.size(); ++i) {
        qreal tickVal = ticksX[i];
        addLine(m_axisY, 1, m_dataBounds.bottom(), m_dataBounds.top(),
                tickVal, tickVal, /*drawLabels=*/true,
                gridColor(), 1.0, scene);
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
