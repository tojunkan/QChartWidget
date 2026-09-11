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
    // QChartLayer(QChartAbstractProjection* projection, QRectF plotArea, QObject* parent = nullptr);
    // ↑ 待 Widget 阶段恢复完整实现（当前仅旧声明保留，未编译引用）

    ~QChartLayer() override;

    // ===== 相机（批次 A：相机归 layer——值成员自持，scene.camera 指向它）=====
    QChartCamera* camera() { return &m_camera; }
    const QChartCamera* camera() const { return &m_camera; }
    /// 注入场景上下文（widget 渲染前调用）
    void setSceneProjection(const QChartAbstractProjection* p) { m_scene.projection = p; }
    void setScenePlotArea(const QRectF& plotArea) { m_scene.plotArea = plotArea; }
    void setSceneBackground(const QColor& c) { m_scene.backgroundColor = c; }
    /// 设置 Numeric 数据范围（legacy 取向：left/right=dim0 极值、bottom/top=dim1 极值）
    /// 并同步到已绑定的 axisX/axisY 语法糖范围（widget viewRect→dataBounds 驱动链末端）
    void setNumericBounds(const QRectF& bounds);
    /// 收集结果快照（collectPrimitives 之后使用；scene.camera 恒指向本层 &m_camera）
    const QChartScene& scene() const { return m_scene; }
    QChartScene& scene() { return m_scene; }

    // ===== 轴绑定 =====
    QChartAxis* axisX() const { return m_axisX; }
    QChartAxis* axisY() const { return m_axisY; }
    void setAxisX(QChartAxis* a);
    void setAxisY(QChartAxis* a);
    virtual bool validateAxes() const;

    // ===== Series 管理（待 Series 阶段随 QChartSeries.cpp 一起恢复）=====
    // void addSeries(QChartSeries* s);
    // void removeSeries(QChartSeries* s);
    // QList<QChartSeries*> seriesList() const { return m_series; }
    // void clearSeries();

    // ===== 绘制（由 QChartWidget 调用）=====

    /// 画网格：用 axisX/axisY 的 tickValues 作为 offset，画数据主脊（轴脊 + 刻度点）；
    /// 批次2（A）起每条网格脊只出 1 个标签，且以自由标签提交（全 NaN 锚点 / refId=-1 /
    /// sourceId=本脊组号 → 由 renderer 的"同组组尾最后可见图元"机制定位），
    /// 文字取固定坐标所属值轴对该刻度的文字（水平脊 y=t → y 轴；垂直脊 x=t → x 轴）。
    /// ★ 后端差异（既定契约）：GL（纯 GPU 后端）不渲染自由标签 → 本网格脊标签在 GL 后端
    ///   不显示（已知缺陷、接受差异）；tier1/tier2 两类标签不受影响。混合后端预研旁路见
    ///   QChartRenderer::hybridResolveFreeLabelAnchor（当前不启用）。
    void drawGrid(QChartScene& scene);
    void collectPrimitives();
    void invalidateData() { m_dataDirty = true; }
    // void drawAllSeries(QChartScene& scene);   // 待 Series 阶段恢复

    // ===== 命中检测（Phase 3 任务 0：定义提升到 QChartHitTester）=====
    using HitResult = QChartHitTester::HitResult;
    // HitResult hitTest(const QPointF& pixel, const DrawContext& ctx) const;
    // ↑ S0：拾取整体后置。旧实现依赖已删除的 makeToPixel/旧 DrawContext 字段，
    //   待拾取（hitTest）随后续阶段恢复时与 QChartHitTester 一起重新接入。

    // ===== 交互 =====
    // 批次 A：默认空实现（.cpp 已有空体）；交互/数据链阶段可视需要恢复纯虚
    virtual void recomputeDataBounds();

signals:
    // seriesAdded(QChartSeries*)/seriesRemoved(QChartSeries*) 待 Series 阶段恢复
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

    // hookSeriesDirty/unhookSeriesDirty 待 Series 阶段随 QChartSeries.cpp 恢复

    /// 网格脊标签策略（批次2 A）：图层决定哪些脊使用"单标签"模式。
    /// 当前策略 = 全部网格脊使用 LabelMode::Single（每脊 1 个自由标签；GL 后端不渲染自由标签，
    /// 该差异为既定契约）；
    /// 子类可覆写以让部分/全部脊不出标签（None）或按刻度出标签（Tickwise）。
    virtual QChartAxis::LabelMode gridSpineLabelMode() const { return QChartAxis::LabelMode::Single; }

    QChartAxis *m_axisX = nullptr;
    QChartAxis *m_axisY = nullptr;

    QRectF m_dataBounds; // 通过 axisX/axisY 的 min/m_max 计算得出，供 drawGrid/collectPrimitives 使用
    QChartCamera m_camera;  // ★ 相机值成员（批次 A：相机归 layer；scene.camera=&m_camera 于构造注入）
    QChartScene m_scene;  // 当前场景快照（collectPrimitives 填充；渲染上下文由 widget 注入）
    bool m_dataDirty = true;
    // QList<QChartSeries*> m_series;   // 待 Series 阶段恢复
    bool m_gridVisible = true;
    std::optional<QColor> m_gridColorOverride;           // 用户显式设过（setGridColor）
    QColor m_themeGridColor = QColor(220, 220, 220);     // 主题注入默认（setThemeGridColor）
};

#endif // QCHARTLAYER_H
