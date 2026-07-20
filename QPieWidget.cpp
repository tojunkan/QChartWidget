#include "QPieWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>
#include <QtMath>
#include <QDebug>
#include <QToolTip>
#include <QApplication>
#include <QPalette>

static constexpr qreal DEFAULT_EXPLODE_FACTOR = 0.15;

// 默认 Material Design 色系
static const QList<QColor> defaultPalette() {
    return {
        QColor("#2196F3"), // Blue
        QColor("#F44336"), // Red
        QColor("#4CAF50"), // Green
        QColor("#FF9800"), // Orange
        QColor("#9C27B0"), // Purple
        QColor("#00BCD4"), // Cyan
        QColor("#FF5722"), // Deep Orange
        QColor("#3F51B5"), // Indigo
        QColor("#8BC34A"), // Light Green
        QColor("#E91E63"), // Pink
    };
}

// 根据亮度选黑/白文字色
static QColor contrastingTextColor(const QColor& bg) {
    // 相对亮度公式: 0.299*R + 0.587*G + 0.114*B
    qreal luminance = 0.299 * bg.red() + 0.587 * bg.green() + 0.114 * bg.blue();
    return luminance > 140 ? Qt::black : Qt::white;
}

static QColor autoColorForIndex(int index) {
    const auto& pal = defaultPalette();
    return pal.at(index % pal.size());
}

QPieWidget::QPieWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    updateAllExplodeOffsets();
}

QPieWidget::~QPieWidget() = default;

// ---------- 数据管理 ----------
void QPieWidget::appendSlice(const QString& label, qreal value, const QColor& color)
{
    Slice s;
    s.label = label;
    s.value = value;
    if (m_autoColor && !color.isValid()) {
        if (!m_palette.isEmpty())
            s.color = m_palette.at(m_slices.size() % m_palette.size());
        else
            s.color = autoColorForIndex(m_slices.size());
    } else {
        s.color = color.isValid() ? color : Qt::gray;
    }
    s.exploded = false;
    s.explodeTarget = 0.0;
    s.explodeOffset = 0.0;
    s.explodeFactor = DEFAULT_EXPLODE_FACTOR;
    s.anim = nullptr;
    m_slices.append(s);
    int idx = m_slices.size() - 1;
    emit sliceAdded(idx);
    emit countChanged(m_slices.size());
    update();
}

void QPieWidget::insertSlice(int index, const QString& label, qreal value, const QColor& color)
{
    if (index < 0 || index > m_slices.size()) return;
    Slice s;
    s.label = label;
    s.value = value;
    if (m_autoColor && !color.isValid()) {
        if (!m_palette.isEmpty())
            s.color = m_palette.at(m_slices.size() % m_palette.size());
        else
            s.color = autoColorForIndex(m_slices.size());
    } else {
        s.color = color.isValid() ? color : Qt::gray;
    }
    s.exploded = false;
    s.explodeTarget = 0.0;
    s.explodeOffset = 0.0;
    s.explodeFactor = DEFAULT_EXPLODE_FACTOR;
    s.anim = nullptr;
    m_slices.insert(index, s);
    emit sliceAdded(index);
    emit countChanged(m_slices.size());
    update();
}

void QPieWidget::removeSlice(int index)
{
    if (index < 0 || index >= m_slices.size()) return;
    if (m_slices[index].anim) {
        m_slices[index].anim->stop();
        delete m_slices[index].anim;
    }
    m_slices.remove(index);
    emit sliceRemoved(index);
    emit countChanged(m_slices.size());
    update();
}

void QPieWidget::clearSlices()
{
    for (auto& s : m_slices) {
        if (s.anim) {
            s.anim->stop();
            delete s.anim;
            s.anim = nullptr;
        }
    }
    m_slices.clear();
    emit countChanged(0);
    update();
}

QPair<QString, qreal> QPieWidget::sliceData(int index) const
{
    if (index < 0 || index >= m_slices.size()) return {};
    return { m_slices[index].label, m_slices[index].value };
}

void QPieWidget::setSliceValue(int index, qreal value)
{
    if (index < 0 || index >= m_slices.size()) return;
    if (qFuzzyCompare(m_slices[index].value, value)) return;
    m_slices[index].value = value;
    emit sliceValueChanged(index, value);
    update();
}

