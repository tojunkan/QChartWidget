# module_layers.md —— layers 模块

> 属于 t53 文档套件；配套 deepdive：`docs/layers/deepdive_layerDepth.md`（分层收集与深度偏置）。
> 2D/3D 同属本模块（对仗代码目录 `include/layers/{2d,3d}`）；基类 QChartLayer 在 `layers/` 根。

## 1. 职责与边界

layers = **图层家族**。核心分工（渲染层，overview §4 三层分离的第三层）：

- **QChartLayer（2D 基类）**：持有系列（所有权 + qDeleteAll）、组装 `toPixel` 闭包、`drawGrid`/`drawAllSeries` 编排；网格可见性/颜色（D12 双槽）。
- **QChartLayer3D**：3D 侧**唯一投影点**（toWorld/投影）；持有 QChartAxes3D 编排器（拥有）；`collectPrimitives` 图元收集 + worldCache 直算；组装 `ProjectFn3D` 全链闭包。

- 依赖：`axes`（QChartAxis/DrawContext）、`series`（系列类型）、`core`（QChartCamera3D/QChartRenderer 的类型：QChartPrimitive）。
- 被依赖：`core`（QChartWidget include QChartLayer；QChartWidget3D 持有 Layer3D）、GL 渲染器（buildBatches 消费 collectPrimitives）。
- 与 core 的头级小环（`QChartWidget.h` ↔ `QChartLayer.h`）见 overview §6 说明。

## 2. 文件与类清单

| 文件 | 类 | Q_OBJECT | 信号 | Q_PROPERTY |
|---|---|---|---|---|
| include/layers/QChartLayer.h + src/layers/QChartLayer.cpp | `QChartLayer`（2D 基类） | ✓ | seriesAdded / seriesRemoved / gridChanged | 2（gridVisible/gridColor） |
| include/layers/3d/QChartLayer3D.h + src/layers/3d/QChartLayer3D.cpp | `QChartLayer3D : QChartLayer` | ✓ | —（继承） | — |

## 3. 公共 API 一览

**QChartLayer（2D）**
- 轴：`setAxisX/setAxisY`、`validateAxes()`；坐标系：`setCoordinateSystem(CoordinateSystem)`。
- 系列：`addSeries/removeSeries/clearSeries/seriesList()`（所有权：析构 qDeleteAll）；信号 seriesAdded/seriesRemoved。
- 绘制：`drawAllSeries(p, ctx)`（遍历组装 toPixel → series->draw）、`drawGrid(p, ctx)`。
- 命中：`hitTest(pixel, ctx)` → HitResult（D27：实现已统一迁入 QChartHitTester，本方法保留兼容入口）。
- 网格：`gridVisible/gridColor`（override 双槽：setGridColor/setThemeGridColor/clearGridColor）。

**QChartLayer3D**
- 轴：`setAxisX/Y/Z`（Z 仅 toNumeric；重绑自动同步 axes3D 配置槽 dim0/1/2）；`axisZ()`。
- 3D 系列：`addSeries3D/removeSeries3D/series3DList()`（存入基类 m_series + 类型化登记；dataChanged → hookSeriesDirty 置脏 worldCache）。
- 投影：`setProjection3D(proj)`（Widget3D 注入，非持有；变化 → worldCache 置脏）。
- 编排器：`axes3D()`（拥有 QChartAxes3D）。
- 网格：`setGridMode(Box/Lattice)`；`setAxesDataBox(dataMin, dataMax)`（Numeric；默认 (0,0,0)=(0,0,0) 无效 → 不生成轴/网格图元）。
- 图元收集：`collectPrimitives(cam, plotArea, out, labels=null)`（轴/网格 Grid+ForegroundDecor 分层 + 系列图元 + worldCache 直算 + billboard 标签可选出参）。
- 闭包：`makeProjectFn(cam, plotArea)`（public：Widget3D 悬停复用同一闭包做屏幕近邻）。

