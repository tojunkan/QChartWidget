# QChartLegend Documentation

## Brief Introduction:
图例（Phase 1 overlay）：独立 QObject（**非 QWidget**），由 renderer 在 plotArea 内 overlay 绘制（QPainterChartRenderer 前景层 / GL 后端 GlHost QPainter overlay），交互由 widget 转发（QChartWidget::mousePressEvent 图例点击切换系列可见性，B4）。单列竖向：色块 + 名字；隐藏系列降透明度。文字色采用 override 双槽（design_theme §2）：主题推 textColor 作默认，显式设色优先。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `bool` | `m_visible` | 图例可见性（Q_PROPERTY visible；NOTIFY visibleChanged）。 | `true` <br> `false` | `true` | `QChartWidget`（setLegendVisible 转发） |
| `Qt::Alignment` | `m_alignment` | 图例对齐（四角：AlignLeft\|AlignTop 默认 / AlignRight\|AlignTop / AlignLeft\|AlignBottom / AlignRight\|AlignBottom；Q_PROPERTY alignment）。 | `Qt::Alignment` | `AlignLeft \| AlignTop` | `QChartWidget` |
| `std::optional<QColor>` | `m_textColorOverride` | 文字色显式覆盖（D12；`setTextColor` 赋值、`clearTextColor` 清空）。 | `std::optional<QColor>` <br> `std::nullopt` | `std::nullopt` | `QChartTheme` |
| `QColor` | `m_themeTextColor` | 主题默认文字色（`setThemeTextColor` 推送）。 | `QColor` | `Qt::black` | `QChartTheme` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartLegend` | 构造函数（parent 透传）。 | `QObject* parent` | public | — | `QChartWidget` 构造（new QChartLegend(this)） | `QChartWidget` |
| `bool` | `isVisible` | 可见性访问器（内联）。 | 无 | public | `true`/`false` | `QChartWidget::isLegendVisible`/测试 | — |
| `void` | `setVisible` | 设置可见性 + emit visibleChanged（变化时）。 | `bool v` | public | — | `QChartWidget::setLegendVisible` | `QChartWidget` |
| `Qt::Alignment` | `alignment` | 对齐访问器（内联）。 | 无 | public | — | `QChartWidget::setLegendAlignment`（读） | — |
| `void` | `setAlignment` | 设置对齐 + emit alignmentChanged。 | `Qt::Alignment a` | public | — | `QChartWidget::setLegendAlignment` | `QChartWidget` |
| `void` | `setTextColor` | 文字色显式覆盖（D12 override 优先）+ emit textColorChanged。 | `const QColor& c` | public | — | 用户/demo | `QChartTheme` |
| `void` | `setThemeTextColor` | 主题默认文字色推送（仅当无 override 时生效）+ emit（视情况）。 | `const QColor& c` | public | — | `QChartWidget::pushTheme` | `QChartWidget` <br> `QChartTheme` |
| `void` | `clearTextColor` | 清除覆盖（回退主题默认）+ emit textColorChanged。 | 无 | public | — | 用户 | `QChartTheme` |
| `QColor` | `textColor` | 有效文字色（内联 = override.value_or(theme)）。 | 无 | public | — | 渲染（draw） | `QChartTheme` |
| `std::optional<QColor>` | `textColorOverride` | 覆盖值访问器（内联）。 | 无 | public | `std::optional<QColor>` | 测试 | — |
| `void` | `draw` | 绘制（单列竖向：色块 + 名字；隐藏系列降透明度；用 textColor）。 | `QPainter* p` <br> `const QRectF& plotArea` <br> `const QList<QChartSeries*>& items` | public | — | `QPainterChartRenderer::drawForeground`（图例段） | `QChartSeries` <br> `QChartScene` |
| `QChartSeries*` | `seriesAt` | 命中：返回命中的系列（未命中 nullptr）。 | `const QPointF& pos` <br> `const QRectF& plotArea` <br> `const QList<QChartSeries*>& items` | public | `QChartSeries*`/`nullptr` | `QChartWidget::mousePressEvent`（图例点击 B4） | `QChartSeries` |
| `QRectF` | `boundingRect` | 图例边界（供 widget 判点击、将来外置布局预留）。 | `plotArea, items` | public | `QRectF` | 测试/交互 | — |
| `QRectF` | `itemRect` | 第 index 个图例项可点击行矩形（测试/交互用；越界返回空）。 | `int index` <br> `plotArea, items` | public | `QRectF` | 测试（点击命中验证） | — |

Notes:
- 图例条目（m_legendItems）由 QChartWidget 组装（rebuildLegendItems：汇总所有 layer、跳过空 name、按 add 顺序），本类只消费 items 参数，不持有系列。
- 渲染归属：QPainter 后端前景层（QPainterChartRenderer::drawForeground 图例段）/ GL 后端 overlay（GlHost::paintGL QPainter 合成层，design_phase3 §6）。

## Overrided Qt Events:
None.（非 QWidget）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `visibleChanged` | 可见性变化。 | — | `QChartWidget`（构造连线 → invalidateForeground，src/core/QChartWidget.cpp:41） | `QChartWidget` |
| `alignmentChanged` | 对齐变化。 | — | `QChartWidget`（构造连线 → invalidateForeground，:42） | `QChartWidget` |
| `textColorChanged` | 文字色变化（override 或清除）。 | — | `QChartWidget`（构造连线 → invalidateForeground，:43） | `QChartWidget` |

Notes:
- 三信号 Connected slots 实测均为 QChartWidget 构造期 lambda（invalidateForeground）——图例变化触发前景重绘。
