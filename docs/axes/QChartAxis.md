# QChartAxis Documentation

## Brief Introduction:
轴基类（2D/3D 共用）：数值化（Data↔Numeric）+ 刻度生成 + 标签格式化 + 绘制（design_notes §Axis 三件事）。**不负责**坐标映射（Projection）与视窗变换（Widget/Camera）。五空间链中掌管 Data↔Numeric 一环。共享类型 **`DrawContext`** 定义于本头（每次 draw 调用传递的上下文：plotArea/dataBounds/viewRect/projection + toNumeric0/1 闭包 + numericToPixel/toPixelCurve/pixelVisible/rectVisible）。语法糖 setRange/min/max（仅 Cartesian，内部字段经 rangeChanged 信号由 Widget 映射到 setDataRangeDim0/Dim1）。D12 颜色双槽（setColor override 优先 / setThemeColor 主题默认 / clearColor 回退）。

## Constant Variables:

| Type | Name | Description | Available Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `constexpr qreal` | `AXIS_MARGIN` | 轴线到文字外侧的总边距。 | `8.0` | — |
| `constexpr qreal` | `TICK_LENGTH` | 主刻度线长度（px）。 | `4.0` | — |
| `constexpr qreal` | `SUB_TICK_LENGTH` | 次刻度线长度（px）。 | `2.0` | — |
| `constexpr qreal` | `TEXT_PADDING` | 文字周围呼吸空间（px，`textPadding()` 返回）。 | `3.0` | — |

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_sugarMin` | 语法糖字段 min（仅 Cartesian 有效，非映射基准；setRange 写入）。 | `qreal` | `0.0` | `QChartWidget`（rangeChanged 映射） |
| `qreal` | `m_sugarMax` | 语法糖字段 max。 | `qreal` | `0.0` | `QChartWidget` |
| `int` | `m_tickCount` | 目标主刻度数（niceStep 参考）。 | `int` | `5` | — |
| `int` | `m_subTickCount` | 每主刻度间次刻度数。 | `int` | `0` | — |
| `bool` | `m_visible` | 可见性（Q_PROPERTY visible）。 | `true`/`false` | `true` | — |
| `QString` | `m_title` | 轴标题（Q_PROPERTY title）。 | `QString` | 空 | — |
| `std::optional<QColor>` | `m_colorOverride` | 显式颜色覆盖（D12；setColor 写入）。 | `std::optional<QColor>`/`nullopt` | `nullopt` | `QChartTheme` |
| `QColor` | `m_themeColor` | 主题默认色（setThemeColor 注入）。 | `QColor` | `Qt::black` | `QChartTheme` |
| `Qt::Alignment` | `m_alignment` | 轴对齐（Q_PROPERTY alignment）。 | `Qt::Alignment` | `Qt::AlignBottom` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartAxis` | 构造函数。 | `QObject* parent=nullptr` <br> `Qt::Alignment alignment=Qt::AlignBottom` | public | — | 子类构造/用户 | — |
| `qreal` | `toNumeric` | **纯虚**：Data → Numeric（非法 data → NaN + qWarning）。 | `QVariant data` | public pure virtual | 子类实现 | DrawContext 闭包（QChartLayer drawAllSeries 组装）/QChartAxes3D/QChartLayer3D | `DrawContext` |
| `QVariant` | `fromNumeric` | **纯虚**：Numeric → Data（NaN/Inf 子类策略）。 | `qreal num` | public pure virtual | 子类实现 | 交互反向/测试 | — |
| `QVector<qreal>` | `tickValues` | **纯虚**：在 [numericMin,numericMax] 生成主刻度（Numeric 空间）。 | `qreal numericMin, qreal numericMax` | public pure virtual | 子类实现 | Widget 2D 边框轴 / `QChartAxes3D::ticks`（委托） | `QChartAxes3D` |
| `QStringList` | `tickLabels` | **纯虚**：刻度 → 标签字符串。 | `const QVector<qreal>& ticks` | public pure virtual | 子类实现 | Widget 2D / `QChartAxes3D::tickLabelTexts` | `QChartAxes3D` |
| `QVector<qreal>` | `subTickValues` | 次刻度位置（默认空；子类可覆盖）。 | `qreal numericMin, qreal numericMax` | public virtual | 空/子类实现 | 2D 绘制 | — |
| `void` | `drawAtEdge` | 边框轴模式（仅 Cartesian）：plotArea 边缘线性插值，不经 Projection。 | `QPainter* painter` <br> `const DrawContext& ctx` <br> `bool drawAxisLine, drawLabels, drawTicks` | public | — | `QPainterChartRenderer::drawBackground`（轴遍历） | `DrawContext` |
| `void` | `drawAtPosition` | 数据主脊模式（所有坐标系）：画在 offset 指定的另一维 Numeric 位置。 | `painter, ctx, qreal offset, drawAxisLine, drawLabels, drawTicks, QString& label, QPen* pen=nullptr` | public | — | `QPainterChartRenderer::drawBackground` | `DrawContext` |
| `QSizeF` | `sizeHint` | 边框轴占用空间估算（主脊返回 {0,0}）。 | `const QFont& font` | public virtual | `QSizeF` | `QChartWidget::layoutAxes` | `QChartWidget` |
| `void` | `setRange` | 语法糖：写入 m_sugarMin/Max + emit rangeChanged（仅 Cartesian 语义）。 | `qreal min, qreal max` | public | — | 用户/`QDateTimeAxis::setRange`（重载包装） | `QChartWidget` |
| `qreal` | `min`/`max` | 语法糖访问器（内联）。 | 无 | public | `qreal` | 测试 | — |
| `void` | `setVisible` | 可见性 + emit visibleChanged（变化时）。 | `bool v` | public | — | `QChartWidget::addAxis` 连线后由外部/用户 | — |
| `void` | `setTitle` | 标题（内联，无 NOTIFY）。 | `const QString& t` | public | — | 用户 | — |
| `void` | `setColor` | D12 显式覆盖（写 override）+ emit styleChanged（值变化时）。 | `const QColor& c` | public | — | 用户/demo | `QChartTheme` |
| `void` | `setThemeColor` | 主题默认注入（仅无 override 时生效）+ 条件 emit styleChanged。 | `const QColor& c` | public | — | `QChartWidget::pushTheme` | `QChartWidget` <br> `QChartTheme` |
| `void` | `clearColor` | 清除覆盖回退主题 + emit styleChanged。 | 无 | public | — | 用户 | `QChartTheme` |
| `std::optional<QColor>` | `colorOverride` | 覆盖值访问器（内联）。 | 无 | public | `std::optional<QColor>` | 主题/调色板判断 | — |
| `void` | `setTickCount` | 目标刻度数 + emit tickCountChanged（变化时）。 | `int n` | public | — | 用户 | — |
| `void` | `setSubTickCount` | 次刻度数 + emit subTickCountChanged。 | `int n` | public | — | 用户 | — |
| `bool` | `isHorizontal` | 水平方向判定（Top/Bottom/HCenter）。 | 无 | public | `true`/`false` | 布局/绘制 | — |
| `bool` | `isInteractive` | 是否允许交互（离散域轴覆盖返回 false → Widget 该维禁 pan/zoom）。 | 无 | public virtual | `true`（默认）/`false`（分类轴） | `QChartWidget::dimensionInteractive` | `QChartWidget` |