## 4. 信号槽表（谁连谁）

| 发送方 | 信号 | 接收方 | 动作 |
|---|---|---|---|
| QChartLayer | seriesAdded(s) / seriesRemoved(s) | QChartWidget | 系列入列/出列 + 属性信号连线 → invalidateForeground |
| QChartLayer | gridChanged | QChartWidget | invalidateBackground |
| QChartSeries3D | dataChanged | QChartLayer3D（hookSeriesDirty） | worldCache 置脏 → collectPrimitives 重建 |
| QChartCamera3D | viewChanged | QChartWidget3D | 反算 + 推轴盒 + 重绘（collect 经 Layer3D） |

## 5. 核心机制

1. **系列所有权在层**（2D/3D 同构）：基类 m_series 拥有系列（qDeleteAll）；3D 额外 m_series3D 类型化遍历（复用图例/主题/调色板/所有权）。
2. **3D 侧唯一投影点**（三层分离）：Layer3D 做 toWorld/投影（makeProjectFn/projectNumeric/emitLine）；QChartAxes3D 只产 Numeric 几何；Renderer 只画。
3. **worldCache 直算**：collectPrimitives 内填充数值型系列 worldCache（= toWorld(numericCache)）与曲面 worldCache（= toWorld(toNumeric(grid))）；置脏源 = 投影/轴/数据变化。
4. **快速通道**（§5.4/D23）：`isIdentityMapping()` → emitLine 免 toWorld（Numeric≡World 直通）、段数 = samplingSegmentsHint()（Cartesian3D=2）。
5. **深度分层**：Grid/Series 统一深度排序 + kGridDepthBias（=1e-3，core）；ForegroundDecor 恒后画；见 deepdive_layerDepth。

## 6. 核心函数一览表（含文件位置）

| 函数 | 职责 | 调用方 | 文件 |
|---|---|---|---|
| `QChartLayer::drawAllSeries/drawGrid` | 2D 绘制编排（toPixel 闭包组装） | QPainterChartRenderer | src/layers/QChartLayer.cpp |
| `QChartLayer::addSeries/removeSeries` | 系列所有权管理 + 信号 | QChartWidget | src/layers/QChartLayer.cpp |
| `QChartLayer3D::makeProjectFn` | 组装 ProjectFn3D 全链闭包 | collectPrimitives / Widget3D 悬停 | src/layers/3d/QChartLayer3D.cpp |
| `QChartLayer3D::collectPrimitives` | 轴/网格/系列图元收集 + worldCache 直算 | QPainterChartRenderer 3D 子路径 / GL buildBatches | src/layers/3d/QChartLayer3D.cpp |
| `QChartLayer3D::emitLine` | 直线采样图元（identity 免 toWorld；段中点深度） | collectPrimitives（网格/系列） | src/layers/3d/QChartLayer3D.cpp |
| `QChartLayer3D::hookSeriesDirty/unhookSeriesDirty` | series dataChanged → worldCache 置脏 | add/removeSeries3D | src/layers/3d/QChartLayer3D.cpp |
| `QChartLayer3D::dimTicks` | 该维刻度值（axes3D 委托） | collectPrimitives（刻度图元） | src/layers/3d/QChartLayer3D.cpp |

## 7. 设计文档对应

- 2D 图层/网格/系列绘制：`docs/design/design_notes.md`（§Grid 绘制、§Series 绘制、§DrawContext）。
- 3D 图层/图元/深度：`docs/design/design_3d.md`（§7 图层与渲染 3D 路径、§7.3 图元列表）。
- 3D 轴编排/网格模式：`docs/design/design_3d_axes.md`（§5.1 网格、§8.3 Layer3D 扩展、§8.5 地板网格并入）。
- 决策：D15（闭包）、D16（深度降序）、D24（分层编排）、D29（两后端深度等价）。
