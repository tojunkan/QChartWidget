# QBarAnimation Documentation

## Brief Introduction:
柱状动画（QChartAnimation 派生）：**Numeric 空间逐 rect morph**——源/目标柱矩形数组（`QVector<QRectF>`）按 α 线性插值，每帧整批 `setRenderOverride` 更新 QBarSeries（覆盖层优先）。**Generator 模式**：`setGenerator(α, out)` 每帧产出完整 rect 集（demo_sort 冒泡排序）。与 QNumericSeriesAnimation 同构（点 vs 矩形）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QBarSeries*` | `m_series` | 目标系列（非持有；setTargetSeries）。 | `QBarSeries*`/`nullptr` | `nullptr` | `QBarSeries` |
| `QVector<QRectF>` | `m_srcRects` | 源柱矩形快照（setSourceRects）。 | `QVector<QRectF>` | 空 | — |
| `QVector<QRectF>` | `m_dstRects` | 目标柱矩形快照（setTargetRects）。 | `QVector<QRectF>` | 空 | — |
| `Generator` | `m_gen` | 数据生成器（setGenerator；α → 完整 rect 集）。 | `std::function<void(qreal,QVector<QRectF>&)>`/空 | 空 | — |
| `bool` | `m_useGenerator` | Generator 模式激活标记。 | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QBarAnimation` | 构造函数。 | `QObject* parent=nullptr` | public | — | 用户/demo | — |
| `void` | `setTargetSeries` | 绑定目标系列（内联）。 | `QBarSeries* series` | public | — | 用户 | `QBarSeries` |
| `void` | `setSourceRects` | 源矩形快照（内联）。 | `const QVector<QRectF>& numericRects` | public | — | 用户/动画编排 | — |
| `void` | `setTargetRects` | 目标矩形快照。 | `const QVector<QRectF>& numericRects` | public | — | 用户/动画编排 | — |
| `void` | `setGenerator` | Generator 模式（α → 完整 rect 集）。 | `Generator gen` | public | — | demo（sort） | — |
| `void` | `animate` | **Numeric 空间逐 rect lerp**（src/dst 同构）或 Generator 产出；每帧 `setRenderOverride(m_tempRects)`。 | `qreal alpha` | public override | — | `updateCurrentTime`（每帧） | `QBarSeries` |

Notes:
- 与 QNumericSeriesAnimation 完全同构（点 ↔ 矩形）；插值在 Numeric 空间（rect 的 l/t/r/b 四分量独立 lerp）。
- 结束清理：动画 finished 后调用方 `clearRenderOverride()`。

## Overrided Qt Events:
None.

## Signals:
None.（继承）
