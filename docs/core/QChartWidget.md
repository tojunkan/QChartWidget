# QChartWidget Documentation

## Brief Introduction:
QChartWidget 是 **2D 图表控件**（继承 QChartAbstractWidget；批次 A 容器化重写后形态，旧文档描述重构前架构已作废）：**纯容器**——持有唯一 2D Projection（`unique_ptr<QChartProjection>`，默认 QCartesianProjection）、唯一 plotArea 由基类布局计算、axes/layers 便捷管理；**不含相机成员**（相机归 layer，经 `layer->camera()` 访问）、无 buildScene、无图例/导出/拾取/动画（旧 API 注释保留+阶段标注，头文件即恢复点索引）。核心驱动链：`onBeforePaint` 首帧把轴语法糖范围（axisX/axisY min/max）经 `projection->computeViewRect` 数值域 → Cartesian 视图窗同步到各层相机（单次 m_viewSynced）；反向 `recomputeDataBounds`（viewRect → computeDataBounds → legacy 取向 m_dataBounds → 各层 setNumericBounds → 轴 setRange 广播）。`drawExternalContent` 在 plotArea 外画边框轴（drawAtEdge：仅 Bottom/Top/Left/Right 对齐），CPU/GPU 双后端共用。S0 期唯一实例化 2D 容器（demo_axis、冒烟/GL 取证测试）。

