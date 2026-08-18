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
#include <optional>
#include "QChartAxis.h" // DrawContext 在此定义
#include "QChartHitTester.h"   // 统一命中引擎（Phase 3 任务 0）：HitResult 定义提升于此

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
    /// 统一 HitResult（Phase 3 任务 0：定义提升到 QChartHitTester，本类保留别名，调用方零改动）
    using HitResult = QChartHitTester::HitResult;
    HitResult hitTest(const QPointF& pixel, const DrawContext& ctx) const;

signals:
    void seriesAdded(QChartSeries*);
    void seriesRemoved(QChartSeries*);
    void gridChanged();

public:
    // ===== Grid 样式 =====
    bool isGridVisible() const { return m_gridVisible; }
    void setGridVisible(bool v);
    QColor gridColor() const { return m_gridColorOverride.value_or(m_themeGridColor); }
    void setGridColor(const QColor& c);
    /// 主题注入默认网格色（内部，Widget 推送）：仅当无显式覆盖时才真正变化
    void setThemeGridColor(const QColor& c) {
        m_themeGridColor = c;
        if (!m_gridColorOverride) emit gridChanged();
    }
    /// 清除显式覆盖，回到主题默认网格色
    void clearGridColor() {
        if (!m_gridColorOverride) return;
        m_gridColorOverride.reset();
        emit gridChanged();
    }
    std::optional<QColor> gridColorOverride() const { return m_gridColorOverride; }

protected:
    /// 组装 toPixel(lambda): Data(QVariant,QVariant) → Pixel(px,py)
    /// 同时注入 toNumeric0/toNumeric1 到 ctx（Series 画曲线边用）
    std::function<QPointF(QVariant,QVariant)> makeToPixel(DrawContext& ctx) const;

    QChartAxis *m_axisX = nullptr;
    QChartAxis *m_axisY = nullptr;
    QList<QChartSeries*> m_series;
    CoordinateSystem m_coordSys = CoordinateSystem::Cartesian;
    bool m_gridVisible = true;
    std::optional<QColor> m_gridColorOverride;           // 用户显式设过（setGridColor）
    QColor m_themeGridColor = QColor(220, 220, 220);     // 主题注入默认（setThemeGridColor）
};

#endif // QCHARTLAYER_H
