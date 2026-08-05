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
    /// toPixel: Data(QVariant, QVariant) → Pixel。NaN 的点应跳过。
    /// ctx: 可选。提供 toNumeric*/toPixelCurve 用于通用坐标系下曲线边。
    ///      nullptr → 像素空间直线（Cartesian 下的快路径/旧行为）
    virtual void draw(QPainter* painter,
                      std::function<QPointF(QVariant,QVariant)> toPixel,
                      const struct DrawContext* ctx = nullptr) const = 0;

    // ===== 命中检测 =====
    virtual int hitTest(const QPointF& pixel,
                        std::function<QPointF(QVariant,QVariant)> toPixel,
                        const struct DrawContext* ctx = nullptr) const;

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
