#include "QBarWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QDebug>
#include <QtMath>
#include <cmath>
#include <algorithm>

// ========== 默认调色板 ==========
static const QList<QColor> defaultBarColors() {
    return {
        QColor("#2196F3"), QColor("#F44336"), QColor("#4CAF50"),
        QColor("#FF9800"), QColor("#9C27B0"), QColor("#00BCD4"),
        QColor("#FF5722"), QColor("#3F51B5"), QColor("#8BC34A"),
        QColor("#E91E63")
    };
}

// ========== QBarSet ==========
QBarSet::QBarSet(const QString& label, QObject* parent)
    : QObject(parent), m_label(label)
{
    QList<QColor> pals = defaultBarColors();
    // 用 label 哈希取色（构造时还不知道 index）
    int idx = qHash(label) % pals.size();
    m_color = pals.at(idx);
    m_borderColor = m_color.darker(120);
}

QBarSet::QBarSet(const QString& label, const QList<qreal>& values,
                 const QColor& color, QObject* parent)
    : QObject(parent), m_label(label), m_values(values)
{
    if (color.isValid()) {
        m_color = color;
        m_borderColor = color.darker(120);
    } else {
        QList<QColor> pals = defaultBarColors();
        int idx = qHash(label) % pals.size();
        m_color = pals.at(idx);
        m_borderColor = m_color.darker(120);
    }
}

void QBarSet::setLabel(const QString& l) {
    if (m_label != l) { m_label = l; emit labelChanged(l); }
}

qreal QBarSet::valueAt(int index) const {
    return (index >= 0 && index < m_values.size()) ? m_values.at(index) : 0;
}

void QBarSet::setValue(int index, qreal v) {
    if (index < 0) return;
    if (index >= m_values.size()) m_values.resize(index + 1);
    if (!qFuzzyCompare(m_values[index], v)) {
        m_values[index] = v;
        emit valueChanged(index);
        emit valuesChanged();
    }
}

void QBarSet::setValues(const QVector<qreal>& v) {
    m_values = v;
    emit valuesChanged();
    qDebug() << "[QBarSet]" << m_label << "values:" << v;
}

void QBarSet::append(qreal v) {
    m_values.append(v);
    emit countChanged();
    emit valuesChanged();
}

void QBarSet::remove(int index) {
    if (index >= 0 && index < m_values.size()) {
        m_values.remove(index);
        emit countChanged();
        emit valuesChanged();
    }
}

void QBarSet::setColor(const QColor& c) {
    if (m_color != c) { m_color = c; emit colorChanged(c); }
}

void QBarSet::setBorderColor(const QColor& c) {
    if (m_borderColor != c) { m_borderColor = c; emit borderColorChanged(c); }
}

// ========== QBarWidget ==========
QBarWidget::QBarWidget(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(200, 150);

    // 默认轴连接信号 → 自动重绘
    auto connectAxis = [this](QChartAxis* axis) {
        connect(axis, &QChartAxis::rangeChanged, this, [this](qreal,qreal) { update(); });
    };
    connectAxis(&m_defaultValueAxis);
    connectAxis(&m_defaultCategoryAxis);
}

QBarWidget::~QBarWidget() {
    qDeleteAll(m_barSets);
}

// -- 数据 --
void QBarWidget::addBarSet(QBarSet* set) {
    if (!set) return;
    // 自动配色
    if (!set->color().isValid() || set->color() == QColor()) {
        set->setColor(autoColorForSet(m_barSets.size()));
    }
    m_barSets.append(set);
    set->setParent(this);
    connect(set, &QBarSet::valuesChanged, this, [this]() { update(); });
    connect(set, &QBarSet::colorChanged, this, [this](const QColor&) { update(); });
    qDebug() << "[QBarWidget] addBarSet:" << set->label()
             << "count=" << m_barSets.size();
    update();
}

void QBarWidget::insertBarSet(int index, QBarSet* set) {
    if (!set || index < 0 || index > m_barSets.size()) return;
    if (!set->color().isValid())
        set->setColor(autoColorForSet(index));
    m_barSets.insert(index, set);
    set->setParent(this);
    connect(set, &QBarSet::valuesChanged, this, [this]() { update(); });
    connect(set, &QBarSet::colorChanged, this, [this](const QColor&) { update(); });
    update();
}

