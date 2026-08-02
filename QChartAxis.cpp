// QChartAxis.cpp —— 轴基类实现
// 包含 drawAtEdge（边框轴）和 drawAtPosition（数据主脊）两个绘制分支
// 遵循五空间模型：刻度在 Numeric 空间生成，边框轴 Numeric→Pixel 线性插值，
// 数据主脊 Numeric→toCartesian→View→Pixel
#include "QChartAxis.h"
#include "QChartDebug.h"
#include <QFontMetrics>
#include <QPainterPath>
#include <QtMath>
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logAxis, "chart.axis")

// ===== 构造 =====
QChartAxis::QChartAxis(QObject* parent, Qt::Alignment alignment)
    : QObject(parent)
    , m_alignment(alignment)
{
    // 语法糖字段 m_sugarMin/m_sugarMax 初始为 0,0（= "未设置"）
    // 对齐校验：只接受六种合法值，非法则回退为 AlignVCenter
    if (alignment != Qt::AlignBottom && alignment != Qt::AlignTop
        && alignment != Qt::AlignLeft   && alignment != Qt::AlignRight
        && alignment != Qt::AlignHCenter && alignment != Qt::AlignVCenter) {
        qWarning() << "QChartAxis: invalid alignment" << static_cast<int>(alignment)
                   << "— falling back to AlignVCenter";
        m_alignment = Qt::AlignVCenter;
    }
    qCDebug(logAxis) << "QChartAxis created, alignment:" << m_alignment;
}

// ===== 样式 setter =====
void QChartAxis::setVisible(bool v) {
    if (m_visible == v) return;
    m_visible = v;
    emit visibleChanged();
}

void QChartAxis::setTickCount(int n) {
    if (n < 2) n = 2; // 至少 2 个刻度才有意义
    if (n == m_tickCount) return;
    m_tickCount = n;
    qCDebug(logAxis) << "tickCount:" << n;
    emit tickCountChanged();
}

void QChartAxis::setSubTickCount(int n) {
    if (n == m_subTickCount) return;
    m_subTickCount = n;
    qCDebug(logAxis) << "subTickCount:" << n;
    emit subTickCountChanged();
}

// ===== 语法糖 setRange（仅 Cartesian 有效）=====
void QChartAxis::setRange(qreal min, qreal max) {
    if (qFuzzyCompare(m_sugarMin, min) && qFuzzyCompare(m_sugarMax, max))
        return;
    m_sugarMin = min;
    m_sugarMax = max;
    qCDebug(logAxis) << "setRange (语法糖):" << min << "→" << max;
    emit rangeChanged(min, max);
    emit styleChanged();
}

// ===== 次刻度默认实现 =====
QVector<qreal> QChartAxis::subTickValues(qreal, qreal) const {
    return {}; // 默认无次刻度，子类可覆盖
}

// ===== sizeHint =====
QSizeF QChartAxis::sizeHint(const QFont& font) const {
    // 数据主脊不占外边距
    if (m_alignment == Qt::AlignHCenter || m_alignment == Qt::AlignVCenter)
        return QSizeF(0, 0);

    QFontMetrics fm(font);
    bool isHoriz = (m_alignment == Qt::AlignBottom || m_alignment == Qt::AlignTop);

    if (isHoriz) {
        // 水平边框轴：宽度自由，高度 = 字体高度 + 边距
        return QSizeF(10.0, static_cast<qreal>(fm.height()) + AXIS_MARGIN);
    } else {
        // 垂直边框轴：高度自由，宽度取合理默认值
        return QSizeF(40.0 + AXIS_MARGIN, 10.0);
    }
}

