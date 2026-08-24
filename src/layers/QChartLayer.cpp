// QChartLayer.cpp —— 图层基类实现
#include "QChartLayer.h"
#include "QChartWidget.h"
#include "QChartCamera.h"
#include "QChartSeries.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logLayer, "chart.layer")

QChartLayer::QChartLayer(QObject* parent) : QObject(parent) {}
QChartLayer::~QChartLayer() { qDeleteAll(m_series); }

// ===== 轴绑定 =====
void QChartLayer::setAxisX(QChartAxis* a) {
    m_axisX = a;
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
std::function<QPointF(QVariant,QVariant)> QChartLayer::makeToPixel(DrawContext& ctx) const {
    // 注入 Numeric 转换闭包——Series 不需要知道 Axis 类型
    const_cast<DrawContext&>(ctx).toNumeric0 = [this](QVariant d) -> qreal {
        return m_axisX ? m_axisX->toNumeric(d) : d.toDouble();
    };
    const_cast<DrawContext&>(ctx).toNumeric1 = [this](QVariant d) -> qreal {
        return m_axisY ? m_axisY->toNumeric(d) : d.toDouble();
    };
    // 链: Data(QVariant) → toNumeric → toCartesian → cartesianToPixel
    // 返回的函数将 Data 空间的 (x,y) 直接映射到 Pixel
    // Series 不知道 Axis 类型——toNumeric 由 Layer 在此注入
    return [this, &ctx](QVariant dataX, QVariant dataY) -> QPointF {
        // Data → Numeric（Axis 负责类型转换：qreal/QDateTime/QString → qreal）
        qreal num0 = m_axisX ? m_axisX->toNumeric(dataX) : dataX.toDouble();
        qreal num1 = m_axisY ? m_axisY->toNumeric(dataY) : dataY.toDouble();

        // NaN check: toNumeric 返回 NaN 表示非法输入
        if (!std::isfinite(num0) || !std::isfinite(num1))
            return QPointF(qQNaN(), qQNaN());

        // Numeric → View Cartesian
        if (!ctx.projection) return QPointF(qQNaN(), qQNaN());
        QPointF cartesian = ctx.projection->toCartesian(num0, num1);

        if (!std::isfinite(cartesian.x()) || !std::isfinite(cartesian.y()))
            return QPointF(qQNaN(), qQNaN());

        // View Cartesian → Pixel（线性映射，唯一实现在 QChartCamera2D）
        return QChartCamera2D::cartesianToPixel(ctx.viewRect, ctx.plotArea,
                                                cartesian.x(), cartesian.y());
    };
}

// ===== drawGrid：用轴 drawAtPosition 画网格线 =====
// QChartLayer.cpp —— drawGrid 修改后

void QChartLayer::drawGrid(QPainter* painter, const DrawContext& ctx) const {
    if (!m_gridVisible) return;
    if (!m_axisX || !m_axisY || !ctx.projection)
        return;

    qCDebug(logLayer) << "drawGrid: dataBounds=" << ctx.dataBounds
        << "viewRect=" << ctx.viewRect;

    painter->save();

    // 获取 dataBounds 对应的 Numeric 范围
    qreal dim0Min = ctx.dataBounds.left();
    qreal dim0Max = ctx.dataBounds.left() + ctx.dataBounds.width();
    qreal dim1Min = ctx.dataBounds.top();
    qreal dim1Max = ctx.dataBounds.top() + ctx.dataBounds.height();

    // ── 画 dim0 方向的网格线（垂直数据主脊）：dim0=扫, dim1=tick ──
    QVector<qreal> ticksY = m_axisY->tickValues(dim1Min, dim1Max);
    QStringList labelsY = m_axisY->tickLabels(ticksY);  // 提前获取完整标签列表

    QPen gridPen;
    for (int i = 0; i < ticksY.size(); ++i) {
        qreal tickVal = ticksY[i];
        QString label = labelsY.value(i);               // 直接取对应标签
        if (i % 2 == 0) {
            gridPen = QPen(gridColor(), 1.0, Qt::DashLine);
            m_axisX->drawAtPosition(painter, ctx, tickVal,
                /*axisLine=*/true, /*labels=*/true, /*ticks=*/true,
                /*label=*/label, /*pen=*/&gridPen);
        }
        else {
            gridPen = QPen(gridColor(), 1.0, Qt::SolidLine);
            m_axisX->drawAtPosition(painter, ctx, tickVal,
                /*axisLine=*/true, /*labels=*/true, /*ticks=*/true,
                /*label=*/label, /*pen=*/&gridPen);
        }
    }

    // ── 画 dim1 方向的网格线（水平数据主脊）：dim1=扫, dim0=tick ──
    QVector<qreal> ticksX = m_axisX->tickValues(dim0Min, dim0Max);
    QStringList labelsX = m_axisX->tickLabels(ticksX);  // 提前获取完整标签列表

    for (int i = 0; i < ticksX.size(); ++i) {
        qreal tickVal = ticksX[i];
        QString label = labelsX.value(i);
        if (i % 2 == 0) {
            gridPen = QPen(gridColor(), 1.0, Qt::DashLine);
            m_axisY->drawAtPosition(painter, ctx, tickVal,
                /*axisLine=*/true, /*labels=*/true, /*ticks=*/true,
                /*label=*/label, /*pen=*/&gridPen);
        }
        else {
            gridPen = QPen(gridColor(), 1.0, Qt::SolidLine);
            m_axisY->drawAtPosition(painter, ctx, tickVal,
                /*axisLine=*/true, /*labels=*/true, /*ticks=*/true,
                /*label=*/label, /*pen=*/&gridPen);
        }
    }

    painter->restore();
}

// ===== Series 管理 =====
void QChartLayer::addSeries(QChartSeries* s) {
    if (!s) return;
    s->setParent(this);
    m_series.append(s);
    emit seriesAdded(s);
}

void QChartLayer::removeSeries(QChartSeries* s) {
    if (m_series.removeAll(s)) {
        emit seriesRemoved(s);
        delete s;
    }
}

void QChartLayer::clearSeries() {
    qDeleteAll(m_series);
    m_series.clear();
}

// ===== drawAllSeries =====
void QChartLayer::drawAllSeries(QPainter* painter, const DrawContext& ctx) {
    if (!validateAxes()) {
        qWarning() << "QChartLayer::drawAllSeries: axes not valid, aborting";
        return;
    }

    auto toPixel = makeToPixel(const_cast<DrawContext&>(ctx));

    for (auto* s : m_series) {
        if (!s || !s->isVisible()) continue;
        painter->save();
        painter->setOpacity(s->opacity());
        s->draw(painter, toPixel, &ctx);
        painter->restore();
    }
}

// ===== 命中检测（Phase 3 任务 0：逻辑委托 QChartHitTester，行为零变化）=====
QChartLayer::HitResult QChartLayer::hitTest(const QPointF& pixel,
                                                   const DrawContext& ctx) const {
    auto toPixel = makeToPixel(const_cast<DrawContext&>(ctx));
    return QChartHitTester::hitTest(pixel, m_series, toPixel, &ctx);
}