void QPieWidget::setSliceLabel(int index, const QString& label)
{
    if (index < 0 || index >= m_slices.size()) return;
    if (m_slices[index].label == label) return;
    m_slices[index].label = label;
    emit sliceLabelChanged(index, label);
    update();
}

// ---------- 样式 ----------
void QPieWidget::setHoleSize(qreal size) { m_holeSize = qBound(0.0, size, 0.99); update(); }
void QPieWidget::setStartAngle(qreal angle) { m_startAngle = angle; update(); }
void QPieWidget::setPieSize(qreal relativeSize) { m_pieSize = qBound(0.0, relativeSize, 1.0); update(); }
void QPieWidget::setPiePosition(qreal x, qreal y) { m_pieX = qBound(0.0, x, 1.0); m_pieY = qBound(0.0, y, 1.0); update(); }
void QPieWidget::setAllLabelsVisible(bool visible) { m_labelsVisible = visible; update(); }
void QPieWidget::setAllLabelsPosition(int position) { m_labelPosition = position; update(); }
void QPieWidget::setAllBorderWidth(int width) { m_borderWidth = qMax(0, width); update(); }
void QPieWidget::setAllBorderColor(const QColor& color) { m_borderColor = color; update(); }

void QPieWidget::setPalette(const QList<QColor>& colors)
{
    if (colors.isEmpty()) return;
    m_palette = colors;
    m_autoColor = false;  // 显式设置调色板 = 禁用自动配色
    for (int i = 0; i < m_slices.size(); ++i) {
        m_slices[i].color = colors.at(i % colors.size());
    }
    update();
}

void QPieWidget::setSliceExploded(int index, bool exploded)
{
    if (index < 0 || index >= m_slices.size()) return;
    m_slices[index].exploded = exploded;
    updateExplodeAnimation(index, exploded);
}

void QPieWidget::setSliceExplodeDistanceFactor(int index, qreal factor)
{
    if (index < 0 || index >= m_slices.size()) return;
    m_slices[index].explodeFactor = qBound(0.0, factor, 1.0);
}

void QPieWidget::setAutoColorEnabled(bool enabled)
{
    m_autoColor = enabled;
    update();
}

void QPieWidget::setTooltipEnabled(bool enabled)
{
    m_tooltipEnabled = enabled;
}

// ---------- 几何辅助 ----------
QRectF QPieWidget::pieRect() const
{
    QRectF rect = QRectF(0, 0, width(), height());
    qreal size = qMin(rect.width(), rect.height()) * m_pieSize;
    QPointF center(rect.width() * m_pieX, rect.height() * m_pieY);
    return QRectF(center.x() - size / 2, center.y() - size / 2, size, size);
}

QPointF QPieWidget::pieCenter() const { return pieRect().center(); }
qreal QPieWidget::pieRadius() const { return pieRect().width() / 2; }
qreal QPieWidget::innerRadius() const {
    return pieRadius() * m_holeSize;   // 洞的大小比例，0 表示无洞
}

