// QChartSeries.h —— 系列基类
// 五空间链中：Series 只存 Data 空间的数据（QVariant），不参与坐标变换。
// 所有坐标变换由 Geometry 注入的 toPixel 函数完成。
#ifndef QCHARTSERIES_H
#define QCHARTSERIES_H
#include <QObject>
#include <QString>
#include <QColor>
#include <QRectF>
#include <QPainter>
#include <QVariant>
#include <functional>

class QChartSeries : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit QChartSeries(const QString& name = {}, QObject* parent = nullptr);
    virtual ~QChartSeries() = default;

    // ===== 数据 =====
    /// 数据点数量
    virtual int count() const = 0;

    // ===== 绘制（纯虚）=====
    /// toPixel: Data(QVariant, QVariant) → Pixel。
    /// 返回 NaN 的点应跳过不画。Series 不知道 Axis 类型，全靠注入的函数。
    virtual void draw(QPainter* painter,
                      std::function<QPointF(QVariant,QVariant)> toPixel) const = 0;

    // ===== 命中检测 =====
    /// 返回命中数据点索引，-1 未命中。默认实现调用 draw 相同路径逐点比较。
    virtual int hitTest(const QPointF& pixel,
                        std::function<QPointF(QVariant,QVariant)> toPixel) const;

    // ===== 样式 =====
    QString name() const { return m_name; }
    void setName(const QString& n);
    bool isVisible() const { return m_visible; }
    void setVisible(bool v);
    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal o);

    QColor color() const { return m_color; }
    void setColor(const QColor& c) {
        if (m_color == c) return;
        m_color = c;
        emit colorChanged();
    }

signals:
    void nameChanged(const QString&);
    void visibleChanged();
    void opacityChanged();
    void colorChanged();

protected:
    QString m_name;
    bool m_visible = true;
    qreal m_opacity = 1.0;
    QColor m_color;
};
#endif // QCHARTSERIES_H
