# QValueAxis Documentation

## Brief Introduction:
最常用的线性数值轴（2D）：`toNumeric/fromNumeric` 恒等映射（Data 恰为 qreal，语义仍独立——同链另一轴可能是 QLogAxis，design_notes §Data≠Numeric）。刻度生成：`tickValues` 用 **niceStep 漂亮步长算法**（`m_tickInterval>0` 时用固定间隔）；`tickLabels` 三态格式化管道（printf 格式 > 固定小数位 > 自动去零）。Q_PROPERTY×3（tickInterval/labelPrecision/labelFormat）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_tickInterval` | 固定刻度间隔（0 = 自动 niceStep；Q_PROPERTY tickInterval）。 | `qreal > 0` / `0` | `0.0` | — |
| `int` | `m_labelPrecision` | 标签小数位（−1 = 自动去零；Q_PROPERTY labelPrecision）。 | `int ≥ 0` / `-1` | `-1` | — |
| `QString` | `m_labelFormat` | printf 格式（空 = 不用；Q_PROPERTY labelFormat）。 | `QString`（如 "%.2f°"） | 空 | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QValueAxis` | 构造函数（QChartAxis 默认 AlignBottom）。 | `QObject* parent=nullptr` <br> `Qt::Alignment alignment=Qt::AlignBottom` | public | — | 用户/demo/测试 | — |
| `qreal` | `toNumeric` | 恒等：Data(qreal) → Numeric。 | `QVariant data` | public override | `data.toReal()` | DrawContext 闭包 | — |
| `QVariant` | `fromNumeric` | 恒等：Numeric → Data。 | `qreal num` | public override | `QVariant(num)` | 交互反向 | — |
| `QVector<qreal>` | `tickValues` | **刻度生成**：退化区间→单刻度；`m_tickInterval>0`→固定步长；否则 `niceStep(range)`（非法→range/(tickCount−1)）；对齐起始（ceil/floor 到 step 倍数）；对齐后区间空→等分退化；填充（含 0.0001·step 容差）；保障 ≥2 刻度。 | `qreal numericMin, qreal numericMax` | public override | `QVector<qreal>` | Widget 2D 边框轴 / `QChartAxes3D::ticks`（3D 委托） | `QChartAxes3D` |
| `QStringList` | `tickLabels` | **格式化管道**：`m_labelFormat` 非空 → asprintf（如 "%.2f°"）；否则 `m_labelPrecision≥0` → 固定小数位；否则自动去零（'f',8 后 chomp 尾零与小数点）。 | `const QVector<qreal>& ticks` | public override | `QStringList` | Widget 2D / `QChartAxes3D::tickLabelTexts` | `QChartAxes3D` |
| `qreal` | `niceStep` | **核心算法（私有）**：range/targetTicks → roughStep → 数量级（10 幂）→ 归一化 [1,10) → 漂亮因子候选 {1,1.5,2,3,4,5,8,10} 最近者 → bestFactor·magnitude。 | `qreal range` | private | `qreal` | `tickValues`（内部） | — |
| `void` | `setTickInterval` | 固定间隔（>0 生效；≤0 重置自动）。 | `qreal v` | public | — | 用户 | — |
| `void` | `setLabelPrecision` | 小数位（−1 自动）。 | `int p` | public | — | 用户 | — |
| `void` | `setLabelFormat` | printf 格式。 | `const QString& f` | public | — | 用户 | — |

Notes:
- **tickValues 全流程（含 niceStep 与退化分支）**：docs/axes/QChartAxis_tickValues_flow.md（★全库最常调用函数之一）。
- 无 Qt 事件覆写（QObject）；信号继承 QChartAxis（无新增）。

## Overrided Qt Events:
None.

## Signals:
None.（继承 QChartAxis 的 5 个；无新增）