// ---------- 核心绘制函数 ----------
void QPieWidget::drawSlice(QPainter* painter, const Slice& slice,
                           qreal startAngle, qreal spanAngle,
                           const QPointF& center, qreal radius, qreal innerRadius,
                           bool drawLabel, int labelPos, int borderWidth, const QColor& borderColor,
                           qreal totalValue)
{
    if (spanAngle <= 0) return;

    qDebug() << "  drawSlice: startAngle=" << startAngle << " spanAngle=" << spanAngle
        << " center=(" << center.x() << "," << center.y() << ")"
        << " radius=" << radius << " inner=" << innerRadius;

    // 计算爆炸偏移向量（沿扇形中轴线，屏幕方向用 -sin）
    qreal offset = slice.explodeOffset * radius * slice.explodeFactor;
    QPointF offsetVec;
    if (offset > 0) {
        qreal midAngle = qDegreesToRadians(startAngle + spanAngle / 2.0);
        offsetVec = QPointF(offset * qCos(midAngle), -offset * qSin(midAngle));
    }
    QPointF centerOffset = center + offsetVec;

    qDebug() << "    centerOffset=(" << centerOffset.x() << "," << centerOffset.y() << ")";


    // 构建路径（环形或实心扇形）
    QPainterPath path;
    qreal radStart = qDegreesToRadians(startAngle);
    qreal radEnd = qDegreesToRadians(startAngle + spanAngle);

    qDebug() << "    radStart=" << radStart << " radEnd=" << radEnd;

    if (innerRadius > 0) {
        // 甜甜圈 - 四点计算
        // arcTo 内部用 -sin 算 Y，所以我们的四点也要用 -sin，才能跟弧的端点对上
        QPointF outerStart = centerOffset + QPointF(radius * qCos(radStart), -radius * qSin(radStart));
        QPointF outerEnd   = centerOffset + QPointF(radius * qCos(radEnd), -radius * qSin(radEnd));
        QPointF innerStart = centerOffset + QPointF(innerRadius * qCos(radStart), -innerRadius * qSin(radStart));
        QPointF innerEnd   = centerOffset + QPointF(innerRadius * qCos(radEnd), -innerRadius * qSin(radEnd));

        qDebug() << "    outerStart=(" << outerStart.x() << "," << outerStart.y() << ")";
        qDebug() << "    outerEnd=(" << outerEnd.x() << "," << outerEnd.y() << ")";
        qDebug() << "    innerStart=(" << innerStart.x() << "," << innerStart.y() << ")";
        qDebug() << "    innerEnd=(" << innerEnd.x() << "," << innerEnd.y() << ")";

        // 单一连续路径：外弧 → 径向线 → 反向内弧 → 闭合（不用 addPath 避免 moveTo 断点）
        path.moveTo(outerStart);
        path.arcTo(QRectF(centerOffset.x() - radius, centerOffset.y() - radius,
            radius * 2, radius * 2),
            startAngle, spanAngle);
        path.lineTo(innerEnd);
        path.arcTo(QRectF(centerOffset.x() - innerRadius, centerOffset.y() - innerRadius,
            innerRadius * 2, innerRadius * 2),
            startAngle + spanAngle, -spanAngle);
        path.closeSubpath();

        // 调试：输出路径元素
        qDebug() << "Path elements (" << path.elementCount() << "):";
        for (int j = 0; j < path.elementCount(); ++j) {
            QPainterPath::Element e = path.elementAt(j);
            qDebug() << "  elem" << j << "type=" << e.type
                << " x=" << e.x << " y=" << e.y;
        }
    } else {
        // 实心扇形
        path.moveTo(centerOffset);
        path.arcTo(QRectF(centerOffset.x() - radius, centerOffset.y() - radius,
                          radius * 2, radius * 2),
                   startAngle, spanAngle);
        path.closeSubpath();
        qDebug() << "    Arc rect: (" << centerOffset.x() - radius << "," << centerOffset.y() - radius
            << ") size=" << radius * 2 << "x" << radius * 2;
        qDebug() << "    Arc angles: start=" << startAngle << " span=" << spanAngle;
    }

    // 填充
    painter->fillPath(path, slice.color);

    // 边框
    if (borderWidth > 0) {
        painter->save();
        painter->setPen(QPen(borderColor, borderWidth));
        painter->drawPath(path);
        painter->restore();
    }

    // 标签
    if (drawLabel) {
        qreal labelAngleRad = qDegreesToRadians(startAngle + spanAngle / 2.0);
        qreal labelRadius;
        if (labelPos == 1) { // 外部
            labelRadius = radius + 18;
        } else { // 内部 — 质心位置
            if (innerRadius > 0) {
                // 环形扇区质心半径: 2/3 * (R³-r³) / (R²-r²)
                qreal R3 = radius * radius * radius;
                qreal r3 = innerRadius * innerRadius * innerRadius;
                qreal R2 = radius * radius;
                qreal r2 = innerRadius * innerRadius;
                labelRadius = (2.0 / 3.0) * (R3 - r3) / (R2 - r2);
            } else {
                // 实心扇区质心: 2/3 * R
                labelRadius = radius * 2.0 / 3.0;
            }
        }
        // 屏幕坐标用 -sin 匹配 arcTo 方向
        QPointF labelPosPoint = centerOffset + QPointF(labelRadius * qCos(labelAngleRad),
                                                       -labelRadius * qSin(labelAngleRad));

        QString text = slice.label + QString(" (%1%)").arg(slice.value / totalValue * 100, 0, 'f', 1);
        painter->save();
        // 外部标签跟背景对比，内部标签跟切片对比
        QColor textColor;
        if (labelPos == 1) {
            // 外部：跟 widget 背景色对比
            QColor bg = QApplication::palette().color(QPalette::Window);
            textColor = contrastingTextColor(bg);
        } else {
            textColor = contrastingTextColor(slice.color);
        }
        painter->setPen(textColor);
        QFont font = painter->font();
        font.setPointSize(font.pointSize() - 1);
        painter->setFont(font);
        // 精确量文字尺寸，让文字中心与质心重合
        QFontMetrics fm(painter->font());
        QRect textBounds = fm.boundingRect(QRect(0, 0, 200, 50), Qt::AlignCenter | Qt::TextSingleLine, text);
        textBounds.moveCenter(labelPosPoint.toPoint());
        painter->drawText(textBounds, Qt::AlignCenter | Qt::TextSingleLine, text);
        painter->restore();
    }
}

