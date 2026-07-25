#include "QChartAxis.h"
#include <QFontMetrics>
#include <QtMath>
#include <QDebug>

// ===== base =====
QChartAxis::QChartAxis(QObject* p, Qt::Alignment alignment)
    : QObject(p), m_alignment(alignment) {
}

QSizeF QChartAxis::sizeHint(const QFont& font) const {
    QFontMetrics fm(font);
    bool isHoriz = (m_alignment == Qt::AlignBottom || m_alignment == Qt::AlignTop || m_alignment == Qt::AlignCenter);

    if (isHoriz) {
        return QSizeF(10, fm.height() + AXIS_MARGIN);
    }
    else {
        qreal w = 0;
        for (const auto& l : tickLabels())
            w = qMax(w, (qreal)fm.horizontalAdvance(l));
        return QSizeF(w + AXIS_MARGIN, 10);
    }
}

void QChartAxis::draw(QPainter* painter, const QRectF& plotArea) const {
    if (!m_visible) return;

    // 检查是否合法方向（基类只处理四种）
    if (!(m_alignment == Qt::AlignTop || m_alignment == Qt::AlignBottom ||
        m_alignment == Qt::AlignLeft || m_alignment == Qt::AlignRight)) {
        return;
    }

    painter->save();
    painter->setPen(m_color);

    QFont f = painter->font();
    f.setPointSize(f.pointSize());
    painter->setFont(f);

    QVector<qreal> ticks = tickValues();
    QStringList labels = tickLabels();
    if (ticks.isEmpty() || labels.isEmpty() || ticks.size() != labels.size()) {
        qWarning() << "Tick data invalid.";
        painter->restore();
        return;
    }

    // ---------- 1. 在 switch 中一次性确定所有方向参数 ----------
    bool isHoriz;          // 是否水平轴
    qreal fixedCoord;      // 轴线固定的坐标（Y 或 X）
    QPointF tickDir;       // 刻度线方向（指向 plotArea 内）
    QPointF labelDir;      // 文字偏移方向（指向 plotArea 外）
    Qt::Alignment textAlign; // 文字对齐方式

    switch (m_alignment) {
    case Qt::AlignTop:
        isHoriz = true;
        fixedCoord = plotArea.top();
        tickDir = { 0, 1 };          // 向下（进入图表）
        labelDir = { 0, -1 };        // 向上（外部）
        textAlign = Qt::AlignHCenter | Qt::AlignTop;
        break;
    case Qt::AlignBottom:
        isHoriz = true;
        fixedCoord = plotArea.bottom();
        tickDir = { 0, -1 };
        labelDir = { 0, 1 };
        textAlign = Qt::AlignHCenter | Qt::AlignBottom;
        break;
    case Qt::AlignLeft:
        isHoriz = false;
        fixedCoord = plotArea.left();
        tickDir = { 1, 0 };
        labelDir = { -1, 0 };
        textAlign = Qt::AlignRight | Qt::AlignVCenter;
        break;
    case Qt::AlignRight:
        isHoriz = false;
        fixedCoord = plotArea.right();
        tickDir = { -1, 0 };
        labelDir = { 1, 0 };
        textAlign = Qt::AlignLeft | Qt::AlignVCenter;
        break;
    default:
        painter->restore();
        return;
    }

    // ---------- 2. 绘制主轴线 ----------
    QPointF axisStart, axisEnd;
    if (isHoriz) {
        axisStart = QPointF(plotArea.left(), fixedCoord);
        axisEnd = QPointF(plotArea.right(), fixedCoord);
    }
    else {
        axisStart = QPointF(fixedCoord, plotArea.top());
        axisEnd = QPointF(fixedCoord, plotArea.bottom());
    }
    painter->drawLine(axisStart, axisEnd);

    // ---------- 3. 准备绘制参数 ----------
    const qreal tickLen = 4.0;
    const qreal subTickLen = 2.0;
    const qreal labelOffset = 8.0;
    const int subTickCount = m_subTickCount; // 每个主刻度之间的次刻度数量

    // 获取主刻度值
    QVector<qreal> mainTicks = ticks;
    QVector<qreal> subTicks = subTickValues(); // 直接调用纯虚函数！

    //// 在 draw 函数开头或刻度循环前
    //qDebug() << "=== Axis draw ===";
    //qDebug() << "Alignment:" << m_alignment;
    //qDebug() << "PlotArea:" << plotArea;
    //qDebug() << "Ticks count:" << ticks.size() << "Labels count:" << labels.size();
    //for (int i = 0; i < qMin(ticks.size(), labels.size()); ++i) {
    //    qDebug() << "Tick" << i << ":" << ticks[i] << "Label:" << labels[i];
    //}
    //qDebug() << "subTickCount:" << m_subTickCount;
    //qDebug() << "subTicks count:" << subTicks.size();

    // ---------- 4. 先画次刻度（直接使用子类提供的 subTickValues） ----------
    if (m_subTickCount > 0) {
        for (qreal subVal : subTicks) {
            qreal norm = valueToNormalized(subVal);
            qreal clampedNorm = qBound(0.0, norm, 1.0);
            if (norm != clampedNorm) {
                qWarning() << "doesn't match. Norm is " << norm << "while clamped version is:" << clampedNorm;
            }

            QPointF pos;
            if (isHoriz) {
                qreal x = plotArea.left() + clampedNorm * plotArea.width();
                pos = QPointF(x, fixedCoord);
            }
            else {
                qreal y = plotArea.bottom() - clampedNorm * plotArea.height();
                pos = QPointF(fixedCoord, y);
            }
            // 次刻度线（更短）
            painter->drawLine(pos, pos + tickDir * subTickLen);
        }
    }

    // ---------- 5. 画主刻度和标签 ----------
    for (int i = 0; i < mainTicks.size(); ++i) {
        qreal norm = valueToNormalized(mainTicks[i]);
        qreal clampedNorm = qBound(0.0, norm, 1.0);

        QPointF pos;
        if (isHoriz) {
            qreal x = plotArea.left() + clampedNorm * plotArea.width();
            pos = QPointF(x, fixedCoord);
        }
        else {
            qreal y = plotArea.bottom() - clampedNorm * plotArea.height();
            pos = QPointF(fixedCoord, y);
        }

        // 主刻度线
        painter->drawLine(pos, pos + tickDir * tickLen);

        // ---------- 文字部分：完全基于字体度量动态计算 ----------
        QString label = labels[i];
        QFontMetrics fm(painter->font());

        // 精确获取文本的像素宽高
        qreal textWidth = fm.horizontalAdvance(label);
        qreal textHeight = fm.height();

        // 文字锚点：从刻度线终点再往外偏移（间距 = AXIS_MARGIN - TICK_LENGTH）
        // 因为 AXIS_MARGIN 是"轴线到文字外边缘"的总距离，扣掉刻度线占的 4px，剩下的留给文字偏移
        qreal offset = AXIS_MARGIN - TICK_LENGTH;
        QPointF textPos = pos + labelDir * offset;

        QRectF textRect;
        if (isHoriz) {
            // 水平轴：文字水平居中，垂直方向紧贴 textPos
            qreal w = textWidth + TEXT_PADDING * 2;
            qreal h = textHeight + TEXT_PADDING * 2;
            qreal x = textPos.x() - w / 2.0;
            qreal y = textPos.y() - h / 2.0 + (labelDir.y() * h) / 2.0;
                    //(labelDir.y() < 0) ? textPos.y() - h : textPos.y()
            // labelDir.y() < 0 表示朝上（顶部轴），矩形底部对齐 textPos
            textRect = QRectF(x, y, w, h);
        }
        else {
            // 垂直轴：文字垂直居中，水平方向紧贴 textPos
            qreal w = textWidth + TEXT_PADDING * 2;
            qreal h = textHeight + TEXT_PADDING * 2;
            qreal x = textPos.x() - w / 2.0 + (labelDir.x() * w) / 2.0;
                    //(labelDir.x() < 0) ? textPos.x() - w : textPos.x();
            qreal y = textPos.y() - h / 2.0;
            textRect = QRectF(x, y, w, h);
        }
        painter->save();
        painter->setPen(Qt::red);
        painter->drawRect(textRect);  // 绘制标签边界
        painter->restore();
        // 然后绘制文字（用原来的颜色）
        painter->drawText(textRect, Qt::AlignCenter, label);
    }

    painter->restore();
}