## Constant Variables:
None.（calculatePlotArea 内局部 minSide=40.0 保底常量非类成员）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `std::unique_ptr<QChartProjection>` | `m_projection` | （private）唯一 2D 投影（Numeric↔View；setProjection 独占替换） | Cartesian/Polar/Functional 等 | 构造默认 `QCartesianProjection` | `QChartProjection` 族 |
| `QRectF` | `m_dataBounds` | （private）当前 Numeric 范围缓存（**legacy 取向**：left/right=dim0、bottom/top=dim1 数值极值；recomputeDataBounds/onBeforePaint 写入） | `QRectF` | 空 | — |
| `QList<QChartAxis*>` | `m_axes` | （private）便捷轴列表（非持有；addAxis/removeAxis 管理；转发挂载到图层） | `QList<QChartAxis*>` | 空 | `QChartAxis` |
| `bool` | `m_viewSynced` | （private）相机视图是否已同步（axis→camera 单次链；层/轴/投影变更时复位） | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartWidget` | 构造：建默认 Cartesian 投影 | `QWidget* parent=nullptr` | public | — | demo_axis、测试、QChartWidget3D 基类 | — |
| — | `~QChartWidget` | 析构（default） | 无 | public | — | — | — |
| `void` | `addLayer` | 便捷加层（转发基类）；首层时把挂起便捷轴补挂（attachToLayer）+ m_viewSynced 复位 | `QChartLayer* layer` | public | — | 用户/demo（w->addLayer(layer)）、QChartWidget3D | `QChartLayer` |
| `void` | `removeLayer` | 移层（转发）；层空时复位 m_viewSynced | `QChartLayer* layer` | public | — | 用户 | — |
| `void` | `addAxis` | 便捷加轴：append + 立即挂到首层（isHorizontal→axisX 否则 axisY，空位才挂）+ m_viewSynced 复位 | `QChartAxis* a` | public | — | demo_axis（addAxis(x/y)）、测试 | `QChartAxis` |
| `void` | `removeAxis` | 移轴：列表移除 + 解绑所有层引用 | `QChartAxis* a` | public | — | 用户 | — |
| `QList<QChartAxis*>` | `axes` | 便捷轴列表（内联） | 无 | public | — | 测试 | — |
| `void` | `setProjection` | 替换唯一投影：m_projection=move；m_viewSynced 复位；广播 projectionChanged；布局脏+重绘 | `std::unique_ptr<QChartProjection> proj` | public | — | 用户（2D 投影切换） | — |
| `QRectF` | `dataBounds` | Numeric 范围访问器（内联，legacy 取向） | 无 | public | `QRectF` | 测试、drawExternalContent ctx | — |
| `void` | `recomputeDataBounds` | 驱动链反向：首层相机 viewRect → `computeDataBounds`（math 取向）→ legacy 取向写 m_dataBounds → 各层 `setNumericBounds` + 重绘 | 无 | public | — | setViewRect/pan/zoom 内部、用户 | — |
| `QRectF` | `viewRect` | 首层相机 viewRect（无有效层返回空） | 无 | public | `QRectF` | 测试/drawExternalContent | `QChartCamera` |
| `void` | `setViewRect` | 广播 setViewRect 到所有层相机（宽高≤0 忽略）+ m_viewSynced=true + recomputeDataBounds | `const QRectF& r` | public | — | 用户/动画阶段 | — |
| `void` | `panViewCartesian` | 广播层相机平移 + recomputeDataBounds | `qreal dx, qreal dy` | public | — | 用户/交互阶段 | — |
| `void` | `zoomViewCartesian` | 广播层相机缩放（锚 cx,cy）+ recomputeDataBounds | `qreal cx, qreal cy, qreal factorX, qreal factorY` | public | — | 用户/交互阶段 | — |
| `QPointF` | `cartesianToPixel` | View Cartesian → Pixel（首层相机 project + 本 widget plotArea；无层 (NaN,NaN)） | `qreal cx, qreal cy` | public | `QPointF` | 用户/测试 | — |
| `QPointF` | `pixelToCartesian` | Pixel → View Cartesian（unproject 原点 x/y） | `const QPointF& pixel` | public | `QPointF` | 交互阶段 | — |
| `QRectF` | `calculatePlotArea` | 覆写（protected）：margins 基值 + 各层 axisX/axisY **边框对齐**轴的 sizeHint 外边距占用（Bottom/Top 加高、Left/Right 加宽；HCenter/VCenter 数据主脊不占）；边距超限按保底 40px 等比收缩 | 无 | protected | `QRectF` | `relayout`（基类） | — |
| `void` | `layoutAxes` | 便捷：= relayout（重算 plotArea + 广播 + 摆放 GL 子控件） | 无 | protected | — | 用户/测试（显式布局） | — |
| `void` | `onBeforePaint` | 覆写：首帧 axis→camera 单次同步（首个双轴层范围 → mathRect → computeViewRect → 各层相机 setViewRect + setNumericBounds + m_dataBounds；m_viewSynced=true） | 无 | protected | — | paintEvent（基类） | — |
| `void` | `drawExternalContent` | 覆写：plotArea 外边框轴——遍历各层 axisX/axisY，仅 Bottom/Top/Left/Right 对齐者 `drawAtEdge(painter, ctx, true,true,true)`（ctx 携带 plotArea/dataBounds/viewRect/projection） | `QPainter& painter` | protected | — | paintEvent（CPU/GPU 外带） | `QChartAxis` |
| `const QChartAbstractProjection*` | `projection` | 覆写（内联）：返回 m_projection.get()（基类 pushContext 注入用） | 无 | protected | 指针 | pushContextToLayers | — |
| `void` | `attachToLayer` | （private）便捷轴挂载：isHorizontal→layer->axisX（空位）否则 axisY | `QChartLayer* layer, QChartAxis* a` | private | — | addAxis/addLayer 内部 | — |
| `bool` | `isAxisAttached` | （private）轴是否已挂到某层 | `QChartAxis* a` | private | `true`/`false` | addLayer 内部 | — |

Notes:
- 无 Q_PROPERTY/新信号（plotAreaChanged/projectionChanged 继承基类；viewChanged/seriesHovered 等注释保留待阶段）。
- 旧 API 恢复点（头注释索引，按阶段归属）：fit 配置族（`viewRectFitMode()/setViewRectFitMode()/scale()/setScale()/fitViewRectToPlotArea()` 等——相机已归层，配置 API 将作用于层相机，待相机配置阶段）；主题族（`setTheme()/theme()/pushTheme()/setBackgroundColor()/clearBackgroundColor()/backgroundColor()/setFollowSystemPalette()/followSystemPalette()/assignSeriesPaletteColor()`——待 Phase-1 主题阶段）；图例族（`legend()/setLegendVisible()/isLegendVisible()/setLegendAlignment()/legendItems()/rebuildLegendItems()`——图例阶段）；导出族（`saveAsPng/Svg/Pdf/setExportTransparentBackground()/exportTransparentBackground()`——导出阶段）；动画族（`setTemporaryProjection()/clearTemporaryProjection()`——动画阶段）；缓存开关（`isCachingEnabled()/setCachingEnabled()`——并入 renderer viewDirty）；`buildScreenScene()/buildExportScene()/buildHoverTooltip()/dimensionInteractive()/drawOverlay()` 与 `setDataRangeDim0()/setDataRangeDim1()`（数据链阶段）、`camera()/renderer()/glRenderer()` 旧 protected 访问器（相机归 layer、渲染器为容器内部细节）、事件覆写/交互状态（mouse*/wheel/pan/zoom/hover 与 seriesHovered/viewChanged 信号——交互/拾取阶段）。
- 布局取向说明：m_dataBounds 与轴语法糖为 **legacy 取向**（Y top>bottom 数值增序）；projection.computeDataBounds 输出 math 取向，转换在 recomputeDataBounds 内完成。
- S0 实测调用方：demo_axis（构造+addAxis×2+addLayer+CPU/GL 切换）；TestWidgetSmoke（addAxis/addLayer/grab 像素/plotArea 广播与边距带断言）；TestWidgetGl（setRenderBackend(OpenGL)+glHostWidget FBO 取证+dpr 尺寸断言）；TestWidget3DSmoke 经子类。

## Overrided Qt Events:
无新增覆写（基类 QChartAbstractWidget 事件已 final：paint/resize/mouse*/wheel；本类仅挂 onBeforePaint/drawExternalContent 钩子）。

## Signals:
None.（无新增；继承 `plotAreaChanged/projectionChanged`；viewChanged 恢复点见头注释）
