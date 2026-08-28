# QChartTheme Documentation

## Brief Introduction:
主题/调色板（Phase 1；决策 A1/A3/A5）。**纯数据 struct，无 Q_OBJECT**（非 QObject，不产 moc——t54 audit B4 修正记录）。主题 = 框架色 + 系列调色板。职责边界：**主题只当默认值**——显式设色优先（D12 override 双槽模型：axis/layer/series/legend/背景的显式 set 优先，`clear*()` 回退主题默认）。系列调色板按 addSeries 顺序循环取色（A5：`assignSeriesPaletteColor` 分配 palette[index % size]）。

## Constant Variables:
None.（`Preset{Light, Dark}` 为类型级枚举）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QColor` | `backgroundColor` | 画布底色（整控件矩形；plotArea 跟随此色，无独立填充）。 | `QColor`（Light/Dark 预设） | Light: 白系 / Dark: 深系 | `QChartWidget`（m_theme） |
| `QColor` | `gridColor` | 网格色。 | `QColor` | 预设 | `QChartLayer` |
| `QColor` | `axisColor` | 轴线 + 刻度 + 刻度标签色。 | `QColor` | 预设 | `QChartAxis` |
| `QColor` | `textColor` | 图例文字色（默认 = axisColor）。 | `QColor` | = axisColor | `QChartLegend` |
| `QVector<QColor>` | `seriesPalette` | 系列循环取色调色板（A5）。 | `QVector<QColor>` | 预设序列 | `QChartSeries` |

Notes:
- 全部字段 public（struct 语义）；无私有状态。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartTheme` | `light` | Light 预设主题（static）。 | 无 | public static | `QChartTheme` | `QChartWidget`（默认 m_theme=QChartTheme::light()）/用户 | — |
| `QChartTheme` | `dark` | Dark 预设主题（static）。 | 无 | public static | `QChartTheme` | `QChartWidget::setTheme(Dark)`/用户 | — |

Notes:
- 无构造函数自定义（聚合初始化：`QChartTheme{...}` 直接赋值即可）；无方法成员。

## Overrided Qt Events:
None.

## Signals:
None.（**非 Q_OBJECT**）
