# QDateTimeAxis Documentation

## Brief Introduction:
日期时间轴（2D）：`toNumeric = toMSecsSinceEpoch()`、`fromNumeric = fromMSecsSinceEpoch(v)`（Numeric 空间 = epoch 毫秒）。`tickValues` 自动选择合适时间单位（年/月/天/时/分/秒，`chooseStep` 按 rangeMs 阶梯）；`tickLabels` 按 `format`（QDateTime::toString 格式）格式化。`setRange(QDateTime,QDateTime)` 重载包装（内部 toNumeric 后走基类 setRange → rangeChanged）。Q_PROPERTY×1（format）。

## Constant Variables:
None.（`chooseStep` 阶梯表为 .cpp 内局部）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QString` | `m_format` | 标签格式（QDateTime::toString；Q_PROPERTY format）。 | `QString`（如 "yyyy-MM-dd"） | 构造传入（默认 ISO） | — |

Notes:
- `TimeStepInfo{stepMs}` 由 `chooseStep(rangeMs)` 生成（年→月→日→时→分→秒阶梯；月非固定毫秒数——按 30.44 天近似或分档，实现见 .cpp）。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QDateTimeAxis` | 构造函数。 | `QObject* parent=nullptr` <br> `Qt::Alignment alignment=Qt::AlignBottom` | public | — | 用户/demo/测试 | — |
| `qreal` | `toNumeric` | `data.toDateTime().toMSecsSinceEpoch()`。 | `QVariant data` | public override | `qreal` | DrawContext 闭包 | — |
| `QVariant` | `fromNumeric` | `QDateTime::fromMSecsSinceEpoch(num)`。 | `qreal num` | public override | `QVariant` | 交互反向 | — |
| `QVector<qreal>` | `tickValues` | **单位自适应刻度**：`chooseStep(rangeMs)` 选 stepMs；对齐起始（epoch 0 起 stepMs 整数倍，ceil 修正）；逐 stepMs 填充；保障 ≥2 刻度。 | `qreal numericMin, qreal numericMax` | public override | `QVector<qreal>` | Widget 2D / `QChartAxes3D::ticks` | `QChartAxes3D` |
| `QStringList` | `tickLabels` | 按 m_format 对 fromMSecsSinceEpoch(t) 格式化。 | `const QVector<qreal>& ticks` | public override | `QStringList` | Widget 2D / `QChartAxes3D::tickLabelTexts` | `QChartAxes3D` |
| `void` | `setRange` | 重载：QDateTime 版（内部 toNumeric 后调基类 setRange → rangeChanged）。 | `const QDateTime& min, const QDateTime& max` | public | — | 用户 | `QChartAxis` |
| `void` | `setFormat` | 设置标签格式。 | `const QString& f` | public | — | 用户 | — |

Notes:
- 月档的毫秒近似：chooseStep 内部对"月"用固定近似（非真实历法月长）——刻度对齐从 epoch 0 整数倍，跨月边界可能偏移（已知近似，文档化）。
- 无新增信号/事件（继承 QChartAxis）。

## Overrided Qt Events:
None.

## Signals:
None.（继承 QChartAxis）
