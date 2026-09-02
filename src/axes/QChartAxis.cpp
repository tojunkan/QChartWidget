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
// 此绘制函数比较特殊，绘制结果不在plotArea内部而在其外部，因此不会提交给renderer，而是直接用QPainter绘制。
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
unsigned int QChartAxis::drawAtPosition(qreal dimMin, qreal dimMax,
                                qreal offset0, qreal offset1,
                                int dimIndex,
                                QVector<QChartPrimitive>& outPrims,
                                QVector<QChartTextLabel>& outLabels,
                                int segments = 72,
                                bool drawLabels = true) const
{
    if (!m_visible) return;
    if (dimMin > dimMax) std::swap(dimMin, dimMax);

    unsigned int cnt = 0;
    // 1. 获取刻度
    QVector<qreal> ticks = tickValues(dimMin, dimMax);
    QStringList labels = tickLabels(ticks);
    const QColor axisColor = color();

    // 2. 计算三个方向向量（轴方向 + 两个垂直方向）
    QVector3D axisDir, dir1, dir2;
    switch (dimIndex) {
    case 0: axisDir = QVector3D(1, 0, 0); dir1 = QVector3D(0, 1, 0); dir2 = QVector3D(0, 0, 1); break;
    case 1: axisDir = QVector3D(0, 1, 0); dir1 = QVector3D(1, 0, 0); dir2 = QVector3D(0, 0, 1); break;
    case 2: axisDir = QVector3D(0, 0, 1); dir1 = QVector3D(1, 0, 0); dir2 = QVector3D(0, 1, 0); break;
    default: return;
    }
    QVector<QVector3D> dirs = { axisDir, -axisDir, dir1, -dir1, dir2, -dir2 };

    // 3. 生成轴线（Path 图元）
    QChartPrimitive axisLine;
    axisLine.type = QChartPrimitive::Type::Path;
    axisLine.color = axisColor;
    axisLine.penWidth = 1.0;
    for (int i = 0; i <= segments; ++i) {
        qreal t = static_cast<qreal>(i) / segments;
        qreal val = dimMin + t * (dimMax - dimMin);
        switch (dimIndex) {
        case 0: axisLine.numVerts.append(QVector3D(val, offset0, offset1)); break;
        case 1: axisLine.numVerts.append(QVector3D(offset0, val, offset1)); break;
        case 2: axisLine.numVerts.append(QVector3D(offset0, offset1, val)); break;
        }
    }
    outPrims.append(axisLine);
    cnt++;

    // 4. 对每个主刻度生成 7 个点 + 1 个标签
    for (int i = 0; i < ticks.size(); ++i) {
        qreal tickVal = ticks[i];
        QVector3D pos;
        switch (dimIndex) {
        case 0: pos = QVector3D(tickVal, offset0, offset1); break;
        case 1: pos = QVector3D(offset0, tickVal, offset1); break;
        case 2: pos = QVector3D(offset0, offset1, tickVal); break;
        }

        // 4b. 7 个点图元（中心 + 6 个方向）
        QChartPrimitive center;
        center.type = QChartPrimitive::Type::Point;
        center.numA = pos;
        center.color = axisColor;
        center.markerSize = 2.0;
        outPrims.append(center);
        cnt++;

        qreal tickLen = (dimMax - dimMin) * TICK_LENGTH;

        for (const QVector3D& d : dirs) {
            QChartPrimitive point;
            point.type = QChartPrimitive::Type::Point;
            point.numA = pos + d * tickLen;
            point.color = axisColor;
            point.markerSize = 2.0;
            outPrims.append(point);
            cnt++;
        }

        // 4c. 标签（锚点指向刻度位置，方向由 Renderer 决定）
        if (drawLabels && i < labels.size() && !labels[i].isEmpty()) {
            QChartTextLabel label;
            label.text = labels[i];
            label.color = axisColor;
            label.fontSize = 10.0f;
            label.alignment = Qt::AlignCenter;
            label.numericAnchor = pos;
            label.sourceId = -1;
            label.refPrimitiveId = -1;
            outLabels.append(label);
        }
    }

    return cnt;
}
