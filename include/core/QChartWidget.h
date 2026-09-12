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

    // ===== 图层管理（便捷，转发基类容器；4a：签名改抽象层类型）=====
    void addLayer(QChartAbstractLayer* layer);
    void removeLayer(QChartAbstractLayer* layer);

    // ===== 轴管理（便捷：加到首个图层；图层缺失时先挂起，addLayer 时补挂）=====
    void addAxis(QChartAxis* a);
    void removeAxis(QChartAxis* a);
    QList<QChartAxis*> axes() const { return m_axes; }

    // ===== Projection（容器唯一持有；访问器见 protected 基类覆写投影()）=====
    void setProjection(std::unique_ptr<QChartProjection> proj);

    // ===== 数据范围 / 视窗驱动链（广播到各 layer 相机与轴）=====
    /// 4b：dataBounds 不再被长期持有——本方法**按需从层/轴临时组装**（legacy 取向：
    /// left/right=dim0 轴范围、bottom/top=dim1 轴范围；无二维层/轴时返回空 QRectF）。
    QRectF dataBounds() const;
    void recomputeDataBounds();                 // 相机窗口 → 临时 dataBounds → 写回各轴范围（4b）
    QRectF viewRect() const;                    // 首层相机 viewRect（无层返回空）
    void setViewRect(const QRectF& r);          // 广播 setViewRect 到所有层相机 + recomputeDataBounds
    void panViewCartesian(qreal dx, qreal dy);
    void zoomViewCartesian(qreal cx, qreal cy, qreal factorX, qreal factorY);

    // ===== 4e：驱动链诊断（单测防回环/幂等计数；外部集成亦可观测）=====
    /// 数值侧 fit 次数（轴范围 → computeViewRect → 相机窗口）
    int fitCount() const { return m_fitCount; }
    /// 相机侧反算次数（相机窗口 → 临时 dataBounds → 写回各轴范围）
    int backCalcCount() const { return m_backCalcCount; }
    bool isNumericDirty() const { return m_numericDirty; }   // 数值侧待 fit
    bool isCameraDirty() const { return m_cameraDirty; }     // 相机侧待反算
    void resetDriveCounters() { m_fitCount = 0; m_backCalcCount = 0; }

    // ===== 4f：鼠标交互（只做“事件 → 既有相机/视窗 API”的接线；开关见基类 setInteractionEnabled）=====
    /// 左键拖动 = 平移视图：像素位移 → 视图坐标位移（pixelDeltaToViewDelta）→ 既有 panViewCartesian
    ///   （走 4e 相机侧反算链：fit 0 次、反算 +1）；
    /// 滚轮 = 以光标为中心缩放：pixelToCartesian 求中心 → 既有 zoomViewCartesian（中心处数据坐标不变）。
    /// 交互开关默认开；关闭后鼠标事件不产生任何视图变化。
    QPointF pixelDeltaToViewDelta(const QPointF& pixelDelta) const;   // 纯计算（单测直接验比例一致性）
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
    /// 4e：渲染前驱动链收口（方向状态驱动，取代 4c 的一次性闩锁 m_viewSynced）：
    ///   ①相机侧脏 → 反算写轴（绝不 fit）；②数值侧脏 → fit + 以实际可见范围回写轴。
    /// 两方向互不触发（写轴期间抑制 rangeChanged 标记）→ 无回环、幂等。
    void onBeforePaint() override;
    /// 4e：像素侧驱动（2D）——plotArea 变化 → 交相机 fit 模式（Stretch/Preserve/Expand/Crop）适配宽比；
    /// 窗口被改动则转相机侧反算（绝不 fit）。Stretch（拉伸铺满，Cartesian 常态）不触碰窗口 → 零视觉变化。
    void onPlotAreaChanged(const QRectF& newPlotArea) override;
    // 4f：鼠标交互钩子（基类 final 事件分发 → 交互开关 → 本类实现）
    void onMousePress(QMouseEvent* e) override;
    void onMouseMove(QMouseEvent* e) override;
    void onMouseRelease(QMouseEvent* e) override;
    void onWheel(QWheelEvent* e) override;
    void drawExternalContent(QPainter& painter) override;   // plotArea 外边框轴 drawAtEdge
    const QChartAbstractProjection* projection() const override { return m_projection.get(); }

private:
    void attachToLayer(QChartLayer* layer, QChartAxis* a);  // 便捷轴挂载（isHorizontal→axisX，否则→axisY）
    void onAxisRangeChanged(qreal min, qreal max);  // 4e：轴范围变化 → 数值侧待 fit（本类写轴期间抑制）
    void markNumericDirty();                        // 4e：置数值侧脏 + 请求重绘
    bool applyNumericFit();                         // 4e：数值侧 fit（轴范围 → 相机窗口 → 实际可见范围回写轴）
    bool backCalcAxesFromCameraWindow();            // 4e：相机窗口 → 临时 dataBounds → 写回各轴范围（计数）
    void writeAxisRanges(const QRectF& legacy);     // 4e：写轴（带抑制深度，防 rangeChanged 回环）
    bool isAxisAttached(QChartAxis* a) const;
    /// 4a：二维层视图（抽象层列表 → 二维层）——viewRect/dataBounds/相机/轴绑定等二维专属操作只作用于
    /// 二维层；三维层（QChartLayer3D）的渲染/上下文本就由 QChartWidget3D 覆写，不经这些路径。
    QList<QChartLayer*> layers2D() const;

    std::unique_ptr<QChartProjection> m_projection;  // 唯一投影（2D；容器持有，默认 Cartesian）
    // 4b：m_dataBounds 成员已删除（范围由各轴持有；dataBounds() 按需从层/轴组装）
    QList<QChartAxis*> m_axes;                        // 便捷轴列表（非持有；转发挂载到图层）

    // ===== 4e：显式驱动方向状态（取代一次性闩锁 m_viewSynced）=====
    //  数值侧（轴 setRange / 图层与轴绑定 / 投影变化）→ m_numericDirty：渲染前 fit 一次
    //    （轴范围 → computeViewRect → 相机窗口 → 以实际可见范围回写轴）；
    //  相机侧（setViewRect/pan/zoom/相机窗口变化）→ m_cameraDirty：渲染前反算写轴，**绝不 fit**；
    //  m_axisWriteDepth>0 表示本类正在写轴（反算/fit 回写）→ 抑制 rangeChanged→数值侧脏，杜绝回环。
    bool m_numericDirty = true;                       // 数值侧待 fit（初值：等待首帧前首次同步）
    bool m_cameraDirty  = false;                      // 相机侧待反算
    int  m_axisWriteDepth = 0;                        // 本类写轴抑制深度（防回环）
    bool m_panning = false;                           // 4f：左键拖动中
    QPointF m_lastPixel;                              // 4f：上次鼠标位置（widget 坐标）
    int  m_fitCount = 0;                              // 诊断：数值侧 fit 次数
    int  m_backCalcCount = 0;                         // 诊断：相机侧反算次数
};

#endif // QCHARTWIDGET_H