void QBarWidget::removeBarSet(int index) {
    if (index < 0 || index >= m_barSets.size()) return;
    delete m_barSets.takeAt(index);
    update();
}

void QBarWidget::removeBarSet(QBarSet* set) {
    int idx = m_barSets.indexOf(set);
    if (idx >= 0) removeBarSet(idx);
}

void QBarWidget::clear() {
    qDeleteAll(m_barSets);
    m_barSets.clear();
    m_categories.clear();
    update();
}

void QBarWidget::setCategories(const QStringList& cats) {
    m_categories = cats;
    m_defaultCategoryAxis.setCategories(cats);
    qDebug() << "[QBarWidget] categories:" << cats;
    update();
}

void QBarWidget::addCategory(const QString& cat) {
    m_categories.append(cat);
    m_defaultCategoryAxis.setCategories(m_categories);
    update();
}

// -- 轴 --
void QBarWidget::setValueAxis(QValueAxis* axis) {
    m_valueAxis = axis ? axis : &m_defaultValueAxis;
    update();
}

void QBarWidget::setCategoryAxis(QChartAxis* axis) {
    m_categoryAxis = axis ? axis : &m_defaultCategoryAxis;
    update();
}

void QBarWidget::setCategoryValues(const QVector<qreal>& values) {
    m_categoryValues = values;
    if (!values.isEmpty()) {
        m_categoryAxis->setMin(*std::min_element(values.begin(), values.end()));
        m_categoryAxis->setMax(*std::max_element(values.begin(), values.end()));
    }
    update();
}

void QBarWidget::setBarLabelsAngle(qreal degrees) {
    m_barLabelsAngle = degrees;
    update();
}

// -- 动画 --
void QBarWidget::setValuesMultiplier(qreal v) {
    v = qBound(0.0, v, 1.0);
    if (!qFuzzyCompare(m_valuesMultiplier, v)) {
        m_valuesMultiplier = v;
        update();
    }
}

// -- 布局 --
void QBarWidget::setBarsType(BarsType t) { m_barsType = t; update(); }
void QBarWidget::setOrientation(Orientation o) { m_orientation = o; update(); }
void QBarWidget::setBarWidth(qreal r) { m_barWidth = qBound(0.1, r, 1.0); update(); }

// -- 标签 --
void QBarWidget::setBarLabelsVisible(bool v) { m_barLabelsVisible = v; update(); }
void QBarWidget::setBarLabelsPosition(LabelsPosition p) { m_barLabelsPosition = p; update(); }
void QBarWidget::setBarLabelsPrecision(int p) { m_barLabelsPrecision = p; update(); }
void QBarWidget::setBarLabelsFormat(const QString& f) { m_barLabelsFormat = f; update(); }

// -- 样式 --
void QBarWidget::setSeriesColors(const QList<QColor>& c) { m_seriesColors = c; update(); }
void QBarWidget::setBarBorderWidth(int w) { m_barBorderWidth = qMax(0, w); update(); }
void QBarWidget::setBarBorderColor(const QColor& c) { m_barBorderColor = c; update(); }
void QBarWidget::setBarRadius(qreal r) { m_barRadius = qMax(0.0, r); update(); }

// ========== 几何计算 ==========
QRectF QBarWidget::plotArea() const {
    return QRectF(kMarginLeft, kMarginTop,
                  width() - kMarginLeft - kMarginRight,
                  height() - kMarginTop - kMarginBottom);
}

QColor QBarWidget::autoColorForSet(int setIndex) const {
    const QList<QColor>& pal = m_seriesColors.isEmpty()
        ? defaultBarColors() : m_seriesColors;
    return pal.at(setIndex % pal.size());
}

// ========== 绘制 ==========
void QBarWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF area = plotArea();
    qDebug() << "[QBarWidget] paintEvent: plotArea=" << area
             << "barSets=" << m_barSets.size() << "cats=" << m_categories.size();

    // 背景
    painter.fillRect(rect(), QColor("#FAFAFA"));
    painter.fillRect(area, Qt::white);

    // 网格 + 轴
    drawAxes(&painter, area);

    // 柱子
    drawBars(&painter, area);

    // 标签
    if (m_barLabelsVisible)
        drawBarLabels(&painter, area);

    // 子类叠加层
    paintOverlay(&painter, area);
}

