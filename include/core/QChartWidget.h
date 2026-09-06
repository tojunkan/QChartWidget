// QChartWidget.h —— 2D 图表控件（S0+批次 A：容器化首改）
// 形态：纯容器（继承 QChartAbstractWidget）——持有唯一 2D Projection（unique_ptr）、
// 唯一的 plotArea 由基类布局计算；layers/axes 便捷管理；viewRect/dataBounds 驱动链
// 经广播/转发 API 作用到各 layer 的相机与轴。不含相机成员、buildScene、图例/导出/拾取/动画。
// 旧成员/方法注释保留并标注“待 <阶段> 阶段恢复”。
#ifndef QCHARTWIDGET_H
#define QCHARTWIDGET_H
#include <QList>
#include <QRectF>
#include <memory>
#include "QChartAbstractWidget.h"
#include "QChartLayer.h"
#include "QChartProjection.h"

class QChartAxis;

class QChartWidget : public QChartAbstractWidget {
    Q_OBJECT
public:
    explicit QChartWidget(QWidget* parent = nullptr);
    ~QChartWidget() override;

    // ===== 图层管理（便捷，转发基类容器）=====
    void addLayer(QChartLayer* layer);
    void removeLayer(QChartLayer* layer);

    // ===== 轴管理（便捷：加到首个图层；图层缺失时先挂起，addLayer 时补挂）=====
    void addAxis(QChartAxis* a);
    void removeAxis(QChartAxis* a);
    QList<QChartAxis*> axes() const { return m_axes; }

    // ===== Projection（容器唯一持有；访问器见 protected 基类覆写投影()）=====
    void setProjection(std::unique_ptr<QChartProjection> proj);

    // ===== 数据范围 / 视窗驱动链（广播到各 layer 相机与轴）=====
    QRectF dataBounds() const { return m_dataBounds; }
    void recomputeDataBounds();                 // viewRect(首层相机) → dataBounds → 轴 setRange 广播
    QRectF viewRect() const;                    // 首层相机 viewRect（无层返回空）
    void setViewRect(const QRectF& r);          // 广播 setViewRect 到所有层相机 + recomputeDataBounds
    void panViewCartesian(qreal dx, qreal dy);
    void zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY);

    // ===== 坐标转换（View Cartesian ↔ Pixel，转发层相机 + 本 widget plotArea）=====
    QPointF cartesianToPixel(qreal cx, qreal cy) const;
    QPointF pixelToCartesian(const QPointF& pixel) const;

    // 旧 API 注释保留（待对应阶段恢复）：
    // viewRectFitMode()/setViewRectFitMode()/scale()/setScale()/FitStrategy fitViewRectToPlotArea()
    //   —— 待相机配置阶段（相机已归 layer，配置 API 将作用于层相机）
    // setTheme(QChartTheme::Preset/const QChartTheme&)/theme()/pushTheme()/setBackgroundColor()/
    // clearBackgroundColor()/backgroundColor()/setFollowSystemPalette()/followSystemPalette()/
    // assignSeriesPaletteColor()/QChartTheme m_theme/m_backgroundColorOverride/m_followSystemPalette/
    // m_seriesColorIndex —— 待 Phase-1 主题阶段恢复
    // legend()/setLegendVisible()/isLegendVisible()/setLegendAlignment()/legendItems()/
    // m_legend/m_legendItems/rebuildLegendItems() —— 待 Phase-1 图例阶段恢复
    // saveAsPng/Svg/Pdf（全部重载）/setExportTransparentBackground()/exportTransparentBackground()/
    // QChartExportScope —— 待导出阶段恢复
    // setTemporaryProjection()/clearTemporaryProjection()/m_tempProjection —— 待动画阶段恢复
    // isCachingEnabled()/setCachingEnabled() —— 缓存已并入 renderer（viewDirty 模型）
    // paintEvent()/resizeEvent()/event()/mouse*/wheel/leaveEvent 覆写 —— 基类已统一 final 分发；交互待交互阶段
    // buildScreenScene()/buildExportScene()/buildHoverTooltip()/dimensionInteractive()/seriesHovered/viewChanged
    //   —— 场景组装由「layer 快照 + renderer 管线」取代；hover/信号待拾取/交互阶段
    // m_camera/m_renderer/m_viewInitialized/m_panEnabled/m_zoomEnabled/m_panStart/m_panning/
    // m_hoverSeries/m_hoverIndex —— 相机归 layer；交互状态待交互阶段恢复
    // （F2/t8 补全，按旧 HEAD 头逐项比对）
    // drawOverlay(QPainter&, const QChartScene&) —— 待 GL overlay/数据标签阶段恢复
    // invalidateLayout() —— 布局脏标记已并入 m_layoutDirty（批次 A 起 relayout 即布局失效）
    // camera()/renderer()/glRenderer()（QChartAbstractWidget 旧 protected 访问器）—— 相机归 layer
    //   （经 layer->camera() 访问）、渲染器为容器内部实现细节（待渲染器配置阶段）
    // setProjection 旧签名 + public projection() —— 现投影经 protected projection() 覆写+setProjection 管理；
    //   如需 public 只读访问器可在后续批次补（QChartAbstractWidget.h 旧 m_legend/m_plotArea 等见该头注释）
    // setDataRangeDim0()/setDataRangeDim1() —— 待 Widget 数据链阶段（现链为 viewRect→recomputeDataBounds→
    //   layer->setNumericBounds→轴 sugar）
    // setPanEnabled/isPanEnabled/setZoomEnabled/isZoomEnabled —— 待交互阶段恢复（钩子已就位）
    // seriesHovered(...) 信号 —— 待拾取/悬停阶段恢复
    //
    // 恢复指引（不依赖记忆）：本清单即恢复点索引；阶段迁移表见
    // docs/stages/S0_axis_pipeline.md（§2 排除清单 / §5 类文档登记 / §6 遗留提示）；
    // 各旧 API 完整旧实现位于用户 git 历史中本文件重构前版本（未执行任何 git 写操作），
    // 恢复时按上表归属阶段（主题 Phase-1 / 图例 Phase-1 / 导出 / 动画 / 交互 / 拾取）取回。

signals:
    // viewChanged()/seriesHovered(...) 待交互阶段恢复（相机变化经层相机 viewChanged 传播）

protected:
    QRectF calculatePlotArea() const override;   // margins + 各层边框轴 sizeHint 外边距
    void layoutAxes();                            // 计算 plotArea + 广播 plotAreaChanged + push 上下文
    void onBeforePaint() override;                // 渲染前：axis sugar → 层相机 viewRect 单次同步
    void drawExternalContent(QPainter& painter) override;   // plotArea 外边框轴 drawAtEdge
    const QChartAbstractProjection* projection() const override { return m_projection.get(); }

private:
    void attachToLayer(QChartLayer* layer, QChartAxis* a);  // 便捷轴挂载（isHorizontal→axisX，否则→axisY）
    bool isAxisAttached(QChartAxis* a) const;

    std::unique_ptr<QChartProjection> m_projection;  // 唯一投影（2D；容器持有，默认 Cartesian）
    QRectF m_dataBounds;                              // 当前 Numeric 范围（viewRect 反算缓存）
    QList<QChartAxis*> m_axes;                        // 便捷轴列表（非持有；转发挂载到图层）
    bool m_viewSynced = false;                        // 相机视图是否已同步（axis→camera 单次链）
};

#endif // QCHARTWIDGET_H
