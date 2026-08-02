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

    // ===== 新接口：toPixel(Data,Data)→Pixel =====
    /// Series 只需实现此方法——用 toPixel 把数据点转成像素后画形状
    /// toPixel 返回 NaN 时跳过该点
    virtual void draw(QPainter* painter,
                      std::function<QPointF(qreal,qreal)> toPixel) const = 0;

    // ===== 旧接口（待删除，子类尚未迁移）=====
    [[deprecated]]
    virtual void drawLegacy(QPainter* painter,
        const QChartGeometry* geometry,
        const QChartAxis* axisX,
        const QChartAxis* axisY,
        const QChartProjection* projection) const { Q_UNUSED(painter); Q_UNUSED(geometry); Q_UNUSED(axisX); Q_UNUSED(axisY); Q_UNUSED(projection); }

    /// 命中检测：返回命中数据点索引，-1 未命中
    virtual int hitTest(const QPointF& pixel,
                        std::function<QPointF(qreal,qreal)> toPixel) const { Q_UNUSED(pixel); Q_UNUSED(toPixel); return -1; }

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
