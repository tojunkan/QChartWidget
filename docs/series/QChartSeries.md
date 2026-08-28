# QChartSeries Documentation

## Brief Introduction:
系列基类（2D/3D 共用，D15 红线起点）：**Series 只存 Data 空间数据（QVariant），不参与坐标变换**——所有坐标变换由 Layer 注入的 `toPixel` 闭包（2D）/`ProjectFn3D` 闭包（3D）完成，系列零耦合（不持 Axis/Projection/Widget/相机）。Q_PROPERTY×4（name/visible/opacity/color 均带 NOTIFY）；D12 颜色双槽（setColor override 优先 / setThemeColor 主题默认 / clearColor 回退）。`count()` 纯虚；`draw`（2D 签名）纯虚；`hitTest` 默认实现（像素 in 多边形，供 2D 系列覆写）。3D 系列见 QChartSeries3D（本类 draw 的 2D 签名在 3D 系列中被"告警 no-op 桩"覆盖防误用）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QString` | `m_name` | 系列名（Q_PROPERTY name；图例条目 key）。 | `QString` | 构造传入（默认空） | `QChartLegend` |
| `bool` | `m_visible` | 可见性（Q_PROPERTY visible）。 | `true`/`false` | `true` | — |
| `qreal` | `m_opacity` | 不透明度（Q_PROPERTY opacity）。 | `qreal` [0,1] | `1.0` | — |
| `std::optional<QColor>` | `m_colorOverride` | 显式颜色覆盖（D12；setColor 写入）。 | `std::optional<QColor>`/`nullopt` | `nullopt` | `QChartTheme` |
| `QColor` | `m_themeColor` | 主题注入默认色（setThemeColor）。 | `QColor` | 构造初始化 | `QChartTheme` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartSeries` | 构造函数。 | `const QString& name={}` <br> `QObject* parent=nullptr` | public | — | 子类构造/用户 | — |
| `int` | `count` | **纯虚**：数据点数量。 | 无 | public pure virtual | 子类实现 | 渲染/测试 | — |
| `void` | `draw` | **纯虚（2D 签名）**：toPixel 闭包绘制（NaN 点跳过；ctx 提供 toNumeric/toPixelCurve 供通用坐标系曲线边；nullptr → 像素直线快路径）。 | `QPainter* painter` <br> `std::function<QPointF(QVariant,QVariant)> toPixel` <br> `const DrawContext* ctx=nullptr` | public pure virtual | 子类实现 | `QChartLayer::drawAllSeries` | `DrawContext` <br> `QChartLayer` |
| `int` | `hitTest` | 命中检测（默认实现：像素 in 多边形；2D 系列覆写）。 | `const QPointF& pixel` <br> `toPixel` <br> `const DrawContext* ctx=nullptr` | public virtual | `int`（index，-1 未命中） | `QChartHitTester::hitTest`（2D） | `QChartHitTester` |
| `void` | `setName` | 设置名称 + emit nameChanged。 | `const QString& n` | public | — | 用户 | — |
| `void` | `setVisible` | 设置可见性 + emit visibleChanged。 | `bool v` | public | — | 用户/图例点击（B4） | `QChartLegend` |
| `void` | `setOpacity` | 设置不透明度 + emit opacityChanged。 | `qreal o` | public | — | 用户/动画 | — |
| `void` | `setColor` | D12 显式覆盖 + emit colorChanged（值变化时）。 | `const QColor& c` | public | — | 用户/demo | `QChartTheme` |
| `void` | `setThemeColor` | 主题默认注入（仅无 override 时生效）+ 条件 emit colorChanged。 | `const QColor& c` | public | — | `QChartWidget::pushTheme`/`assignSeriesPaletteColor` | `QChartWidget` <br> `QChartTheme` |
| `void` | `clearColor` | 清除覆盖回退主题 + emit colorChanged。 | 无 | public | — | 用户 | `QChartTheme` |
| `std::optional<QColor>` | `colorOverride` | 覆盖值访问器（内联）。 | 无 | public | `std::optional<QColor>` | 主题/调色板判断 | — |

Notes:
- **零耦合红线**：本类无任何 Axis/Projection/Widget 引用——3D 延续（QChartSeries3D 仅多缓存字段，仍无映射对象引用，reviewer grep 验证点）。
- 所有权：系列由 Layer 持有（QChartLayer::addSeries → m_series，qDeleteAll）；Widget 经 layer 接线系列属性信号。

## Overrided Qt Events:
None.（QObject）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `nameChanged` | 名称变化。 | `const QString&` | `QChartWidget`（addLayer seriesAdded 时连线 → invalidateForeground + 图例重建） | `QChartWidget` |
| `visibleChanged` | 可见性变化。 | — | `QChartWidget`（连线 → invalidateForeground） | `QChartWidget` |
| `opacityChanged` | 不透明度变化。 | — | `QChartWidget`（连线 → invalidateForeground） | `QChartWidget` |
| `colorChanged` | 颜色变化（override/主题/清除）。 | — | `QChartWidget`（连线 → invalidateForeground） | `QChartWidget` |

Notes:
- Connected slots 实测：QChartWidget::addLayer 的 seriesAdded 槽内 connect（src/core/QChartWidget.cpp:84-87）——系列属性变化统一触发前景重绘。
- 3D 系列 dataChanged 另连 QChartLayer3D（worldCache 置脏），见 QChartSeries3D.md。