// ---------- paintEvent ----------
void QPieWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    qDebug() << "=== paintEvent ===";
    if (m_slices.isEmpty()) {
        QPainter painter(this);
        painter.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }

    qreal total = 0;
    for (const auto& s : m_slices) total += s.value;
    if (qFuzzyIsNull(total)) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF rect = pieRect();
    QPointF center = rect.center();
    qreal radius = rect.width() / 2;
    qreal innerR = innerRadius();

    qDebug() << "total=" << total << " count=" << m_slices.size();
    qDebug() << "pieRect:" << rect << " center:" << center << " radius:" << radius << " innerR:" << innerR;

    qreal startAngle = m_startAngle - 90; // 转换为Qt坐标系
    qDebug() << "startAngle (Qt coords) =" << startAngle;

    qreal currentAngle = startAngle;
    for (int i = 0; i < m_slices.size(); ++i) {
        const Slice& slice = m_slices[i];
        qreal spanAngle = (slice.value / total) * 360.0;
        qDebug() << "Slice" << i << "value=" << slice.value << "span=" << spanAngle
                 << "currentAngle=" << currentAngle;
        if (spanAngle <= 0) continue;

        drawSlice(&painter, slice,
                  currentAngle, spanAngle,
                  center, radius, innerR,
                  m_labelsVisible, m_labelPosition,
                  m_borderWidth, m_borderColor,
                  total);

        currentAngle += spanAngle;
        // 规范化到 0~360 方便观察
        currentAngle = fmod(currentAngle, 360.0);
        if (currentAngle < 0) currentAngle += 360.0;
    }
    qDebug() << "=== paintEvent end ===";
}

// ---------- 交互 ----------
int QPieWidget::sliceAtPos(const QPointF& pos) const
{
    if (m_slices.isEmpty()) return -1;

    QPointF center = pieCenter();
    qreal dx = pos.x() - center.x();
    qreal dy = pos.y() - center.y();
    qreal dist = qSqrt(dx * dx + dy * dy);
    qreal radius = pieRadius();
    qreal innerR = innerRadius();

    //qDebug() << "sliceAtPos: pos=(" << pos.x() << "," << pos.y() << ")";
    //qDebug() << "  center=(" << center.x() << "," << center.y() << ")";
    //qDebug() << "  dx=" << dx << " dy=" << dy << " dist=" << dist;
    //qDebug() << "  radius=" << radius << " innerR=" << innerR;

    if (dist < innerR || dist > radius) {
        //qDebug() << "  -> outside annular region";
        return -1;
    }

    // 鼠标屏幕极角（0°=3点钟，CCW 增加）
    qreal angle = qRadiansToDegrees(qAtan2(dy, dx));
    if (angle < 0) angle += 360.0;

    qreal total = 0;
    for (const auto& s : m_slices) total += s.value;
    if (qFuzzyIsNull(total)) return -1;

    // 切片角度在 Qt 参数空间；映射到屏幕空间：screen_angle = -param_angle (mod 360)
    // arcTo 正 sweep = 屏幕 CW（递减角度）
    qreal start = m_startAngle - 90; // Qt 参数空间起始角
    qreal curAngle = start;
    for (int i = 0; i < m_slices.size(); ++i) {
        qreal span = (m_slices[i].value / total) * 360.0;
        if (span <= 0) { curAngle += span; continue; }

        // 参数空间 → 屏幕空间
        qreal startScreen = fmod(-curAngle, 360.0);
        if (startScreen < 0) startScreen += 360.0;
        qreal endScreen = fmod(-(curAngle + span), 360.0);
        if (endScreen < 0) endScreen += 360.0;

        // CW 从 startScreen 到 endScreen（递减角度）
        bool inside = false;
        if (qFuzzyCompare(startScreen, endScreen)) {
            inside = true; // 全圆
        } else if (startScreen > endScreen) {
            // 正常情况：楔形 = [endScreen, startScreen)
            inside = (angle >= endScreen && angle < startScreen);
        } else {
            // 跨越 0°：楔形 = [0, startScreen) ∪ [endScreen, 360)
            inside = (angle < startScreen || angle >= endScreen);
        }
        if (inside) return i;

        curAngle += span;
    }
    return -1;
}

