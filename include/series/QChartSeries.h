// QChartSeries.h —— 系列基类
// 五空间链中：Series 只存 Data 空间的数据（QVariant），不参与坐标变换。
// 所有坐标变换由 Layer 注入的 toPixel 函数完成。
#ifndef QCHARTSERIES_H
#define QCHARTSERIES_H
#include <QObject>
#include <QString>
#include <QColor>
#include <QRectF>
#include <QPainter>
#include <QVariant>
#include <functional>
#include <optional>

class QChartScene;   // 收集目标（series 只往 scene.primitives 追加，不持有）
class QChartAxis;    // 数据数值化：axis->toNumeric(QVariant) → Numeric

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

    // ===== 图元收集（纯虚）=====
    /// 把本系列的数据收集为 Numeric 图元，追加到 scene.primitives（绘制统一由 renderer 完成）。
    /// ax/ay/az：调用时临时传入的轴指针（series 不持有）；nullptr → 按 QVariant::toDouble() 直通。
    /// 二维系列忽略 az（默认 nullptr）；三维系列用满三根轴。
    /// 非法（NaN/Inf）点由实现跳过；sourceId/penWidth 的组包装由 layer 统一处理。
    virtual void collectPrimitives(QChartScene& scene,
                                   const QChartAxis* ax, const QChartAxis* ay,
                                   const QChartAxis* az = nullptr) const = 0;

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

    QColor color() const { return m_colorOverride.value_or(m_themeColor); }
    /// 用户显式设色（A3：写 override，永久盖过主题直到 clearColor）
    void setColor(const QColor& c) {
        if (m_colorOverride && *m_colorOverride == c) return;
        m_colorOverride = c;
        emit colorChanged();
    }
    /// 主题注入默认色（内部，Widget 推送）：仅当无显式覆盖时才真正变化
    void setThemeColor(const QColor& c) {
        m_themeColor = c;
        if (!m_colorOverride) emit colorChanged();
    }
    /// 清除显式覆盖，回到主题默认色
    void clearColor() {
        if (!m_colorOverride) return;
        m_colorOverride.reset();
        emit colorChanged();
    }
    /// 显式覆盖（供主题/调色板判断）
    std::optional<QColor> colorOverride() const { return m_colorOverride; }

signals:
    void nameChanged(const QString&);
    void visibleChanged();
    void opacityChanged();
    void colorChanged();
    void dataChanged();  // 数据点数量/内容变化（触发 Layer 重绘）

protected:
    QString m_name;
    bool m_visible = true;
    qreal m_opacity = 1.0;
    std::optional<QColor> m_colorOverride;   // 用户显式设过（setColor）
    QColor m_themeColor;                     // 主题注入默认（setThemeColor）
};
#endif // QCHARTSERIES_H
