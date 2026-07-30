#include "QChartAxis.h"
#include "QCartesianProjection.h"
#include "QChartDebug.h"
#include <QFontMetrics>
#include <QtMath>
#include <QDebug>
#include <qLoggingCategory>

// ===== base =====
QChartAxis::QChartAxis(QObject* p, Qt::Alignment alignment)
    : QObject(p), m_alignment(Qt::AlignCenter) {
    if (isAlignmentValid(alignment))m_alignment = alignment;
    //如果传入参数不正确，自动置为aligncenter
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


/// <summary>
/// 轴类的绘制函数，由widget类的drawBackground函数调用。
/// 轴本身有两种语义：
/// 一种是在特定坐标系下从原点出发的，其中一坐标不变的“直线”，我们用Qt::AlignCenter来表示，因为它画在plotArea的内部；
/// 另一种是画在plotArea的侧面供使用者快速知道plotArea视窗的范围，我们用上下左右的对齐来表示。
/// 虽然第二种语义是更为特化的版本，但因为使用情况非常广泛，而且实现起来比较固定，我们在基类的方法中实现它，
/// 而第一种语义则交给各子类实现，因为它直接和各种坐标映射挂钩，甚至可能不是直线，更特化。
/// </summary>
/// <param name="painter">绘画的输出</param>
/// <param name="plotArea">绘制轴类所参考的plotArea，每个Widget只有一个</param>
/// <param name="projection">所用的坐标映射</param>
/// <param name="offset">偏移量是为了留给drawGrid使用的，对于AlignCenter的轴，我们确定位置的时候需要知道该轴的另一个坐标是多少（默认0）</param>
/// <param name="drawAxisLine">是否绘制轴线</param>
/// <param name="drawLabels">是否绘制刻度标签</param>
/// <param name="drawTicks">是否绘制（大、小）标签</param>
void QChartAxis::draw(QPainter* painter, 
    const QRectF& plotArea, 
    const QChartProjection* projection,
    qreal offset,
    bool drawAxisLine,
    bool drawLabels, 
    bool drawTicks) const {
    if (!m_visible) return;

    // 检查是否合法方向（基类只处理四种，不处理AlignCenter）
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
        qWarning() << "Tick data invalid."<<"ticks have size of "<<ticks.size()<<" while labels have size of "<<labels.size();
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
    if (drawAxisLine) {
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
    }

    // ---------- 3. 准备绘制参数 ----------
    const qreal tickLen = 4.0;
    const qreal subTickLen = 2.0;
    const qreal labelOffset = 8.0;
    const int subTickCount = m_subTickCount; // 每个主刻度之间的次刻度数量

    // 获取主刻度值
    QVector<qreal> mainTicks = ticks;
    QVector<qreal> subTicks = subTickValues(); // 直接调用纯虚函数！

    //qCDebug(logAxis) << "=== Axis draw ===";
    //qCDebug(logAxis) << "Alignment:" << m_alignment;
    //qCDebug(logAxis) << "PlotArea:" << plotArea;
    //qCDebug(logAxis) << "Ticks count:" << ticks.size() << "Labels count:" << labels.size();
    //for (int i = 0; i < qMin(ticks.size(), labels.size()); ++i) {
    //    qCDebug(logAxis) << "Tick" << i << ":" << ticks[i] << "Label:" << labels[i];
    //}
    //qCDebug(logAxis) << "subTickCount:" << m_subTickCount;
    //qCDebug(logAxis) << "subTicks count:" << subTicks.size();

    // ---------- 4. 先画次刻度（直接使用子类提供的 subTickValues） ----------
    if (drawTicks && m_subTickCount > 0) {
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
        if(drawTicks)painter->drawLine(pos, pos + tickDir * tickLen);

        // ---------- 文字部分：完全基于字体度量动态计算 ----------
        if (drawLabels) {
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

            if (logAxis().isDebugEnabled()) {
                painter->save();
                painter->setPen(Qt::red);
                painter->drawRect(textRect);  // 绘制标签边界
                painter->restore();
            }
            // 然后绘制文字（用原来的颜色）
            painter->drawText(textRect, Qt::AlignCenter, label);
        }
    }
    
    if (logAxis().isDebugEnabled()) {
        QRectF axisArea;
        switch (m_alignment) {
        case Qt::AlignBottom:
            axisArea = QRectF(plotArea.left(), plotArea.bottom(), plotArea.width(), sizeHint(f).height());
            break;
        case Qt::AlignTop:
            axisArea = QRectF(plotArea.left(), plotArea.top() - sizeHint(f).height(), plotArea.width(), sizeHint(f).height());
            break;
        case Qt::AlignLeft:
            axisArea = QRectF(plotArea.left() - sizeHint(f).width(), plotArea.top(), sizeHint(f).width(), plotArea.height());
            break;
        case Qt::AlignRight:
            axisArea = QRectF(plotArea.right(), plotArea.top(), sizeHint(f).width(), plotArea.height());
            break;
        default:
            axisArea = QRectF();
        }
        painter->setPen(Qt::blue);
        qCDebug(logAxis) << "axis area :" << axisArea;
        painter->drawRect(axisArea);
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
    qreal span = m_max - m_min;
    qreal newSpan = span * factor;

    qreal newMin = centerVal - centerNorm * newSpan;
    qreal newMax = centerVal + (1.0 - centerNorm) * newSpan;

    //qCDebug(logAxis) << "zoom: centerNorm=" << centerNorm
    //    << "centerVal=" << centerVal
    //    << "oldMin=" << m_min
    //    << "oldMax=" << m_max
    //    << "newMin=" << newMin
    //    << "newMax=" << newMax;

    if (newMax - newMin > 0.001) {
        setMin(newMin);
        setMax(newMax);
    }
}

// ===== 次刻度标签（基类默认返回空，绝大多数子类无需重写） =====
QStringList QChartAxis::subTickLabels() const {
    return QStringList(); // 默认不画文字
}

bool QChartAxis::isAlignmentValid(Qt::Alignment alignment) const {
    if (coordinateSystem() == CoordinateSystem::Cartesian)return true;
    else if (alignment == Qt::AlignCenter)return true;
    qWarning() << "input alignment is not valid.";
    return false;
}