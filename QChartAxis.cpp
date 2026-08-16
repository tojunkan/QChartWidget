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
Q_LOGGING_CATEGORY(logAxisVerbose, "chart.axis.verbose", QtWarningMsg)

// QChartAxis.cpp

// ============================================================
// 匿名命名空间：存放绘制辅助函数（不暴露到头文件）
// ============================================================
namespace {

    // 1. Numeric → Pixel 完整映射链
    QPointF mapNumericToPixel(const DrawContext& ctx, qreal num0, qreal num1)
    {
        if (!ctx.projection) return QPointF(qQNaN(), qQNaN());
        QPointF cartesian = ctx.projection->toCartesian(num0, num1);
        if (!std::isfinite(cartesian.x()) || !std::isfinite(cartesian.y()))
            return QPointF(qQNaN(), qQNaN());

        return QChartCamera::cartesianToPixel(ctx.viewRect, ctx.plotArea,
                                              cartesian.x(), cartesian.y());
    }

    // 2. View Cartesian 路径 → Pixel 路径
    QPainterPath mapViewPathToPixel(const DrawContext& ctx, const QPainterPath& viewPath)
    {
        QPainterPath pixelPath;
        if (viewPath.isEmpty()) return pixelPath;

        for (int i = 0; i < viewPath.elementCount(); ++i) {
            const auto& el = viewPath.elementAt(i);
            QPointF p = QChartCamera::cartesianToPixel(ctx.viewRect, ctx.plotArea,
                                                       el.x, el.y);
            if (i == 0 || el.isMoveTo())
                pixelPath.moveTo(p);
            else
                pixelPath.lineTo(p);
        }
        return pixelPath;
    }

    // 3. 画小十字刻度标记
    void drawTickMark(QPainter* painter, const QPointF& pos, qreal length = 5.0)
    {
        painter->drawPoint(pos);
        painter->drawPoint(QPointF(pos.x() + length, pos.y()));
        painter->drawPoint(QPointF(pos.x() - length, pos.y()));
        painter->drawPoint(QPointF(pos.x(), pos.y() + length));
        painter->drawPoint(QPointF(pos.x(), pos.y() - length));
    }

    // 4. 单标签绘制
// 辅助函数：从 Pixel 路径中提取标签锚点
    void drawSingleLabel(QPainter* painter,
        const DrawContext& ctx,
        const QPainterPath& pixelPath,
        const QString& label)
    {
        if (pixelPath.isEmpty() || label.isEmpty()) return;

        // 1. 寻找路径上第一个位于 plotArea 内部的点作为锚点
        QPointF anchor;
        bool found = false;
        for (int i = pixelPath.elementCount() - 1; i >= 0; --i) {
            const auto& el = pixelPath.elementAt(i);
            QPointF pos(el.x, el.y);
            if (ctx.plotArea.contains(pos)) {
                anchor = pos;
                found = true;
                break;
            }
        }
        if (!found) { 
			qWarning() << "drawSingleLabel: no anchor point found in plotArea for label" << label;
            return; 
        }

        // 2. 测量文字尺寸
        QFontMetrics fm(painter->font());
        qreal textW = static_cast<qreal>(fm.horizontalAdvance(label));
        qreal textH = static_cast<qreal>(fm.height());
        const qreal pad = QChartAxis::textPadding();  // 间距
        const qreal w = textW + pad * 2;
        const qreal h = textH + pad * 2;

        // 3. 计算到四条边的距离，选择最远方向放置
        qreal distLeft = anchor.x() - ctx.plotArea.left();
        qreal distRight = ctx.plotArea.right() - anchor.x();
        qreal distTop = anchor.y() - ctx.plotArea.top();
        qreal distBottom = ctx.plotArea.bottom() - anchor.y();

        // 找最大距离的方向
        enum Dir { Left, Right, Top, Bottom };
        Dir dir = Left;
        qreal maxDist = distLeft;
        if (distRight > maxDist) { maxDist = distRight; dir = Right; }
        if (distTop > maxDist) { maxDist = distTop;   dir = Top; }
        if (distBottom > maxDist) { maxDist = distBottom; dir = Bottom; }

        // 根据方向计算文本矩形
        QRectF textRect;
        switch (dir) {
        case Left:   // 文本放左侧
            textRect = QRectF(anchor.x() - w - pad, anchor.y() - h / 2, w, h);
            break;
        case Right:  // 文本放右侧
            textRect = QRectF(anchor.x() + pad, anchor.y() - h / 2, w, h);
            break;
        case Top:    // 文本放上方
            textRect = QRectF(anchor.x() - w / 2, anchor.y() - h - pad, w, h);
            break;
        case Bottom: // 文本放下方
            textRect = QRectF(anchor.x() - w / 2, anchor.y() + pad, w, h);
            break;
        }

        // 4. 如果计算出的矩形超出 plotArea，进行裁剪调整
        if (!ctx.plotArea.contains(textRect)) {
            // 尝试将矩形移入 plotArea 内（水平或垂直方向微调）
            if (textRect.left() < ctx.plotArea.left())
                textRect.moveLeft(ctx.plotArea.left() + pad);
            if (textRect.right() > ctx.plotArea.right())
                textRect.moveRight(ctx.plotArea.right() - pad);
            if (textRect.top() < ctx.plotArea.top())
                textRect.moveTop(ctx.plotArea.top() + pad);
            if (textRect.bottom() > ctx.plotArea.bottom())
                textRect.moveBottom(ctx.plotArea.bottom() - pad);
            // 如果调整后仍无效或尺寸过小，直接居中放置（兜底）
            if (textRect.width() < w * 0.5 || textRect.height() < h * 0.5) {
                textRect = QRectF(ctx.plotArea.center().x() - w / 2,
                    ctx.plotArea.center().y() - h / 2,
                    w, h);
            }
        }

        painter->drawText(textRect, Qt::AlignCenter, label);
    }

} // namespace

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


