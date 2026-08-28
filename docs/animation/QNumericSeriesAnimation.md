# QNumericSeriesAnimation Documentation

## Brief Introduction:
折线数值动画（QChartAnimation 派生）：**Numeric 空间逐点 morph**——源/目标数值点数组（`QVector<QPointF>`）按 α 线性插值，每帧整批 `setRenderOverride` 更新 QXYSeries（动画覆盖层优先于真实数据）。**Generator 模式**：`setGenerator(α, out)` 每帧产出完整点集（demo_sort 冒泡排序动画——数据驱动帧而非插值两帧）。`animate` 幂等（只依赖 α 与快照）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QXYSeries*` | `m_series` | 目标系列（非持有；setTargetSeries）。 | `QXYSeries*`/`nullptr` | `nullptr` | `QXYSeries` |
| `QVector<QPointF>` | `m_srcPoints` | 源数值点快照（setSourcePoints）。 | `QVector<QPointF>` | 空 | — |
| `QVector<QPointF>` | `m_dstPoints` | 目标数值点快照（setTargetPoints）。 | `QVector<QPointF>` | 空 | — |
| `QVector<QPointF>` | `m_tempPoints` | 临时帧点集（每帧重建，setRenderOverride 消费）。 | `QVector<QPointF>` | 空 | — |
| `Generator` | `m_gen` | 数据生成器（setGenerator；α → 完整点集）。 | `std::function<void(qreal,QVector<QPointF>&)>`/空 | 空 | — |
| `bool` | `m_useGenerator` | Generator 模式激活标记。 | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QNumericSeriesAnimation` | 构造函数。 | `QObject* parent=nullptr` | public | — | 用户/demo | — |
| `void` | `setTargetSeries` | 绑定目标系列（内联）。 | `QXYSeries* series` | public | — | 用户 | `QXYSeries` |
| `void` | `setSourcePoints` | 源点快照（内联）。 | `const QVector<QPointF>& numericPts` | public | — | 用户/动画编排 | — |
| `void` | `setTargetPoints` | 目标点快照。 | `const QVector<QPointF>& numericPts` | public | — | 用户/动画编排 | — |
| `void` | `setGenerator` | Generator 模式（α → 完整点集）。 | `Generator gen` | public | — | demo（sort） | — |
| `void` | `animate` | **Numeric 空间逐点 lerp**：`m_temp[i] = src + (dst−src)·α`（n=qMin 两端）；Generator 模式 → m_gen(α, m_temp)；每帧 `setRenderOverride(m_temp)`（覆盖层 → 绘制优先）。 | `qreal alpha` | public override | — | `updateCurrentTime`（每帧） | `QXYSeries` |

Notes:
- **插值空间**：Numeric（数值化后）——不插值 Data（QDateTime/QString 无插值语义，"Data≠Numeric"红线在动画侧的体现）。
- 长度不一致：取 qMin（截断）——调用方保证同构更优。
- 结束清理：动画 finished 后调用方 `clearRenderOverride()`（覆盖层不自动清）。

## Overrided Qt Events:
None.

## Signals:
None.（继承）
