#ifndef QXYSERIES_H
#define QXYSERIES_H

#include <QObject>
#include <QVector>
#include <QColor>
#include <QString>

struct QXYPoint {
    qreal x = 0;
    qreal y = 0;
    QXYPoint() = default;
    QXYPoint(qreal _x, qreal _y) : x(_x), y(_y) {}
};

class QXYSeries : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit QXYSeries(const QString& name = {}, QObject* parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString& n);

    int count() const { return m_points.size(); }
    void append(qreal x, qreal y);
    void append(const QXYPoint& p);
    void insert(int index, qreal x, qreal y);
    void remove(int index);
    void replace(int index, qreal x, qreal y);
    void clear();
    QXYPoint at(int index) const;
    QVector<QXYPoint> points() const { return m_points; }
    void setPoints(const QVector<QXYPoint>& pts);

    QColor color() const { return m_color; }
    void setColor(const QColor& c);

    // 生成测试数据
    static QXYSeries* sinusoidal(const QString& name, int n, qreal amp, qreal freq, qreal phase, QObject* parent = nullptr);
    static QXYSeries* randomScatter(const QString& name, int n, qreal xMean, qreal xSd, qreal yMean, qreal ySd, QObject* parent = nullptr);

signals:
    void countChanged();
    void pointsChanged();
    void pointAdded(int index);
    void pointRemoved(int index);
    void pointReplaced(int index);
    void nameChanged(const QString& name);
    void colorChanged(const QColor& color);

private:
    QString m_name;
    QVector<QXYPoint> m_points;
    QColor m_color;
};

#endif // QXYSERIES_H
