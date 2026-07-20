#ifndef QBARWIDGET_H
#define QBARWIDGET_H

#include <QWidget>
#include <QVector>
#include <QList>
#include <QColor>
#include <QString>
#include <QStringList>
#include <QPair>
#include "QChartAxis.h"

// ========== QBarSet — 数据集 (QObject, 发信号) ==========
class QBarSet : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)

public:
    explicit QBarSet(const QString& label, QObject* parent = nullptr);
    QBarSet(const QString& label, const QList<qreal>& values,
            const QColor& color = QColor(), QObject* parent = nullptr);

    QString label() const { return m_label; }
    void setLabel(const QString& l);

    int count() const { return m_values.size(); }
    qreal valueAt(int index) const;
    void setValue(int index, qreal v);
    QVector<qreal> values() const { return m_values; }
    void setValues(const QVector<qreal>& v);
    void append(qreal v);
    void remove(int index);

    QColor color() const { return m_color; }
    void setColor(const QColor& c);

    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor& c);

signals:
    void valuesChanged();
    void valueChanged(int index);
    void labelChanged(const QString& label);
    void colorChanged(const QColor& color);
    void borderColorChanged(const QColor& color);
    void countChanged();

private:
    QString m_label;
    QVector<qreal> m_values;
    QColor m_color;
    QColor m_borderColor;
};

// ========== QBarWidget — 柱状图 ==========
class QBarWidget : public QWidget
{
    Q_OBJECT
public:
    enum BarsType { Groups, Stacked, StackedPercent };
    enum Orientation { Vertical, Horizontal };
    enum LabelsPosition { Center, InsideEnd, InsideBase, OutsideEnd };

    explicit QBarWidget(QWidget* parent = nullptr);
    ~QBarWidget() override;

    // -- 数据 --
    void addBarSet(QBarSet* set);            // BarWidget 拥有所有权
    void insertBarSet(int index, QBarSet* set);
    void removeBarSet(int index);
    void removeBarSet(QBarSet* set);
    void clear();
    QList<QBarSet*> barSets() const { return m_barSets; }
    int barSetCount() const { return m_barSets.size(); }

    void setCategories(const QStringList& cats);
    QStringList categories() const { return m_categories; }
    void addCategory(const QString& cat);
    int categoryCount() const { return m_categories.size(); }
    // 值定位（用于直方图等，category 对应数值而非字符串标签）
    void setCategoryValues(const QVector<qreal>& values);
    QVector<qreal> categoryValues() const { return m_categoryValues; }

    // -- 轴 --
    void setValueAxis(QValueAxis* axis);     // 传 nullptr 用内置默认轴
    QValueAxis* valueAxis() const { return m_valueAxis; }
    void setCategoryAxis(QChartAxis* axis);
    QChartAxis* categoryAxis() const { return m_categoryAxis; }

    // -- 动画钩子 --
    void setValuesMultiplier(qreal v);
    qreal valuesMultiplier() const { return m_valuesMultiplier; }

    // -- 布局 --
    void setBarsType(BarsType t);
    void setOrientation(Orientation o);
    Orientation orientation() const { return m_orientation; }
    void setBarWidth(qreal ratio);   // 0~1, 默认 0.7

    // -- 标签 --
    void setBarLabelsVisible(bool v);
    void setBarLabelsPosition(LabelsPosition p);
    void setBarLabelsPrecision(int p);
    void setBarLabelsFormat(const QString& fmt);
    void setBarLabelsAngle(qreal degrees);
    qreal barLabelsAngle() const { return m_barLabelsAngle; }

    // -- 颜色 --
    void setSeriesColors(const QList<QColor>& colors);
    QList<QColor> seriesColors() const { return m_seriesColors; }

    // -- 样式 --
    void setBarBorderWidth(int w);
    void setBarBorderColor(const QColor& c);
    void setBarRadius(qreal r);

    // -- 交互 --
    QPair<int,int> barAtPos(const QPointF& pos) const;

signals:
    void barClicked(int setIndex, int catIndex);
    void barDoubleClicked(int setIndex, int catIndex);
    void barPressed(int setIndex, int catIndex);
    void barReleased(int setIndex, int catIndex);
    void barHovered(int setIndex, int catIndex, bool entered);

protected:
    void paintEvent(QPaintEvent* event) override;
    virtual void paintOverlay(QPainter* p, const QRectF& plotArea);
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // 绘图辅助
    QRectF plotArea() const;
    void drawAxes(QPainter* p, const QRectF& area);
    virtual void drawBars(QPainter* p, const QRectF& area);
    virtual void drawBarLabels(QPainter* p, const QRectF& area);
    // 柱矩形计算
    QRectF barRect(int setIdx, int catIdx, const QRectF& area,
                   const QVector<qreal>& stackedBases) const;
    // 自动配色
    QColor autoColorForSet(int setIndex) const;

private:
    // 数据
    QList<QBarSet*> m_barSets;              // 拥有所有权
    QStringList m_categories;

    // 轴 — 默认内嵌
    QValueAxis m_defaultValueAxis;
    QBarCategoryAxis m_defaultCategoryAxis;
    QValueAxis* m_valueAxis = &m_defaultValueAxis;
    QChartAxis* m_categoryAxis = &m_defaultCategoryAxis;
    QVector<qreal> m_categoryValues;      // 值定位（非空时覆盖 categorical 定位）

    // 布局
    BarsType m_barsType = Groups;
    Orientation m_orientation = Vertical;
    qreal m_barWidth = 0.7;          // 柱宽占槽宽比
    qreal m_valuesMultiplier = 1.0;

    // 标签
    bool m_barLabelsVisible = false;
    LabelsPosition m_barLabelsPosition = OutsideEnd;
    int m_barLabelsPrecision = 1;
    QString m_barLabelsFormat;

    // 样式
    QList<QColor> m_seriesColors;    // 空=用默认色
    int m_barBorderWidth = 0;
    QColor m_barBorderColor = Qt::white;
    qreal m_barRadius = 0;
    qreal m_barLabelsAngle = 0;          // 标签旋转角（度）

    // 交互状态
    int m_hoverSet = -1, m_hoverCat = -1;
    int m_pressSet = -1, m_pressCat = -1;

    // 边距
    static constexpr int kMarginLeft = 50;
    static constexpr int kMarginRight = 20;
    static constexpr int kMarginTop = 20;
    static constexpr int kMarginBottom = 40;
};

#endif // QBARWIDGET_H