// ===== drawAtEdge：边框轴 =====
void QChartAxis::drawAtEdge(QPainter* painter,
                            const DrawContext& ctx,
                            bool drawAxisLine,
                            bool drawLabels,
                            bool drawTicks) const
{
    if (!m_visible) return;

    // 仅四种边缘对齐
    if (!(m_alignment == Qt::AlignTop    || m_alignment == Qt::AlignBottom
       || m_alignment == Qt::AlignLeft   || m_alignment == Qt::AlignRight)) {
        qWarning() << "QChartAxis::drawAtEdge: alignment" << m_alignment
                   << "is not a border alignment — use drawAtPosition()";
        return;
    }

    painter->save();
    painter->setPen(m_color);
    QFont f = painter->font();
    f.setPointSize(f.pointSize());
    painter->setFont(f);

    // ── 确定本轴对应的 Numeric 维度范围 ──
    bool isHoriz = (m_alignment == Qt::AlignBottom || m_alignment == Qt::AlignTop);
    qreal numericMin, numericMax;
    if (isHoriz) {
        numericMin = ctx.dataBounds.left();
        numericMax = ctx.dataBounds.left() + ctx.dataBounds.width();
    } else {
        numericMin = ctx.dataBounds.top();
        numericMax = ctx.dataBounds.top() + ctx.dataBounds.height();
    }

    // ── 方向参数 ──
    qreal fixedCoord;
    QPointF tickDir, labelDir;
    switch (m_alignment) {
    case Qt::AlignTop:
        fixedCoord = ctx.plotArea.top();   tickDir = {0, 1};  labelDir = {0, -1}; break;
    case Qt::AlignBottom:
        fixedCoord = ctx.plotArea.bottom(); tickDir = {0, -1}; labelDir = {0, 1};  break;
    case Qt::AlignLeft:
        fixedCoord = ctx.plotArea.left();   tickDir = {1, 0};  labelDir = {-1, 0}; break;
    case Qt::AlignRight:
        fixedCoord = ctx.plotArea.right();  tickDir = {-1, 0}; labelDir = {1, 0};  break;
    default: painter->restore(); return;
    }

    // ── 轴线 ──
    if (drawAxisLine) {
        QPointF start, end;
        if (isHoriz) {
            start = QPointF(ctx.plotArea.left(),  fixedCoord);
            end   = QPointF(ctx.plotArea.right(), fixedCoord);
        } else {
            start = QPointF(fixedCoord, ctx.plotArea.top());
            end   = QPointF(fixedCoord, ctx.plotArea.bottom());
        }
        painter->drawLine(start, end);
        qCDebug(logAxis) << "drawAtEdge: axis line" << start << "→" << end;
    }

    // ── 刻度生成 ──
    const QVector<qreal> mainTicks = tickValues(numericMin, numericMax);
    const QStringList labels = tickLabels(mainTicks);
    const QVector<qreal> subTicks = subTickValues(numericMin, numericMax);

    // ── 次刻度 ──
    if (drawTicks && !subTicks.isEmpty()) {
        for (qreal subVal : subTicks) {
            qreal norm = (numericMax != numericMin)
                ? (subVal - numericMin) / (numericMax - numericMin) : 0.5;
            norm = qBound(0.0, norm, 1.0);
            QPointF pos;
            if (isHoriz)
                pos = QPointF(ctx.plotArea.left() + norm * ctx.plotArea.width(), fixedCoord);
            else
                pos = QPointF(fixedCoord, ctx.plotArea.bottom() - norm * ctx.plotArea.height());
            painter->drawLine(pos, pos + tickDir * SUB_TICK_LENGTH);
        }
    }

    // ── 主刻度和标签 ──
    const qreal tickLen = TICK_LENGTH;
    const qreal lblOff  = AXIS_MARGIN - TICK_LENGTH;

    for (int i = 0; i < mainTicks.size(); ++i) {
        qreal norm = (numericMax != numericMin)
            ? (mainTicks[i] - numericMin) / (numericMax - numericMin) : 0.5;
        norm = qBound(0.0, norm, 1.0);

        QPointF pos;
        if (isHoriz)
            pos = QPointF(ctx.plotArea.left() + norm * ctx.plotArea.width(), fixedCoord);
        else
            pos = QPointF(fixedCoord, ctx.plotArea.bottom() - norm * ctx.plotArea.height());

        // 刻度线
        if (drawTicks)
            painter->drawLine(pos, pos + tickDir * tickLen);

        // 标签
        if (drawLabels && i < labels.size()) {
            const QString& label = labels[i];
            QFontMetrics fm(painter->font());
            qreal textW = static_cast<qreal>(fm.horizontalAdvance(label));
            qreal textH = static_cast<qreal>(fm.height());

            QPointF textBase = pos + tickDir * tickLen + labelDir * lblOff;
            QRectF textRect;
            qreal w = textW + TEXT_PADDING * 2.0;
            qreal h = textH + TEXT_PADDING * 2.0;
            if (isHoriz) {
                qreal x = textBase.x() - w / 2.0;
                qreal y = (labelDir.y() > 0) ? textBase.y() : textBase.y() - h;
                textRect = QRectF(x, y, w, h);
            } else {
                qreal x = (labelDir.x() > 0) ? textBase.x() : textBase.x() - w;
                qreal y = textBase.y() - h / 2.0;
                textRect = QRectF(x, y, w, h);
            }

            if (logAxis().isDebugEnabled()) {
                painter->save();
                painter->setPen(Qt::red);
                painter->drawRect(textRect);
                painter->restore();
            }

            painter->drawText(textRect, Qt::AlignCenter, label);
        }
    }

    painter->restore();
}

