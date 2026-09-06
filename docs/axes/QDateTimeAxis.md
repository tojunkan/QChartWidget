# QDateTimeAxis Documentation

## Brief Introduction:
QDateTimeAxis 是日期时间轴（2D，继承 QChartAxis）：Data = QDateTime（用户领域类型），`toNumeric = toMSecsSinceEpoch()`、`fromNumeric = fromMSecsSinceEpoch(qint64(num))`——Numeric 空间 = epoch 毫秒（qreal）。`tickValues` 按范围自动选时间单位（1s→10y 阶梯表 + `chooseStep` 档位上调），刻度对齐 epoch 整数倍；`tickLabels` 支持用户 `format`（QDateTime::toString 格式）或按相邻刻度最小间隔推断格式；`subTickValues` 用所择单位的建议细分。提供 `setRange(QDateTime,QDateTime)` 便捷重载。构造把默认主刻度数调为 7。S0 未直接测试本轴（旧 test_qdatetimeaxis 未纳入 S0 列表）。

## Constant Variables:
None.（本类无类级常量；时间单位表为 cpp 内全局静态 `QList<TimeUnit>`：步长 ms + 显示格式 + 建议次刻度数，非类成员）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QString` | `m_format` | （private）用户自定义 QDateTime 显示格式；空 = 自适应推断 | `QString` | 空 | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QDateTimeAxis` | 构造；默认 tickCount=7 | `QObject* parent=nullptr, Qt::Alignment alignment=AlignBottom` | public | — | 用户 | — |
| `qreal` | `toNumeric` | 覆写（内联）：Data(QDateTime) → epoch 毫秒 qreal | `QVariant data` | public | `qreal` | 虚调用链（Series/Widget 阶段） | — |
| `QVariant` | `fromNumeric` | 覆写（内联）：Numeric(ms) → `QDateTime::fromMSecsSinceEpoch`（qint64 截断） | `qreal num` | public | `QVariant(QDateTime)` | Widget 阶段消费 | — |
| `QVector<qreal>` | `tickValues` | 覆写：`chooseStep` 定步长；起点对齐 epoch 步长整数倍；退化返回单刻度；不足 2 个保底端点 | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `drawAtPosition`/`drawAtEdge`（虚调用） | — |
| `QStringList` | `tickLabels` | 覆写：m_format 非空 → 直接 `toString(m_format)`；否则取相邻刻度最小间隔经时间单位表 `inferFormatFromInterval` 推断格式 | `const QVector<qreal>& ticks` | public | `QStringList` | `drawAtPosition`/`drawAtEdge`（虚调用） | — |
| `QVector<qreal>` | `subTickValues` | 覆写：`chooseStep` 的建议细分 n≤0 返回空；否则主刻度间均分插入（限界内） | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `drawAtEdge`（待 Widget 阶段） | — |
| `void` | `setRange` | 便捷重载：QDateTime 版 setRange——先 toNumeric 再走基类 `setRange(qreal,qreal)`（→ rangeChanged + styleChanged） | `const QDateTime& min, const QDateTime& max` | public | — | 用户/测试 | `QChartAxis` |
| `QString` | `format` | 显示格式访问器（内联） | 无 | public | `QString` | 测试 | — |
| `void` | `setFormat` | 设置显示格式（内联，不发信号） | `const QString& f` | public | — | 用户 | — |
| `TimeStepInfo` | `chooseStep` | （private）按 rangeMs 择档：目标 ticks=max(2,m_tickCount)；先取首个 `unit.ms ≥ range/(target-1)` 的单位，估计刻度数 > 2.5×target 时升档；返回 {stepMs, format, subDivisions} | `qint64 rangeMs` | private | `TimeStepInfo` | `tickValues`/`tickLabels`/`subTickValues` 内部 | — |

Notes:
- Q_PROPERTY：`format`（无 NOTIFY）。
- 私有嵌套 `TimeStepInfo`（stepMs/format/subDivisions）；时间单位表 1s("hh:mm:ss")→10y("yyyy") 共 18 档，见 cpp。
- 该类无 S0 期测试直接覆盖（S0 轴矩阵用 QValueAxis）；经 QChartAxis 虚接口由 Layer/Widget 阶段消费，行为由旧 test_qdatetimeaxis 参考。

## Overrided Qt Events:
无（QObject 非 QWidget；本类无 Qt 事件覆写）。

## Signals:
None.（无新增信号；setRange 便捷重载触发继承的 rangeChanged/styleChanged）
