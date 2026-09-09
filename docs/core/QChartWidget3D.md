# QChartWidget3D Documentation

## Brief Introduction:
QChartWidget3D 是 **3D 图表控件**（继承 QChartWidget；批次 B2 轻量 3D 容器重写后形态）：**单 QChartLayer3D 托管容器**——构造即建默认 layer3D（QObject 子）并接线基类 addLayer；默认三轴为 2D `QValueAxis` 实例（this 持有，0..10、tick5、黑，可经 setAxisX3D/Y3D/Z3D 替换，生命周期调用方约定同基类 addAxis）。**本类不持有相机/投影成员**：`camera3D()/projection3D()/viewCube()/worldToPixel()` 全转发 layer3D（相机=layer3D 的 m_camera3D 值成员，模式同 2D 相机归层）；域盒/网格由 layer3D 承载。3D 特定 API 隔离本类，基类只做 2D 容器行为；渲染/布局/plotArea/广播/GL 宿主复用基类，覆写 renderLayers/renderLayersGL/pushContextToLayers/onBeforePaint(空)/drawExternalContent(空) 使场景源=layer3D.scene3D + camera3D。旧实体（自持相机/投影、GlHost、buildScreenScene、轨道拖拽、hover 信号、series3D 包围盒等）注释保留+恢复点标注（头文件即索引）。S0 期由 demo_axis3d 与 widget3d 冒烟/GL 测试实例化。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartLayer3D*` | `m_layer3D` | （private）托管默认 layer3D（QObject 子、构造即建并经基类 addLayer 接线；非持有于 m_layers 之外另存指针） | 指针 | 构造创建 | `QChartLayer3D` |
| `QValueAxis*` | `m_axisX3D/Y3D/Z3D` | （private）默认三轴（owned by this；可替换） | 指针 | 构造创建（0..10/tick5/黑） | `QValueAxis` |
| `std::optional<QVector3D>` | `m_domainMin/m_domainMax` | （private）域盒请求缓存（setDomainBox 写入；hasDomainBox 判有） | 值/`std::nullopt` | `std::nullopt` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartWidget3D` | 构造：建默认 layer3D + addLayer 接线 + 三默认轴绑定到 layer3D | `QWidget* parent=nullptr` | public | — | demo_axis3d、TestWidget3DSmoke、TestWidget3DGl | — |
| — | `~QChartWidget3D` | 析构（default） | 无 | public | — | — | — |
| `QChartLayer3D*` | `layer3D` | 3D 图层访问器（内联；非 const/const） | 无 | public | 指针 | demo_axis3d（setGridMode）、测试 | — |
| `void` | `addLayer3D` | 兼容旧调用名：经基类 addLayer 接线（内部） | `QChartLayer3D* g` | public | — | 用户（多 3D 层待扩展阶段） | — |
| `void` | `setAxisX3D/Y3D/Z3D` | 透传 layer3D::setAxisX/Y/Z（同步 axes3D 配置） | `QChartAxis* a` | public | — | 用户 | `QChartAxis` |
| `QChartAxis*` | `axisX3D/Y3D/Z3D` | 三轴访问器（透传 layer3D） | 无 | public | 指针 | 测试 | — |
| `void` | `setDomainBox` | 设域盒：存缓存 + layer3D.setDataBounds + fitWorld | `const QVector3D& dataMin, const QVector3D& dataMax` | public | — | demo_axis3d（(-3,-3,-3)-(3,3,3)）、测试 | — |
| `void` | `clearDomainBox` | 清域盒：回退默认盒 (0,0,0)-(10,10,10) + fitWorld | 无 | public | — | 用户 | — |
| `bool` | `hasDomainBox` | 域盒缓存是否有值（内联） | 无 | public | `true`/`false` | 测试 | — |
| `void` | `setProjection3D` | 设 3D 投影（转发 layer3D 内部采样提示；scene3D.projection 由 pushContext 注入）+ fitWorld | `const QChartProjection3D* proj` | public | — | demo_axis3d（静态 QCartesianProjection3D）、测试 | `QChartProjection3D` |
| `const QChartProjection3D*` | `projection3D` | 投影访问器（转发 layer3D） | 无 | public | 指针 | 测试 | — |
| `QChartCamera3D*` | `camera3D` | 相机访问器（转发 layer3D 值成员；非 const/const） | 无 | public | 指针/`nullptr` | demo_axis3d（setYaw/Pitch）、测试（camera3D()==layer3D()->camera3D() 断言） | `QChartCamera3D` |
| `void` | `fitWorld` | 相机 fit：域盒（无则 axes3D 数据盒，再退默认 0..10）外扩 6% → setViewCube；按 fov 重算 distance（halfDiag/sin(fov/2)）+ resetNearFar；姿态复位 yaw45/pitch30 | 无 | public | — | setDomainBox/clearDomainBox/setProjection3D 内部、用户 | `QCube` |
| `QPointF` | `worldToPixel` | 世界坐标 → 像素（camera3D->project(w, m_plotArea).screen；无层 (NaN,NaN)） | `const QVector3D& w` | public | `QPointF` | 测试 | — |
| `QCube` | `viewCube` | 相机当前 viewCube（转发） | 无 | public | `QCube` | 测试 | — |
| `void` | `setViewCube` | 转发相机 setViewCube + resetNearFar | `const QCube& box` | public | — | 用户 | — |
| `void` | `pushContextToLayers` | 覆写：scene3D 注入 projection3D/plotArea/背景 | 无 | protected | — | renderLayers/renderLayersGL | — |
| `void` | `renderLayers` | 覆写（CPU）：push → layer3D.collectPrimitives → **scene3D 拷贝**（camera 指针仍指 layer 成员）→ cpuRenderer render | `QPaintDevice* device` | protected | — | paintEvent（基类 CPU 路径） | `QPainterChartRenderer` |
| `void` | `renderLayersGL` | 覆写（GL）：push → 设备分辨率透明 labelDev QImage → collect → scene3D 拷贝 → **全部图元 depth=2.0f**（decor 批次：depthTest 关 = 全边可见，B1/B2 线框冒烟语义；3D series 深度后续批次接管）→ glRenderer render → SourceOver 合成 | `QPaintDevice* device` | protected | — | GlPlotWidget::paintGL | `QOpenGLChartRenderer` |
| `void` | `onBeforePaint` | 覆写：空（3D 不走 2D axis→camera 同步链） | 无 | protected | — | paintEvent | — |
| `void` | `drawExternalContent` | 覆写：空（3D 无边框轴/标题，plotArea 外保持背景） | `QPainter& painter` | protected | — | paintEvent | — |