Notes:
- **DrawContext 共享类型**（本头定义，跨模块使用——architecture_overview §8）：`{plotArea, dataBounds, viewRect, projection, toNumeric0/1}` + `numericToPixel(num0,num1)`（Projection→Camera 全链）、`toPixelCurve(dataCurve, segments)`（createPath + cartesian→pixel 一步）、`pixelVisible/rectVisible`（像素可见性粗筛，margin 可调）。
- 信号槽全量连线：见 Signals 段；Called By 的 2D 绘制路径见 QPainterChartRenderer.md。

## Overrided Qt Events:
None.（QObject 非 QWidget）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `rangeChanged` | setRange 语法糖触发。 | `qreal min, qreal max` | `QChartWidget::addAxis` 连线（src/core/QChartWidget.cpp:135 → 重算 dataBounds + fit + invalidate） | `QChartWidget` |
| `visibleChanged` | 可见性变化。 | — | `QChartWidget::addAxis` 连线（→ invalidateBackground，:145） | `QChartWidget` |
| `styleChanged` | 样式（颜色等）变化。 | — | `QChartWidget::addAxis` 连线（→ invalidateBackground，:146） | `QChartWidget` |
| `tickCountChanged` | 主刻度数变化。 | — | `QChartWidget::addAxis` 连线（→ invalidateBackground，:147） | `QChartWidget` |
| `subTickCountChanged` | 次刻度数变化。 | — | 同上（连线于 addAxis） | `QChartWidget` |

Notes:
- 五个信号 Connected slots 实测均为 QChartWidget::addAxis 构造期 lambda（invalidateBackground / dataBounds 重算）。
- 3D 侧：QChartLayer3D 重绑轴时同步 axes3D 配置槽并置 worldCache 脏（非信号连线，见 QChartLayer3D.md）。