void QBarWidget::drawAxes(QPainter* p, const QRectF& area)
{
    p->save();
    p->setPen(QPen(m_valueAxis->color(), 1));
    QFont axisFont = p->font();
    axisFont.setPointSize(axisFont.pointSize() - 1);
    p->setFont(axisFont);

    qreal axisLen = (m_orientation == Vertical) ? area.height() : area.width();

    // --- 网格线 (value axis) ---
    if (m_valueAxis->isGridVisible()) {
        QPen gridPen(m_valueAxis->gridColor(), 1, Qt::DashLine);
        p->setPen(gridPen);
        QVector<qreal> ticks = m_valueAxis->tickValues();
        for (qreal t : ticks) {
            qreal pos = m_valueAxis->mapToPixel(t, axisLen);
            if (m_orientation == Vertical) {
                qreal y = area.bottom() - pos;
                if (y >= area.top() && y <= area.bottom())
                    p->drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
            } else {
                qreal x = area.left() + pos;
                if (x >= area.left() && x <= area.right())
                    p->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
            }
        }
    }

    // --- 刻度标签 (value axis) ---
    p->setPen(m_valueAxis->color());
    QVector<qreal> ticks = m_valueAxis->tickValues();
    QStringList labels = m_valueAxis->tickLabels();
    for (int i = 0; i < ticks.size() && i < labels.size(); ++i) {
        qreal pos = m_valueAxis->mapToPixel(ticks[i], axisLen);
        if (m_orientation == Vertical) {
            qreal y = area.bottom() - pos;
            QRectF labelRect(area.left() - 45, y - 10, 40, 20);
            p->drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, labels[i]);
            // 小刻度线
            p->drawLine(QPointF(area.left() - 2, y), QPointF(area.left(), y));
        } else {
            qreal x = area.left() + pos;
            QRectF labelRect(x - 20, area.bottom() + 2, 40, 20);
            p->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, labels[i]);
            p->drawLine(QPointF(x, area.bottom()), QPointF(x, area.bottom() + 2));
        }
    }

    // --- 分类轴标签 ---
    bool valueBased = !m_categoryValues.isEmpty();
    qreal catLen = (m_orientation == Vertical) ? area.width() : area.height();
    QStringList catLabels;
    QVector<qreal> catPositions;
    if (valueBased) {
        // 值定位：每个 bar 的位置显示该 bar 的中心值
        catLabels = m_categoryAxis->tickLabels();
        QVector<qreal> ticks = m_categoryAxis->tickValues();
        for (qreal t : ticks)
            catPositions.append(m_categoryAxis->mapToPixel(t, catLen));
    } else {
        catLabels = m_categoryAxis->tickLabels();
        for (int i = 0; i < catLabels.size(); ++i)
            catPositions.append(m_categoryAxis->mapToPixel(i, catLen));
    }

    for (int i = 0; i < catLabels.size() && i < catPositions.size(); ++i) {
        qreal pos = catPositions[i];
        if (m_orientation == Vertical) {
            qreal x = area.left() + pos;
            p->save();
            if (m_barLabelsAngle != 0) {
                p->translate(x, area.bottom() + 14);
                p->rotate(m_barLabelsAngle);
                p->drawText(0, 0, catLabels[i]);
            } else {
                QRectF labelRect(x - 30, area.bottom() + 2, 60, 20);
                p->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, catLabels[i]);
            }
            p->restore();
        } else {
            qreal y = area.bottom() - pos;
            QRectF labelRect(area.left() - 45, y - 10, 40, 20);
            p->drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, catLabels[i]);
        }
    }

    // --- 轴线 ---
    p->setPen(QPen(m_valueAxis->color(), 1));
    if (m_orientation == Vertical) {
        p->drawLine(area.topLeft(), area.bottomLeft());    // Y 轴
        p->drawLine(area.bottomLeft(), area.bottomRight()); // X 轴
    } else {
        p->drawLine(area.bottomLeft(), area.bottomRight()); // X 轴
        p->drawLine(area.topLeft(), area.bottomLeft());     // Y 轴
    }

    p->restore();
}