// ===== drawAtPosition：数据主脊 =====
void QChartAxis::drawAtPosition(QPainter* painter,
                                const DrawContext& ctx,
                                qreal offset,
                                bool drawAxisLine,
                                bool drawLabels,
                                bool drawTicks) const
{
    if (!m_visible) return;
    if (!ctx.projection) {
        qWarning() << "QChartAxis::drawAtPosition: DrawContext has null projection";
        return;
    }

    painter->save();
    painter->setPen(m_color);

    bool isHoriz = (m_alignment == Qt::AlignHCenter || m_alignment == Qt::AlignLeft || m_alignment == Qt::AlignRight);
    // isHoriz=true  → 沿 dim0 扫描, dim1=offset
    // isHoriz=false → dim0=offset, 沿 dim1 扫描

    // ── 轴线（经 Projection 映射后可能弯曲）──
    if (drawAxisLine) {
        auto dataCurve = [isHoriz, offset, &ctx](qreal t) -> QPointF {
            if (isHoriz) {
                qreal num0 = ctx.dataBounds.left() + t * ctx.dataBounds.width();
                return QPointF(num0, offset);
            } else {
                qreal num1 = ctx.dataBounds.top() + t * ctx.dataBounds.height();
                return QPointF(offset, num1);
            }
        };

        QPainterPath path = ctx.projection->createPath(dataCurve, 72);

        qCDebug(logRender) << "drawAtPosition: path.elementCount=" << path.elementCount();
        if (path.elementCount() > 0) {
            qCDebug(logRender) << "drawAtPosition: path first point="
                << path.elementAt(0)
                << "last point="
                << path.elementAt(path.elementCount() - 1);
            QRectF pathBounds = path.boundingRect();
            qCDebug(logRender) << "drawAtPosition: path bounds=" << pathBounds;
        }

        // View Cartesian path → Pixel path
        if (!path.isEmpty()) {
            QPainterPath pixelPath;
            int validPoints = 0;
            for (int i = 0; i < path.elementCount(); ++i) {
                const auto& el = path.elementAt(i);
                qreal px = ctx.plotArea.left()
                    + (el.x - ctx.viewRect.left()) / ctx.viewRect.width()
                    * ctx.plotArea.width();
                qreal py = ctx.plotArea.bottom()
                    - (el.y - ctx.viewRect.top()) / ctx.viewRect.height()
                    * ctx.plotArea.height();

                if (i == 0 || el.isMoveTo())
                    pixelPath.moveTo(px, py);
                else
                    pixelPath.lineTo(px, py);
                ++validPoints;
            }

            // ── 新增调试 ──
            qCDebug(logRender) << "drawAtPosition: pixelPath validPoints=" << validPoints
                << "boundingRect=" << pixelPath.boundingRect()
                << "plotArea=" << ctx.plotArea;

            painter->drawPath(pixelPath);
        }
        else {
            qCDebug(logRender) << "drawAtPosition: path is EMPTY! Skipping axis line.";
        }
    }

    // ── 刻度和标签 ──
    if (drawTicks || drawLabels) {
        qreal numericMin, numericMax;
        if (isHoriz) {
            numericMin = ctx.dataBounds.left();
            numericMax = ctx.dataBounds.left() + ctx.dataBounds.width();
        } else {
            numericMin = ctx.dataBounds.top();
            numericMax = ctx.dataBounds.top() + ctx.dataBounds.height();
        }

        const QVector<qreal> ticks = tickValues(numericMin, numericMax);
        const QStringList labels = tickLabels(ticks);
        if (ticks.isEmpty()) { painter->restore(); return; }

        qCDebug(logRender) << "drawAtPosition: offset=" << offset << "isHoriz=" << isHoriz
                         << "ticks=" << ticks << "plotArea=" << ctx.plotArea
                         << "viewRect=" << ctx.viewRect;

        for (int i = 0; i < ticks.size(); ++i) {
            // Numeric → View Cartesian
            QPointF cartesian;
            if (isHoriz)
                cartesian = ctx.projection->toCartesian(ticks[i], offset);
            else
                cartesian = ctx.projection->toCartesian(offset, ticks[i]);

            if (!std::isfinite(cartesian.x()) || !std::isfinite(cartesian.y())) {
                qCDebug(logRender) << "drawAtPosition: NaN at tick" << ticks[i] << "—skipping";
                continue;
            }

            qCDebug(logRender) << "drawAtPosition: tick" << ticks[i]
                            << "→ cartesian" << cartesian;

            // View Cartesian → Pixel
            qreal px = ctx.plotArea.left()
                + (cartesian.x() - ctx.viewRect.left()) / ctx.viewRect.width()
                * ctx.plotArea.width();
            qreal py = ctx.plotArea.bottom()
                - (cartesian.y() - ctx.viewRect.top()) / ctx.viewRect.height()
                * ctx.plotArea.height();
            QPointF pixelPos(px, py);

            if (!ctx.plotArea.contains(pixelPos))
                continue;

            // 刻度标记：用点状标记画（避免弯曲路径上做法向量）
            if (drawTicks) {
                // 画一个以 pixelPos 为中心的小十字
                painter->drawPoint(pixelPos);
                painter->drawPoint(QPointF(px + 1.0, py));
                painter->drawPoint(QPointF(px - 1.0, py));
                painter->drawPoint(QPointF(px, py + 1.0));
                painter->drawPoint(QPointF(px, py - 1.0));
            }

            // 标签
            if (drawLabels && i < labels.size()) {
                QFontMetrics fm(painter->font());
                qreal textW = static_cast<qreal>(fm.horizontalAdvance(labels[i]));
                qreal textH = static_cast<qreal>(fm.height());
                // 放在 tick 右下方
                QRectF textRect(px + 6.0, py + 6.0,
                                textW + TEXT_PADDING * 2.0,
                                textH + TEXT_PADDING * 2.0);
                painter->drawText(textRect, Qt::AlignCenter, labels[i]);
            }
        }
    }

    painter->restore();
}
