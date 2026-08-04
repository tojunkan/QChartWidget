// QChartGeometry.cpp —— 几何层基类实现
#include "QChartGeometry.h"
#include "QChartWidget.h"
#include "QChartSeries.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logGeometry, "chart.geometry")

QChartGeometry::QChartGeometry(QObject* parent) : QObject(parent) {}
QChartGeometry::~QChartGeometry() { qDeleteAll(m_series); }

// ===== 轴绑定 =====
void QChartGeometry::setAxisX(QChartAxis* a) {
    m_axisX = a;
    qCDebug(logGeometry) << "setAxisX:" << (a ? "set" : "null");
}

void QChartGeometry::setAxisY(QChartAxis* a) {
    m_axisY = a;
    qCDebug(logGeometry) << "setAxisY:" << (a ? "set" : "null");
}

bool QChartGeometry::validateAxes() const {
    if (!m_axisX) {
        qWarning() << "QChartGeometry::validateAxes: axisX is null";
        return false;
    }
    if (!m_axisY) {
        qWarning() << "QChartGeometry::validateAxes: axisY is null";
        return false;
    }
    return true;
}

// ===== Grid 样式 =====
void QChartGeometry::setGridVisible(bool v) {
    if (m_gridVisible == v) return;
    m_gridVisible = v;
    emit gridChanged();
}

void QChartGeometry::setGridColor(const QColor& c) {
    if (m_gridColor == c) return;
    m_gridColor = c;
    emit gridChanged();
}

// ===== makeToPixel：组装完整坐标变换链 =====
std::function<QPointF(QVariant,QVariant)> QChartGeometry::makeToPixel(const DrawContext& ctx) const {
    // 链: Data(QVariant) → toNumeric → toCartesian → cartesianToPixel
    // 返回的函数将 Data 空间的 (x,y) 直接映射到 Pixel
    // Series 不知道 Axis 类型——toNumeric 由 Geometry 在此注入
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

        // View Cartesian → Pixel（线性映射）
        qreal px = ctx.plotArea.left()
            + (cartesian.x() - ctx.viewRect.left()) / ctx.viewRect.width()
            * ctx.plotArea.width();
        qreal py = ctx.plotArea.bottom()
            - (cartesian.y() - ctx.viewRect.top()) / ctx.viewRect.height()
            * ctx.plotArea.height();

        return QPointF(px, py);
    };
}

// ===== drawGrid：用轴 drawAtPosition 画网格线 =====
void QChartGeometry::drawGrid(QPainter* painter, const DrawContext& ctx) const {
    if (!m_gridVisible) return;
    if (!m_axisX || !m_axisY || !ctx.projection)
        return;

    qCDebug(logGeometry) << "drawGrid: dataBounds=" << ctx.dataBounds
                         << "viewRect=" << ctx.viewRect;

    painter->save();
    painter->setPen(QPen(m_gridColor, 1.0, Qt::DotLine));

    // 获取 dataBounds 对应的 Numeric 范围
    qreal dim0Min = ctx.dataBounds.left();
    qreal dim0Max = ctx.dataBounds.left() + ctx.dataBounds.width();
    qreal dim1Min = ctx.dataBounds.top();
    qreal dim1Max = ctx.dataBounds.top() + ctx.dataBounds.height();

    // 画 dim0 方向的网格线（垂直数据主脊）：dim0=扫, dim1=tick
    int countY = qMax(2, m_axisY->tickCount());
	QVector<qreal> ticksY = m_axisY->tickValues(dim1Min, dim1Max);
    QPen gridPen;
    int i = 0;
    for (qreal tickVal : ticksY) {
        // tickVal 已经是 Numeric 空间的 dim1 值 → 作为 offset 传给 dim0 的轴
        if (i % 2 == 0) {
			gridPen = QPen(m_gridColor, 1.0, Qt::DashLine);
            m_axisX->drawAtPosition(painter, ctx, tickVal,
                /*axisLine=*/true, /*labels=*/true, /*ticks=*/true, /*pen=*/&gridPen);
        }
        else {
			gridPen = QPen(m_gridColor, 1.0, Qt::SolidLine);
            m_axisX->drawAtPosition(painter, ctx, tickVal,
                /*axisLine=*/true, /*labels=*/true, /*ticks=*/true, /*pen=*/&gridPen);
        }
        i++;
    }

    // 画 dim1 方向的网格线（水平数据主脊）：dim1=扫, dim0=tick
    i = 0;
    int countX = qMax(2, m_axisX->tickCount());
	QVector<qreal> ticksX = m_axisX->tickValues(dim0Min, dim0Max);
    for (qreal tickVal : ticksX) {
        if (i % 2 == 0) {
			gridPen = QPen(m_gridColor, 1.0, Qt::DashLine);
            m_axisY->drawAtPosition(painter, ctx, tickVal,
                /*axisLine=*/true, /*labels=*/true, /*ticks=*/true, /*pen=*/&gridPen);
		}
		else {
			gridPen = QPen(m_gridColor, 1.0, Qt::SolidLine);
            m_axisY->drawAtPosition(painter, ctx, tickVal,
                /*axisLine=*/true, /*labels=*/true, /*ticks=*/true, /*pen=*/&gridPen);
		}
        i++;
    }

    painter->restore();
}

// ===== Series 管理 =====
void QChartGeometry::addSeries(QChartSeries* s) {
    if (!s) return;
    s->setParent(this);
    m_series.append(s);
    emit seriesAdded(s);
}

void QChartGeometry::removeSeries(QChartSeries* s) {
    if (m_series.removeAll(s)) {
        emit seriesRemoved(s);
        delete s;
    }
}

void QChartGeometry::clearSeries() {
    qDeleteAll(m_series);
    m_series.clear();
}

// ===== drawAllSeries =====
void QChartGeometry::drawAllSeries(QPainter* painter, const DrawContext& ctx) {
    if (!validateAxes()) {
        qWarning() << "QChartGeometry::drawAllSeries: axes not valid, aborting";
        return;
    }

    auto toPixel = makeToPixel(ctx);

    for (auto* s : m_series) {
        if (!s || !s->isVisible()) continue;
        painter->save();
        painter->setOpacity(s->opacity());
        s->draw(painter, toPixel);
        painter->restore();
    }
}

// ===== 命中检测 =====
QChartGeometry::HitResult QChartGeometry::hitTest(const QPointF& pixel,
                                                   const DrawContext& ctx) const {
    auto toPixel = makeToPixel(ctx);
    for (int i = m_series.size() - 1; i >= 0; --i) {
        auto* s = m_series[i];
        if (!s || !s->isVisible()) continue;
        int idx = s->hitTest(pixel, toPixel);
        if (idx >= 0)
            return { s, idx };
    }
    return { nullptr, -1 };
}
