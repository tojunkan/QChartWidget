// QChartLayer.h —— 图层基类
// 持有 axisX/axisY 和 Series 列表，从Widget接收plotArea/dataBounds，负责 drawGrid 和 drawAllSeries
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
    QChartLayer(QChartAbstractProjection* projection, QRectF plotArea, QObject* parent = nullptr);
    
    ~QChartLayer() override;

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

    /// 画网格：用 axisX/axisY 的 tickValues 作为 offset，画数据主脊（只画轴线，无标签刻度）
    void drawGrid(QChartScene& scene);
    void drawAllSeries(QChartScene& scene);
    void collectPrimitives();
    void invalidateData() { m_dataDirty = true; }

    // ===== 命中检测 =====
    /// 统一 HitResult（Phase 3 任务 0：定义提升到 QChartHitTester，本类保留别名，调用方零改动）
    using HitResult = QChartHitTester::HitResult;
    HitResult hitTest(const QPointF& pixel, const DrawContext& ctx) const;

    // ===== 交互 =====
    virtual void recomputeDataBounds() = 0;

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

    void hookSeriesDirty(QChartSeries* s);
    void unhookSeriesDirty(QChartSeries* s);

    QChartAxis *m_axisX = nullptr;
    QChartAxis *m_axisY = nullptr;

    QRectF m_dataBounds; // 通过 axisX/axisY 的 min/m_max 计算得出，供 drawGrid/collectPrimitives 使用
    QChartScene m_scene;  // 当前场景快照（由 buildScene 填充）
    bool m_dataDirty = true;
    QList<QChartSeries*> m_series;
    bool m_gridVisible = true;
    std::optional<QColor> m_gridColorOverride;           // 用户显式设过（setGridColor）
    QColor m_themeGridColor = QColor(220, 220, 220);     // 主题注入默认（setThemeGridColor）
};

#endif // QCHARTLAYER_H