void QChartAxis::pan(qreal deltaNorm) {
    qreal span = m_max - m_min;
    qreal shift =  - deltaNorm * span;
    //这里加一个负号是因为拖动的时候，控件的移动效果和数据的效果是反过来的。
    //举个例子，比如一个左侧的，以上为正方向的轴，如果你鼠标从上往下划（不需要考虑y增大的问题，因为虽然Qt中y正方向向下，但这个问题在QChartProjection中已经被纳入考虑了）
    // //那么有两种可能：
    //平移模式：数据就随着增大，也就是不加负号的情况
    //滚轮模式：控件的观感从上同步往下，也就是加负号的情况
    setMin(m_min + shift);
    setMax(m_max + shift);
}

void QChartAxis::zoom(qreal centerNorm, qreal factor) {
    if (factor <= 0) return;
    qreal centerVal = normalizedToValue(centerNorm);
    qreal halfSpan = (m_max - m_min) * factor / 2.0;
    qreal newMin = centerVal - halfSpan;
    qreal newMax = centerVal + halfSpan;
    if (qAbs(newMax - newMin) > 0.001) {
        setMin(newMin);
        setMax(newMax);
    }
}

// ===== 次刻度标签（基类默认返回空，绝大多数子类无需重写） =====
QStringList QChartAxis::subTickLabels() const {
    return QStringList(); // 默认不画文字
}