void QBarWidget::drawBars(QPainter* p, const QRectF& area)
{
    int nCats = m_categories.size();
    int nSets = m_barSets.size();
    if (nCats == 0 || nSets == 0) return;

    qreal catLen = (m_orientation == Vertical) ? area.width() : area.height();
    qreal valLen = (m_orientation == Vertical) ? area.height() : area.width();
    bool valueBased = !m_categoryValues.isEmpty();
    qreal slotW = valueBased ? 0 : catLen / nCats;

    // 值定位：计算每个 category 的像素位置和柱宽
    QVector<qreal> catPixels(nCats);
    QVector<qreal> catBarWidths(nCats);
    if (valueBased) {
        for (int i = 0; i < nCats; ++i) {
            catPixels[i] = m_categoryAxis->mapToPixel(m_categoryValues[i], catLen);
        }
        // 柱宽 = 相邻中心间距 * barWidth（等宽 bin 假设）
        qreal avgGap = (nCats > 1) ? (catPixels[nCats-1] - catPixels[0]) / (nCats - 1) : catLen / nCats;
        for (int i = 0; i < nCats; ++i) {
            catBarWidths[i] = avgGap * m_barWidth;
        }
    }

    // 计算堆叠基线
    QVector<qreal> stackedBases(nCats, 0);
    QVector<qreal> stackedTotals(nCats, 0);
    if (m_barsType == Stacked || m_barsType == StackedPercent) {
        for (int cat = 0; cat < nCats; ++cat) {
            qreal total = 0;
            for (auto* set : m_barSets)
                total += set->valueAt(cat);
            stackedTotals[cat] = total;
        }
    }

    qDebug() << "[QBarWidget] drawBars: cats=" << nCats << "sets=" << nSets
             << "slotW=" << slotW << "barWidth=" << m_barWidth
             << "type=" << m_barsType;

    for (int setIdx = 0; setIdx < nSets; ++setIdx) {
        QBarSet* set = m_barSets.at(setIdx);
        p->save();

        for (int catIdx = 0; catIdx < nCats; ++catIdx) {
            qreal rawVal = set->valueAt(catIdx);
            if (rawVal <= 0 && m_barsType != StackedPercent) continue;

            qreal val = rawVal * m_valuesMultiplier;
            qreal base = 0;
            qreal displayVal = val;

            if (m_barsType == Stacked) {
                base = stackedBases[catIdx];
                displayVal = val;
                stackedBases[catIdx] += val;
            } else if (m_barsType == StackedPercent) {
                qreal total = stackedTotals[catIdx];
                if (total <= 0) continue;
                qreal pct = (total > 0) ? rawVal / total : 0;
                base = stackedBases[catIdx];
                displayVal = pct * 100.0 * m_valuesMultiplier;
                stackedBases[catIdx] += displayVal / m_valuesMultiplier * m_valuesMultiplier;
            }

            // 像素位置
            qreal basePixel = m_valueAxis->mapToPixel(base, valLen);
            qreal topPixel  = m_valueAxis->mapToPixel(base + displayVal, valLen);

            // 柱的水平位置
            qreal barStart, barPixelW;
            if (valueBased) {
                qreal catCenter = catPixels[catIdx];
                barPixelW = catBarWidths[catIdx] / nSets;
                barStart = catCenter - catBarWidths[catIdx] / 2 + setIdx * barPixelW;
            } else {
                qreal catStart = catIdx * slotW;
                qreal groupW = slotW * m_barWidth;
                qreal groupStart = catStart + (slotW - groupW) / 2;
                barPixelW = groupW / nSets;
                barStart = groupStart + setIdx * barPixelW;
            }

            QRectF barRect;
            if (m_orientation == Vertical) {
                qreal y0 = area.bottom() - basePixel;
                qreal y1 = area.bottom() - topPixel;
                barRect = QRectF(area.left() + barStart,
                                 qMin(y0, y1),
                                 barPixelW,
                                 qAbs(y1 - y0));
            } else {
                qreal x0 = area.left() + basePixel;
                qreal x1 = area.left() + topPixel;
                qreal barY, barH;
                if (valueBased) {
                    qreal c = catPixels[catIdx];
                    barH = catBarWidths[catIdx] / nSets;
                    barY = area.bottom() - c - catBarWidths[catIdx] / 2 + setIdx * barH;
                } else {
                    qreal catStart = catIdx * slotW;
                    barH = slotW * m_barWidth / nSets;
                    barY = area.bottom() - catStart - slotW;
                }
                barRect = QRectF(qMin(x0, x1), barY, qAbs(x1 - x0), barH);
            }

            // 绘制柱
            if (m_barRadius > 0) {
                QPainterPath path;
                path.addRoundedRect(barRect, m_barRadius, m_barRadius);
                p->fillPath(path, set->color());
                if (m_barBorderWidth > 0) {
                    p->setPen(QPen(m_barBorderColor, m_barBorderWidth));
                    p->drawPath(path);
                }
            } else {
                p->fillRect(barRect, set->color());
                if (m_barBorderWidth > 0) {
                    p->setPen(QPen(m_barBorderColor, m_barBorderWidth));
                    p->drawRect(barRect);
                }
            }
        }
        p->restore();
    }
}