// 绘制部分：

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
    painter->setPen(color());
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
    bool drawTicks,
    QString& label,
    QPen* pen) const
{
    if (!m_visible) return;
    if (!ctx.projection) {
        qWarning() << "QChartAxis::drawAtPosition: DrawContext has null projection";
        return;
    }

    painter->save();
    if (pen)painter->setPen(*pen);
    else painter->setPen(color());

    bool isHoriz = isHorizontal();

    // ── 构建数据曲线 ──
    auto dataCurve = [isHoriz, offset, &ctx](qreal t) -> QPointF {
        if (isHoriz) {
            qreal num0 = ctx.dataBounds.left() + t * ctx.dataBounds.width();
            return QPointF(num0, offset);
        }
        else {
            qreal num1 = ctx.dataBounds.top() + t * ctx.dataBounds.height();
            return QPointF(offset, num1);
        }
        };

    // ── 构建 View Cartesian 路径 ──
    QPainterPath viewPath = ctx.projection->createPath(dataCurve, 72);

    // ── 画轴线 ──
	QPainterPath pixelPath;
    if (drawAxisLine && !viewPath.isEmpty()) {
        pixelPath = mapViewPathToPixel(ctx, viewPath);
        painter->drawPath(pixelPath);
    }

    // ── 刻度和标签 ──
    // 注意：多标签逻辑已完全移除。只画单标签（offset）和刻度标记。
    if (drawTicks || drawLabels) {
        qreal numericMin, numericMax;
        if (isHoriz) {
            numericMin = ctx.dataBounds.left();
            numericMax = ctx.dataBounds.left() + ctx.dataBounds.width();
        }
        else {
            numericMin = ctx.dataBounds.top();
            numericMax = ctx.dataBounds.top() + ctx.dataBounds.height();
        }

        const QVector<qreal> ticks = tickValues(numericMin, numericMax);
        const QStringList labels = tickLabels(ticks);
        if (ticks.isEmpty()) { painter->restore(); return; }

        // ── 画刻度标记（所有可见的 tick 位置） ──
        if (drawTicks) {
            for (int i = 0; i < ticks.size(); ++i) {
                qreal num0, num1;
                if (isHoriz) {
                    num0 = ticks[i];
                    num1 = offset;
                }
                else {
                    num0 = offset;
                    num1 = ticks[i];
                }
                QPointF pixelPos = mapNumericToPixel(ctx, num0, num1);
                if (!std::isfinite(pixelPos.x()) || !std::isfinite(pixelPos.y())) {
                    qCDebug(logAxisVerbose) << "TICK NaN: tick=" << ticks[i] << "pixel=" << pixelPos;
                    continue;
                }
                if (!ctx.plotArea.contains(pixelPos)) {
                    qCDebug(logAxisVerbose) << "TICK SKIP: tick=" << ticks[i] << "pixel=" << pixelPos
                                            << "plotArea=" << ctx.plotArea;
                    continue;
                }
                drawTickMark(painter, pixelPos);
            }
        }

        // ── 画单标签（offset 对应的标签） ──
        if (drawLabels) {
            if (!label.isEmpty()) {
                QString labelWithUnit = label;
				if(!isHoriz) labelWithUnit = ctx.projection->dimensionName(0) + "=" + label;
				else labelWithUnit = ctx.projection->dimensionName(1) + "=" + label;
                drawSingleLabel(painter, ctx, pixelPath, labelWithUnit);
            }
        }
    }

    painter->restore();
}