Notes:
- 形态核对实测：camera3D()==layer3D()->camera3D()（B2 审查断言）；无相机/投影成员（头注释+测试断言）。
- GL 通路说明：3D 场景的 scene3D.camera = layer3D 的 m_camera3D（构造注入）；GL 批次 u_viewProj 来自 camera3D->viewProjectionMatrix（aspect=plotArea），变换注入由投影族 glslToCartesian 提供；glHostWidget() 由基类提供（plotArea 对齐）。
- 旧实体恢复点（头注释）：`m_camera3D`/`m_projection3D` 持有（批次 B2 移除）、`m_worldBounds`（QChartWorldBox 旧名/旧类型，随 3D 系列阶段删除）、旧 GlHost/RenderBackend（基类已有）、`buildScreenScene()/buildExportScene()` 覆写（场景组装由 layer3D.collect 取代）、`invalidateBackground()/invalidateForeground()` 覆写（语义已被 renderer viewDirty/layer dataDirty 取代）、orbit/dolly 手势状态与 m_anchorBox（交互阶段）、uvHovered/Selected 信号与 updateHover（拾取阶段）、series3D 数据包围盒/反算（3D 系列阶段）、setCamera3D 旧签名、layers3D()/m_layers3D 列表访问器（现单 layer3D 托管）。
- S0 实测调用方：demo_axis3d、TestWidget3DSmoke（CPU grab 像素断言）、TestWidget3DGl（GL 宿主 FBO 取证）。

## Overrided Qt Events:
无（基类事件 final；本类仅覆写虚钩子 pushContextToLayers/renderLayers/renderLayersGL/onBeforePaint/drawExternalContent）。

## Signals:
None.（uvHovered/uvSelected/uvHoveredEnd 等旧信号待拾取阶段恢复；继承 plotAreaChanged/projectionChanged）