void QBarWidget::drawBarLabels(QPainter* p, const QRectF& area)
{
    int nCats = m_categories.size();
    int nSets = m_barSets.size();
    if (nCats == 0 || nSets == 0) return;

    bool valueBased = !m_categoryValues.isEmpty();
    qreal catLen = (m_orientation == Vertical) ? area.width() : area.height();
    qreal valLen = (m_orientation == Vertical) ? area.height() : area.width();
    qreal slotW = valueBased ? 0 : catLen / nCats;
    QVector<qreal> catPixels(nCats), catBarWidths(nCats);
    if (valueBased) {
        for (int i = 0; i < nCats; ++i)
            catPixels[i] = m_categoryAxis->mapToPixel(m_categoryValues[i], catLen);
        qreal avgGap = (nCats > 1) ? (catPixels[nCats-1] - catPixels[0]) / (nCats - 1) : catLen / nCats;
        for (int i = 0; i < nCats; ++i) catBarWidths[i] = avgGap * m_barWidth;
    }

    QVector<qreal> stackedBases(nCats, 0);
    QVector<qreal> stackedTotals(nCats, 0);
    if (m_barsType == Stacked || m_barsType == StackedPercent) {
        for (int cat = 0; cat < nCats; ++cat) {
            qreal total = 0;
            for (auto* set : m_barSets) total += set->valueAt(cat);
            stackedTotals[cat] = total;
        }
    }

    QFont labelFont = p->font();
    labelFont.setPointSize(labelFont.pointSize() - 2);
    p->setFont(labelFont);

    for (int setIdx = 0; setIdx < nSets; ++setIdx) {
        QBarSet* set = m_barSets.at(setIdx);
        for (int catIdx = 0; catIdx < nCats; ++catIdx) {
            qreal rawVal = set->valueAt(catIdx);
            if (rawVal <= 0 && m_barsType != StackedPercent) continue;
            qreal val = rawVal * m_valuesMultiplier;
            qreal base = 0;
            if (m_barsType == Stacked) {
                base = stackedBases[catIdx];
                stackedBases[catIdx] += val;
            } else if (m_barsType == StackedPercent) {
                qreal total = stackedTotals[catIdx];
                if (total <= 0) continue;
                base = stackedBases[catIdx];
                val = (rawVal / total) * 100.0 * m_valuesMultiplier;
                stackedBases[catIdx] += val;
            }

            qreal basePixel = m_valueAxis->mapToPixel(base, valLen);
            qreal topPixel  = m_valueAxis->mapToPixel(base + val, valLen);

            qreal barStart, barPixelW;
            if (valueBased) {
                qreal c = catPixels[catIdx];
                barPixelW = catBarWidths[catIdx] / nSets;
                barStart = c - catBarWidths[catIdx] / 2 + setIdx * barPixelW;
            } else {
                qreal catStart = catIdx * slotW;
                qreal groupW = slotW * m_barWidth;
                qreal groupStart = catStart + (slotW - groupW) / 2;
                barPixelW = groupW / nSets;
                barStart = groupStart + setIdx * barPixelW;
            }

            // 标签文字
            QString label;
            if (!m_barLabelsFormat.isEmpty())
                label = QString::asprintf(qPrintable(m_barLabelsFormat), rawVal);
            else
                label = QString::number(rawVal, 'f', m_barLabelsPrecision);

            QPointF labelPt;
            if (m_orientation == Vertical) {
                qreal xCenter = area.left() + barStart + barPixelW / 2;
                qreal yTop = area.bottom() - topPixel;
                qreal yBottom = area.bottom() - basePixel;
                if (m_barLabelsPosition == Center)
                    labelPt = QPointF(xCenter, (yTop + yBottom) / 2);
                else if (m_barLabelsPosition == InsideEnd)
                    labelPt = QPointF(xCenter, yTop + 4);
                else if (m_barLabelsPosition == InsideBase)
                    labelPt = QPointF(xCenter, yBottom - 4);
                else // OutsideEnd
                    labelPt = QPointF(xCenter, yTop - 4);
            } else {
                qreal xLeft = area.left() + basePixel;
                qreal xRight = area.left() + topPixel;
                qreal yCenter;
                if (valueBased) {
                    qreal c = catPixels[catIdx];
                    yCenter = area.bottom() - c;
                } else {
                    qreal catStart = catIdx * slotW;
                    yCenter = area.bottom() - catStart - slotW / 2;
                }
                if (m_barLabelsPosition == Center)
                    labelPt = QPointF((xLeft + xRight) / 2, yCenter);
                else // OutsideEnd
                    labelPt = QPointF(xRight + 4, yCenter);
            }

            p->setPen(Qt::black);
            QFontMetrics fm(p->font());
            QRect textBounds = fm.boundingRect(label);
            textBounds.moveCenter(labelPt.toPoint());
            p->drawText(textBounds, Qt::AlignCenter, label);
        }
    }
}

