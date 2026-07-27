#ifndef QCHARTSERIES_H
#define QCHARTSERIES_H
#include "QChartProjection.h"
#include <QObject>
#include <QString>
#include <QColor>
#include <QRectF>
#include <QPainter>

class QChartGeometry;
class QChartAxis;

class QChartSeries : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
        Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit QChartSeries(const QString& name={}, QObject* parent=nullptr);
    virtual ~QChartSeries() = default;

    virtual CoordinateSystem coordinateSystem() const = 0;

    QString name() const { return m_name; }
    void setName(const QString& n);
    bool isVisible() const { return m_visible; }
    void setVisible(bool v);
    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal o);

    QColor color() const { return m_color; }
    void setColor(const QColor& c) { 
        if (m_color == c)return;
        m_color = c; 
        emit colorChanged();
    }

    virtual int count() const { return 0; }
    virtual QRectF boundingRect() const { return {}; }

    virtual void draw(QPainter* painter,
        const QChartGeometry* geometry,
        const QChartAxis* axisX,
        const QChartAxis* axisY, 
        const QChartProjection* projection) const = 0;

signals:
    void nameChanged(const QString&);
    void visibleChanged();
    void opacityChanged();
    void dataChanged();
    void colorChanged();

protected:
    QString m_name;
    bool m_visible = true;
    qreal m_opacity = 1.0;
    QColor m_color;
};
#endif //!QCHARTSERIES_H
