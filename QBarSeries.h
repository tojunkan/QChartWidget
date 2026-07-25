#ifndef QBARSERIES_H
#define QBARSERIES_H

#include "QAbstractSeries.h"
#include "QChartAxis.h"
#include <QVector>
#include <QList>
#include <QString>

// ========== QBarSet ==========
class QBarSet : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit QBarSet(const QString& label, QObject* parent = nullptr);
    QString label() const { return m_label; }
    void setLabel(const QString& l);
    int count() const { return m_values.size(); }
    qreal valueAt(int i) const;
    void setValue(int i, qreal v);
    QVector<qreal> values() const { return m_values; }
    void setValues(const QVector<qreal>& v);
    QColor color() const { return m_color; }
    void setColor(const QColor& c);
signals:
    void valuesChanged(); void valueChanged(int); void labelChanged(const QString&); void colorChanged(const QColor&);
private:
    QString m_label; QVector<qreal> m_values; QColor m_color;
};

// ========== QBarSeries ==========
class QBarSeries : public QAbstractSeries
{
    Q_OBJECT
public:
    enum BarsType { Groups, Stacked, StackedPercent };
    enum Orientation { Vertical, Horizontal };
    enum LabelsPosition { Center, InsideEnd, InsideBase, OutsideEnd };

    explicit QBarSeries(const QString& name = {}, QObject* parent = nullptr);

    // 数据
    void addBarSet(QBarSet* set);
    void removeBarSet(QBarSet* set);
    QList<QBarSet*> barSets() const { return m_barSets; }
    void setCategories(const QStringList& cats) { m_categories = cats; emit dataChanged(); }
    QStringList categories() const { return m_categories; }
    int categoryCount() const { return m_categories.size(); }

    // 布局
    void setBarsType(BarsType t) { m_barsType = t; }
    BarsType barsType() const { return m_barsType; }
    void setBarWidth(qreal r) { m_barWidth = qBound(0.1, r, 1.0); }
    qreal barWidth() const { return m_barWidth; }

    // 标签
    void setBarLabelsVisible(bool v) { m_labelsVisible = v; }
    void setBarLabelsPosition(LabelsPosition p) { m_labelsPos = p; }
    void setBarLabelsPrecision(int p) { m_labelsPrecision = p; }
    void setBarBorderWidth(int w) { m_borderWidth = w; }
    void setBarBorderColor(const QColor& c) { m_borderColor = c; }
    void setBarRadius(qreal r) { m_barRadius = r; }
    qreal barRadius() const { return m_barRadius; }
    void setOrientation(Orientation o) { m_orientation = o; }
    Orientation orientation() const { return m_orientation; }


protected:
    QList<QBarSet*> m_barSets;
    QStringList m_categories;
    BarsType m_barsType = Groups;
    qreal m_barWidth = 0.7;
    bool m_labelsVisible = false;
    LabelsPosition m_labelsPos = OutsideEnd;
    int m_labelsPrecision = 1;
    int m_borderWidth = 0;
    QColor m_borderColor = Qt::white;
    qreal m_barRadius = 0;
    Orientation m_orientation = Vertical;
};

#endif