void QBarWidget::paintOverlay(QPainter*, const QRectF&)
{
    // 子类重写
}

// ========== 交互 ==========
QPair<int,int> QBarWidget::barAtPos(const QPointF& pos) const
{
    QRectF area = plotArea();
    if (!area.contains(pos)) return {-1, -1};

    int nCats = m_categories.size();
    int nSets = m_barSets.size();
    if (nCats == 0 || nSets == 0) return {-1, -1};

    qreal catLen = (m_orientation == Vertical) ? area.width() : area.height();
    qreal slotW = catLen / nCats;

    int catIdx;
    if (m_orientation == Vertical) {
        catIdx = int((pos.x() - area.left()) / slotW);
    } else {
        catIdx = int((area.bottom() - pos.y()) / slotW);
    }
    if (catIdx < 0 || catIdx >= nCats) return {-1, -1};

    // 简化：返回第一个 set（对 grouped/stacked 都适用）
    // 后续可精确到 set 级别
    return {0, catIdx};
}

void QBarWidget::mouseMoveEvent(QMouseEvent* event)
{
    auto [setIdx, catIdx] = barAtPos(event->pos());
    bool hit = (setIdx >= 0 && catIdx >= 0);
    (void)setIdx; // 暂用 catIdx 判断
    if (hit != (m_hoverCat >= 0) || catIdx != m_hoverCat) {
        // 离开旧
        if (m_hoverCat >= 0) emit barHovered(m_hoverSet, m_hoverCat, false);
        // 进入新
        if (hit) {
            m_hoverSet = setIdx;
            m_hoverCat = catIdx;
            setCursor(Qt::PointingHandCursor);
            emit barHovered(m_hoverSet, m_hoverCat, true);
        } else {
            m_hoverSet = -1;
            m_hoverCat = -1;
            setCursor(Qt::ArrowCursor);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void QBarWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        auto [setIdx, catIdx] = barAtPos(event->pos());
        if (setIdx >= 0 && catIdx >= 0) {
            m_pressSet = setIdx;
            m_pressCat = catIdx;
            emit barPressed(setIdx, catIdx);
            qDebug() << "[QBarWidget] barPressed:" << setIdx << catIdx;
        }
    }
    QWidget::mousePressEvent(event);
}

void QBarWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_pressSet >= 0) {
        auto [setIdx, catIdx] = barAtPos(event->pos());
        if (setIdx == m_pressSet && catIdx == m_pressCat) {
            emit barClicked(setIdx, catIdx);
            qDebug() << "[QBarWidget] barClicked:" << setIdx << catIdx;
        }
        emit barReleased(m_pressSet, m_pressCat);
        m_pressSet = -1;
        m_pressCat = -1;
    }
    QWidget::mouseReleaseEvent(event);
}

void QBarWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    auto [setIdx, catIdx] = barAtPos(event->pos());
    if (setIdx >= 0 && catIdx >= 0) {
        emit barDoubleClicked(setIdx, catIdx);
        qDebug() << "[QBarWidget] barDoubleClicked:" << setIdx << catIdx;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void QBarWidget::leaveEvent(QEvent* event)
{
    if (m_hoverCat >= 0) {
        emit barHovered(m_hoverSet, m_hoverCat, false);
        m_hoverSet = -1;
        m_hoverCat = -1;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::leaveEvent(event);
}
