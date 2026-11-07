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

    // ===== 图层管理（单层：转发基类；2D 只接受 QChartLayer）=====
    void addLayer(QChartAbstractLayer* layer);

    // ===== 轴管理（轴在 QChartLayer 构造时传入、运行期不可更换）=====
    // addAxis 只做三件事：登记（参与外边距计算）、连接 widget 侧信号、必要时同步相机窗口。
    void addAxis(QChartAxis* a);
    void removeAxis(QChartAxis* a);   // 已绑定的轴不允许移除（会 qWarning 并返回）
    QList<QChartAxis*> axes() const { return m_axes; }

    // ===== Projection（容器唯一持有；访问器见 protected 基类覆写投影()）=====
    void setProjection(std::unique_ptr<QChartProjection> proj);

    // ===== 数据范围 / 视窗驱动链（轴 ↔ 相机，双向由信号/直接调用协调）=====
    /// 按需从层/轴临时组装（正常 QRectF：left/right=dim0 轴范围、top/bottom=dim1 轴范围；无层/轴→空）。
    QRectF dataBounds() const;
    QRectF viewRect() const;                    // 二维层相机 viewRect（无层返回空）
    void setViewRect(const QRectF& r);          // 相机侧操作：设窗口并反算轴
    void panViewCartesian(qreal dx, qreal dy);
    void zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY);

    // ===== 4f：鼠标交互（事件 → 相机像素级操作；开关见基类 setInteractionEnabled）=====
    /// 左键拖动 = panByPixels；滚轮 = zoomAtPixel（中心处数据坐标不变）。
    static qreal wheelZoomFactor(int angleDeltaY);                    // 纯计算（120/格 → 1.15 倍）

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
    // setDataRangeDim0()/setDataRangeDim1() —— 已废弃（4b）：轴是范围的唯一持有者，不再经 Widget 映射；
    //   数据链为 viewRect→recomputeDataBounds→直接写各轴 setRange
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
    /// 渲染前：只做内容指纹注入（视图变化由相机 viewChanged 驱动收集，不再靠指纹）。
    void onBeforePaint() override;
    /// plotArea 变化 → 交 layer 相机按 fit 模式适配宽比。
    void onPlotAreaChanged(const QRectF& newPlotArea) override;
    // 4f：鼠标交互钩子（基类 final 事件分发 → 交互开关 → 本类实现）
    void onMousePress(QMouseEvent* e) override;
    void onMouseMove(QMouseEvent* e) override;
    void onMouseRelease(QMouseEvent* e) override;
    void onWheel(QWheelEvent* e) override;
    void drawExternalContent(QPainter& painter) override;   // plotArea 外边框轴 drawAtEdge
    const QChartAbstractProjection* projection() const override { return m_projection.get(); }

private:
    /// 轴范围变化 → 算 viewRect 交给 layer（m_cameraTriggered 期间忽略：那是本类写轴引起的）
    void onAxisRangeChanged(qreal min, qreal max);
    /// 接 layer 的 viewChanged：computeDataBounds(viewRect) → 写轴（m_cameraTriggered 防回环）
    void updateAxes(const QRectF& viewRect);
    bool isAxisAttached(QChartAxis* a) const;
    /// 唯一二维层（单层：m_layer 即二维层；非二维层时返回 nullptr）。
    QChartLayer* layer2D() const;

    std::unique_ptr<QChartProjection> m_projection;  // 唯一投影（2D；容器持有，默认 Cartesian）
    // 4b：m_dataBounds 成员已删除（范围由各轴持有；dataBounds() 按需从层/轴组装）
    QList<QChartAxis*> m_axes;                        // 便捷轴列表（非持有；转发挂载到图层）

    // ===== 驱动防回环（唯一守卫）=====
    //  true 表示本类正在写轴（由相机 viewChanged 引起）→ onAxisRangeChanged 忽略本次 rangeChanged。
    bool m_cameraTriggered = false;
    bool m_panning = false;                           // 4f：左键拖动中
    QPointF m_lastPixel;                              // 4f：上次鼠标位置（widget 坐标）
};

#endif // QCHARTWIDGET_H
