// QChartLayer.h —— 图层基类
// 持有 axisX/axisY 和 Series 列表，组装坐标转换链 (Data→toNumeric→toCartesian→cartesianToPixel)
// 负责 drawGrid 和 drawAllSeries
#ifndef QCHARTLAYER_H
#define QCHARTLAYER_H
#include <QObject>
#include <QList>
#include <QRectF>
#include <QPointF>
#include <QPainter>
#include <QColor>
#include <functional>
#include "QChartAxis.h" // DrawContext 在此定义

class QChartSeries;

class QChartLayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible NOTIFY gridChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY gridChanged)
public:
    explicit QChartLayer(QObject* parent = nullptr);
    ~QChartLayer() override;

    /// 坐标系类型——由 Widget 在 setProjection / addLayer 时同步更新
    /// 默认 Cartesian。子类初始构造时可预设，Widget 随后覆盖
    CoordinateSystem coordinateSystem() const { return m_coordSys; }
    void setCoordinateSystem(CoordinateSystem cs) { m_coordSys = cs; }

    // ===== 轴绑定 =====
    QChartAxis* axisX() const { return m_axisX; }
    QChartAxis* axisY() const { return m_axisY; }
    void setAxisX(QChartAxis* a);
    void setAxisY(QChartAxis* a);
    virtual bool validateAxes() const;

    // ===== Series 管理 =====
    void addSeries(QChartSeries* s);
    void removeSeries(QChartSeries* s);
    QList<QChartSeries*> seriesList() const { return m_series; }
    void clearSeries();

    // ===== 绘制（由 QChartWidget 调用）=====
    /// 遍历系列，组装 toPixel，调用 series->draw
    void drawAllSeries(QPainter* p, const DrawContext& ctx);

    /// 画网格：用 axisX/axisY 的 tickValues 作为 offset，画数据主脊（只画轴线，无标签刻度）
    void drawGrid(QPainter* p, const DrawContext& ctx) const;

    // ===== 命中检测 =====
    struct HitResult { QChartSeries* series = nullptr; int index = -1; };
    HitResult hitTest(const QPointF& pixel, const DrawContext& ctx) const;

signals:
    void seriesAdded(QChartSeries*);
    void seriesRemoved(QChartSeries*);
    void gridChanged();

public:
    // ===== Grid 样式 =====
    bool isGridVisible() const { return m_gridVisible; }
    void setGridVisible(bool v);
    QColor gridColor() const { return m_gridColor; }
    void setGridColor(const QColor& c);

protected:
    /// 组装 toPixel(lambda): Data(QVariant,QVariant) → Pixel(px,py)
    /// 同时注入 toNumeric0/toNumeric1 到 ctx（Series 画曲线边用）
    std::function<QPointF(QVariant,QVariant)> makeToPixel(DrawContext& ctx) const;

    QChartAxis *m_axisX = nullptr;
    QChartAxis *m_axisY = nullptr;
    QList<QChartSeries*> m_series;
    CoordinateSystem m_coordSys = CoordinateSystem::Cartesian;
    bool m_gridVisible = true;
    QColor m_gridColor = QColor(220, 220, 220);
};

#endif // QCHARTLAYER_H
