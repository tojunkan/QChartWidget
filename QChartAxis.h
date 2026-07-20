#ifndef QCHARTAXIS_H
#define QCHARTAXIS_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QDateTime>
#include <QtMath>

// ========== 基类 ==========
class QChartAxis : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal min READ min WRITE setMin NOTIFY rangeChanged)
    Q_PROPERTY(qreal max READ max WRITE setMax NOTIFY rangeChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor)
    Q_PROPERTY(int tickCount READ tickCount WRITE setTickCount)
    Q_PROPERTY(int subTickCount READ subTickCount WRITE setSubTickCount)

public:
    explicit QChartAxis(QObject* parent = nullptr);
    virtual ~QChartAxis() = default;

    // 范围
    qreal min() const { return m_min; }
    qreal max() const { return m_max; }
    virtual void setMin(qreal v) { m_min = v; emit rangeChanged(m_min, m_max); }
    virtual void setMax(qreal v) { m_max = v; emit rangeChanged(m_min, m_max); }

    // 核心映射
    virtual qreal mapToPixel(qreal value, qreal axisLength) const = 0;
    virtual qreal pixelToValue(qreal pixel, qreal axisLength) const = 0;

    // 刻度
    int tickCount() const { return m_tickCount; }
    void setTickCount(int n);
    int subTickCount() const { return m_subTickCount; }
    void setSubTickCount(int n) { m_subTickCount = n; }
    virtual QVector<qreal> tickValues() const = 0;
    virtual QStringList tickLabels() const = 0;

    // 样式
    bool isVisible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; emit visibleChanged(); }
    bool isGridVisible() const { return m_gridVisible; }
    void setGridVisible(bool v) { m_gridVisible = v; }
    QString title() const { return m_title; }
    void setTitle(const QString& t) { m_title = t; }
    QColor color() const { return m_color; }
    void setColor(const QColor& c) { m_color = c; }
    QColor gridColor() const { return m_gridColor; }
    void setGridColor(const QColor& c) { m_gridColor = c; }

signals:
    void rangeChanged(qreal min, qreal max);
    void visibleChanged();
    void tickCountChanged();

protected:
    qreal m_min = 0;
    qreal m_max = 10;
    int m_tickCount = 5;
    int m_subTickCount = 0;
    bool m_visible = true;
    bool m_gridVisible = true;
    QString m_title;
    QColor m_color = Qt::black;
    QColor m_gridColor = QColor(220, 220, 220);
};

// ========== QValueAxis — 线性数值轴 ==========
class QValueAxis : public QChartAxis
{
    Q_OBJECT
    Q_PROPERTY(qreal tickInterval READ tickInterval WRITE setTickInterval)
    Q_PROPERTY(int labelDecimals READ labelDecimals WRITE setLabelDecimals)
    Q_PROPERTY(QString labelFormat READ labelFormat WRITE setLabelFormat)

public:
    explicit QValueAxis(QObject* parent = nullptr);

    qreal mapToPixel(qreal value, qreal axisLength) const override;
    qreal pixelToValue(qreal pixel, qreal axisLength) const override;

    qreal tickInterval() const { return m_tickInterval; }
    void setTickInterval(qreal v);   // 0 = auto nice ticks
    int labelDecimals() const { return m_labelDecimals; }
    void setLabelDecimals(int n) { m_labelDecimals = n; }
    QString labelFormat() const { return m_labelFormat; }
    void setLabelFormat(const QString& f) { m_labelFormat = f; }

    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;

private:
    qreal niceStep(qreal range) const;

private:
    qreal m_tickInterval = 0;     // 0 = auto
    int m_labelDecimals = -1;     // -1 = auto
    QString m_labelFormat;        // 空 = 默认
};

// ========== QBarCategoryAxis — 分类轴 ==========
class QBarCategoryAxis : public QChartAxis
{
    Q_OBJECT
    Q_PROPERTY(QStringList categories READ categories WRITE setCategories NOTIFY categoriesChanged)

public:
    explicit QBarCategoryAxis(QObject* parent = nullptr);

    qreal mapToPixel(qreal value, qreal axisLength) const override;
    qreal pixelToValue(qreal pixel, qreal axisLength) const override;

    QStringList categories() const { return m_categories; }
    void setCategories(const QStringList& cats);
    void append(const QString& cat);
    void insert(int index, const QString& cat);
    void remove(const QString& cat);
    void clear();

    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;

signals:
    void categoriesChanged();

private:
    QStringList m_categories;
};

// ========== QLogAxis — 对数轴 ==========
class QLogAxis : public QChartAxis
{
    Q_OBJECT
    Q_PROPERTY(qreal base READ base WRITE setBase)

public:
    explicit QLogAxis(QObject* parent = nullptr);

    qreal base() const { return m_base; }
    void setBase(qreal b);

    qreal mapToPixel(qreal value, qreal axisLength) const override;
    qreal pixelToValue(qreal pixel, qreal axisLength) const override;

    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;

private:
    qreal m_base = 10.0;
};

// ========== QDateTimeAxis — 日期轴 ==========
class QDateTimeAxis : public QChartAxis
{
    Q_OBJECT
    Q_PROPERTY(QString format READ format WRITE setFormat)

public:
    explicit QDateTimeAxis(QObject* parent = nullptr);

    QString format() const { return m_format; }
    void setFormat(const QString& f) { m_format = f; }

    void setRange(QDateTime min, QDateTime max);
    QDateTime dateTimeMin() const { return m_dtMin; }
    QDateTime dateTimeMax() const { return m_dtMax; }

    qreal mapToPixel(qreal value, qreal axisLength) const override;
    qreal pixelToValue(qreal pixel, qreal axisLength) const override;

    QVector<qreal> tickValues() const override;
    QStringList tickLabels() const override;

    // value 是 epoch 秒 (qint64 转 qreal)
    static qreal toEpoch(const QDateTime& dt) { return qreal(dt.toSecsSinceEpoch()); }

private:
    QString m_format = "yyyy-MM-dd";
    QDateTime m_dtMin;
    QDateTime m_dtMax;
};

#endif // QCHARTAXIS_H
