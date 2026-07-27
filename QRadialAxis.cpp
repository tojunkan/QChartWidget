#include "QRadialAxis.h"
#include <QFontMetrics>
#include <QtMath>
#include <QDebug>

// ===== 构造函数 =====
QRadialAxis::QRadialAxis(QObject* parent)
    : QChartAxis(parent, Qt::AlignCenter)
{
    m_min = 0.0;
    m_max = 1.0;
    m_tickCount = 4;          // 0, 0.25, 0.5, 0.75, 1.0
    m_visible = true;     // 默认可见（可自行关闭）
    m_panEnabled = false;    // 原点固定，不可拖拽
    m_zoomEnabled = true;     // 可缩放（径向缩放）
}

CoordinateSystem QRadialAxis::coordinateSystem() const { return CoordinateSystem::Polar; }

// ===== 径向缩放（忽略 centerNorm，从原点均匀缩放） =====
void QRadialAxis::zoom(qreal /*centerNorm*/, qreal factor) {
    qWarning() << "zoom function triggered.";
    qreal span = m_max - m_min;
    if (span <= 0.0) return;
    qreal newMax = m_min + span * factor;
    //if (newMax > 2.0)  newMax = 2.0;   // 限制最大范围
    //if (newMax < 0.1)  newMax = 0.1;   // 限制最小范围
    setMax(newMax);
}

// ===== 坐标映射（线性） =====
qreal QRadialAxis::valueToNormalized(qreal value) const {
    if (qFuzzyCompare(m_max, m_min)) return 0.0;
    return (value - m_min) / (m_max - m_min);
}

qreal QRadialAxis::normalizedToValue(qreal norm) const {
    return m_min + norm * (m_max - m_min);
}

// ===== 主刻度 =====
QVector<qreal> QRadialAxis::tickValues() const {
    qreal step = (m_max - m_min) / qreal(m_tickCount);
    QVector<qreal> ticks;
    for (int i = 0; i <= m_tickCount; ++i) {
        ticks.append(m_min + qreal(i) * step);
    }
    return ticks;
}

// ===== 主刻度标签 =====
QStringList QRadialAxis::tickLabels() const {
    QStringList labels;
    for (qreal value : tickValues()) {
        labels.append(QString::number(value, 'f', 2));
    }
    return labels;
}

// ===== 次刻度（线性插值） =====
QVector<qreal> QRadialAxis::subTickValues() const {
    QVector<qreal> subs;
    if (m_subTickCount <= 0) return subs;

    qreal step = (m_max - m_min) / qreal(m_tickCount);
    qreal subStep = step / qreal(m_subTickCount + 1);

    for (int i = 0; i < m_tickCount; ++i) {
        qreal mainVal = m_min + qreal(i) * step;
        for (int j = 1; j <= m_subTickCount; ++j) {
            subs.append(mainVal + qreal(j) * subStep);
        }
    }
    return subs;
}

// ============================================================
// 新增：径向轴绘制（中心向右延伸的射线）
// 完全复制基类 QChartAxis::draw 的逻辑，但适配中心点方向
// ============================================================
void QRadialAxis::draw(QPainter* painter, const QRectF& plotArea, const QChartProjection* projection) const {
    if (!m_visible) return;

    // 径向轴只支持 AlignCenter
    if (m_alignment != Qt::AlignCenter) {
        qWarning() << "RadialAxis should have AlignCenter.";
        //QChartAxis::draw(painter, plotArea, projection);
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
        qWarning() << "Radial axis: tick data invalid.";
        painter->restore();
        return;
    }

    // ---------- 1. 径向轴特有参数 ----------
    QPointF center = plotArea.center();
    qreal maxRadius = plotArea.width() / 2.0;
    QPointF dir = QPointF(1, 0);          // 射线方向：向右
    QPointF tickDir = QPointF(0, -1);     // 刻度线方向：向上（垂直）
    QPointF labelDir = QPointF(0, 1);     // 标签方向：向下（在射线下方）

    // ---------- 2. 绘制主射线（轴线） ----------
    QPointF axisStart = center;
    QPointF axisEnd = center + dir * maxRadius;
    painter->drawLine(axisStart, axisEnd);

    // ---------- 3. 准备刻度参数 ----------
    const qreal tickLen = TICK_LENGTH;
    const qreal subTickLen = SUB_TICK_LENGTH;

    QVector<qreal> mainTicks = ticks;
    QVector<qreal> subTicks = subTickValues();

    // ---------- 4. 画次刻度 ----------
    if (m_subTickCount > 0) {
        for (qreal subVal : subTicks) {
            qreal norm = valueToNormalized(subVal);
            qreal clampedNorm = qBound(0.0, norm, 1.0);
            QPointF pos = center + dir * (clampedNorm * maxRadius);
            painter->drawLine(pos, pos + tickDir * subTickLen);
        }
    }

    // ---------- 5. 画主刻度和标签 ----------
    for (int i = 0; i < mainTicks.size(); ++i) {
        qreal norm = valueToNormalized(mainTicks[i]);
        qreal clampedNorm = qBound(0.0, norm, 1.0);
        QPointF pos = center + dir * (clampedNorm * maxRadius);

        // 主刻度线
        painter->drawLine(pos, pos + tickDir * tickLen);

        // ---------- 文字部分 ----------
        QString label = labels[i];
        QFontMetrics fm(painter->font());
        qreal textWidth = fm.horizontalAdvance(label);
        qreal textHeight = fm.height();

        // 文字偏移：从刻度线终点再往外偏移（即 labelDir 方向）
        qreal offset = AXIS_MARGIN - TICK_LENGTH;  // 刻度线长已占4px，剩余边距给文字
        QPointF textPos = pos + labelDir * offset;

        // 计算文本矩形（水平居中，顶部对齐 textPos）
        QRectF textRect;
        qreal w = textWidth + TEXT_PADDING * 2;
        qreal h = textHeight + TEXT_PADDING * 2;
        qreal x = textPos.x() - w / 2.0;          // 水平居中
        qreal y = textPos.y();                    // 顶部对齐（因为标签在射线下方）
        textRect = QRectF(x, y, w, h);

        // 调试用红色边框（与基类一致）
        painter->save();
        painter->setPen(Qt::red);
        painter->drawRect(textRect);
        painter->restore();

        // 绘制文字（水平居中，顶部对齐）
        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, label);
    }

    painter->restore();
}