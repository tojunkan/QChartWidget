// QScatterSeries.h —— 散点系列
// 每个数据点收集为一个 Point 图元（Numeric 空间），绘制由 renderer 统一完成。
// 只存 Data，数值化用调用时传入的轴（collectPrimitives 的 ax/ay），不知道 Axis/Projection 类型。
#pragma once
#include "QXYSeries.h"
#include <QPen>

class QScatterSeries : public QXYSeries
{
    Q_OBJECT
    Q_PROPERTY(int markerSize READ markerSize WRITE setMarkerSize NOTIFY markerSizeChanged)
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY fillColorChanged)
public:
    // marker 形状
    enum class MarkerShape { Circle, Square, Triangle, Diamond, Plus, Cross };

    explicit QScatterSeries(const QString& name = {}, QObject* parent = nullptr);

    // ===== 图元收集（每点一个 Point，Numeric 空间；az 不用于二维）=====
    void collectPrimitives(QChartScene& scene,
                           const QChartAxis* ax, const QChartAxis* ay,
                           const QChartAxis* az = nullptr) const override;

    // ===== 命中检测：像素到最近数据点的距离 < 阈值 =====
    int hitTest(const QPointF& pixel,
                std::function<QPointF(QVariant,QVariant)> toPixel,
                const DrawContext* ctx = nullptr) const override;

    // ===== marker 样式（NOTIFY 供 QPropertyAnimation 驱动时实时刷新）=====
    MarkerShape markerShape() const { return m_markerShape; }
    void setMarkerShape(MarkerShape s) { m_markerShape = s; }

    int markerSize() const { return m_markerSize; }
    void setMarkerSize(int size) {
        size = qMax(1, size);
        if (m_markerSize == size) return;
        m_markerSize = size;
        emit markerSizeChanged();
    }

    QPen pen() const { return m_pen; }               // 描边，默认 NoPen
    void setPen(const QPen& p) { m_pen = p; }
    QColor fillColor() const { return m_fillColor; } // 填充色，默认透明
    void setFillColor(const QColor& c) {
        if (m_fillColor == c) return;
        m_fillColor = c;
        emit fillColorChanged();
    }

signals:
    void markerSizeChanged();
    void fillColorChanged();

private:
    MarkerShape m_markerShape = MarkerShape::Circle;
    int m_markerSize = 8;
    QPen m_pen = Qt::NoPen;
    QColor m_fillColor = Qt::transparent;
};