void QPieWidget::mousePressEvent(QMouseEvent* event)
{
    qDebug() << "mousePress at (" << event->pos().x() << "," << event->pos().y() << ")";
    if (event->button() == Qt::LeftButton) {
        int idx = sliceAtPos(event->pos());
        if (idx != -1) {
            m_pressedIndex = idx;
            emit slicePressed(idx);
            qDebug() << "  -> Hit slice index:" << idx;
        }
        else {
            qDebug() << "  -> Hit nothing";
        }
    }
    QWidget::mousePressEvent(event);
}

void QPieWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_pressedIndex != -1) {
            int idx = sliceAtPos(event->pos());
            if (idx == m_pressedIndex) emit sliceClicked(idx);
            emit sliceReleased(m_pressedIndex);
            m_pressedIndex = -1;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void QPieWidget::mouseMoveEvent(QMouseEvent* event)
{
    int idx = sliceAtPos(event->pos());
    if (idx != m_hoverIndex) {
        if (m_hoverIndex != -1) {
            updateExplodeAnimation(m_hoverIndex, false);
            emit sliceHovered(m_hoverIndex, false);
            QToolTip::hideText();
        }
        m_hoverIndex = idx;
        if (m_hoverIndex != -1) {
            updateExplodeAnimation(m_hoverIndex, true);
            emit sliceHovered(m_hoverIndex, true);
            if (m_tooltipEnabled) {
                const auto& s = m_slices.at(idx);
                qreal total = 0;
                for (const auto& sl : m_slices) total += sl.value;
                qreal pct = total > 0 ? (s.value / total * 100.0) : 0;
                QString tip = QString("%1\n%2 (%3%)")
                    .arg(s.label).arg(s.value).arg(pct, 0, 'f', 1);
                QToolTip::showText(event->globalPos(), tip, this);
            }
        }
        setCursor(idx != -1 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void QPieWidget::leaveEvent(QEvent* event)
{
    if (m_hoverIndex != -1) {
        updateExplodeAnimation(m_hoverIndex, false);
        emit sliceHovered(m_hoverIndex, false);
        m_hoverIndex = -1;
        setCursor(Qt::ArrowCursor);
        QToolTip::hideText();
    }
    QWidget::leaveEvent(event);
}

// ---------- 爆炸动画 ----------
void QPieWidget::updateExplodeAnimation(int index, bool on)
{
    if (index < 0 || index >= m_slices.size()) return;
    Slice& slice = m_slices[index];
    slice.exploded = on;
    slice.explodeTarget = on ? 1.0 : 0.0;

    if (slice.anim) {
        slice.anim->stop();
        slice.anim->deleteLater();
        slice.anim = nullptr;
    }

    QVariantAnimation* varAnim = new QVariantAnimation(this);
    varAnim->setDuration(300);
    varAnim->setEasingCurve(QEasingCurve::OutQuad);
    varAnim->setStartValue(slice.explodeOffset);
    varAnim->setEndValue(slice.explodeTarget);
    connect(varAnim, &QVariantAnimation::valueChanged, this, [this, index](const QVariant& value) {
        if (index >= 0 && index < m_slices.size()) {
            m_slices[index].explodeOffset = value.toReal();
            update();
        }
    });
    connect(varAnim, &QVariantAnimation::finished, this, &QPieWidget::onExplodeAnimationFinished);
    varAnim->start();
    slice.anim = varAnim;
}

void QPieWidget::onExplodeAnimationFinished()
{
    // 可选清理
}

void QPieWidget::updateAllExplodeOffsets()
{
    for (auto& s : m_slices) s.explodeOffset = 0.0;
}
