# QValueAxis Documentation

## Brief Introduction:
QValueAxis 是 2D 数值轴（继承 QChartAxis），Data 与 Numeric 之间为**恒等映射**（Data=qreal，toNumeric/fromNumeric 直通）。刻度用 **niceStep 算法**自动选"漂亮"步长（因子表 1/1.5/2/3/4/5/8/10 × 10^n）；支持 `tickInterval` 固定步长、`labelFormat` printf 格式串（如 "%g°"）与 `labelPrecision` 固定小数位三种标签/步长定制。S0 中该轴是轴管线测试的默认夹具轴（dim0/dim1）。

## Constant Variables:
None.（本类无新增常量；继承 QChartAxis 的 AXIS_MARGIN/TICK_LENGTH/SUB_TICK_LENGTH/TEXT_PADDING）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_tickInterval` | （private）固定主刻度步长；`0` = 自动（niceStep） | `qreal(>0)`/`0.0` | `0.0` | — |
| `int` | `m_labelPrecision` | （private）标签固定小数位数；`-1` = 自动去零 | `int(≥0)`/`-1` | `-1` | — |
| `QString` | `m_labelFormat` | （private）printf 风格标签格式串；空 = 不使用 | `QString` | 空 | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QValueAxis` | 构造；无特殊初始化（刻度依赖调用方传入范围） | `QObject* parent=nullptr, Qt::Alignment alignment=AlignBottom` | public | — | 用户/测试夹具 | `QChartAxis` |
| `qreal` | `toNumeric` | 覆写：Data(qreal) 直通；`toDouble` 失败返回 NaN + qWarning | `QVariant data` | public | `qreal`/NaN | 虚调用链（Series/Widget 阶段）；S0 无直接调用 | — |
| `QVariant` | `fromNumeric` | 覆写：Numeric → qreal 直通（NaN/Inf 原样） | `qreal num` | public | `QVariant(qreal)` | Widget 阶段消费 | — |
| `QVector<qreal>` | `tickValues` | 覆写：退化区间返回单刻度；有 tickInterval 用固定步长，否则 niceStep；ceil 对齐起点；对齐后空区间退化为等分；保底 ≥2 刻度（端点） | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `QChartAxis::drawAtPosition`/`drawAtEdge`（基类虚调用）、`QChartLayer::drawGrid`、TestAxisPipeline/TestAxisMatrix 夹具 | — |
| `QStringList` | `tickLabels` | 覆写：labelFormat 非空 → `QString::asprintf`；否则 labelPrecision≥0 → 定点 'f' 位数；否则 'f' 8 位后去尾零/去点 | `const QVector<qreal>& ticks` | public | `QStringList` | `drawAtPosition`/`drawAtEdge`（虚调用） | — |
| `QVector<qreal>` | `subTickValues` | 覆写：m_subTickCount≤0 返回空；否则按主刻度步长等分（含首尾外侧补点到范围边界） | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `drawAtEdge`（待 Widget 阶段）；S0 无调用方 | — |
| `void` | `setLabelFormat` | 设置 printf 格式串（内联，不发信号；优先级高于 labelPrecision） | `const QString& fmt` | public | — | 用户 | — |
| `QString` | `labelFormat` | 格式串访问器（内联） | 无 | public | `QString` | 测试/用户 | — |
| `int` | `labelPrecision` | 小数位访问器（内联） | 无 | public | `int` | 测试 | — |
| `void` | `setLabelPrecision` | 设置小数位（内联，不发信号） | `int n` | public | — | 用户 | — |
| `qreal` | `tickInterval` | 固定步长访问器（内联） | 无 | public | `qreal` | 测试 | — |
| `void` | `setTickInterval` | 设置固定步长：`v≤0` 复位为 0（自动）；**总是 emit tickCountChanged()** | `qreal v` | public | — | 用户 | — |
| `qreal` | `niceStep` | （private）核心算法：range/targetTicks → 数量级归一 → 因子表最近邻 × 10^n；非法区间返回 1.0 | `qreal range` | private | `qreal` | `tickValues` 内部 | — |

Notes:
- 继承 QChartAxis 的刻度/样式/语法糖成员与信号；本类 Q_PROPERTY：`tickInterval`、`labelPrecision`、`labelFormat`（均无 NOTIFY）。
- 标签格式优先级：`labelFormat` > `labelPrecision` > 自动去零。
- S0 实测用法：TestAxisPipeline/TestAxisMatrix 以 `QValueAxis` 直配 dim0（AlignBottom）与 dim1（AlignLeft）轴、tickCount=5，经 `drawAtPosition` 提交图元走双后端管线；CPU 8/8、GL 8/8 组合矩阵均由此轴产生刻度/标签（Cartesian 10、Polar 11 个标签断言）。

## Overrided Qt Events:
无（QObject 非 QWidget；本类无 Qt 事件覆写）。

## Signals:
None.（无新增信号；注意 `setTickInterval` 触发继承的 `QChartAxis::tickCountChanged`）
