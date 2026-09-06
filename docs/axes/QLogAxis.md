# QLogAxis Documentation

## Brief Introduction:
QLogAxis 是对数轴（2D，继承 QChartAxis）：Data = qreal（>0），`toNumeric = log10(v)`（v≤0 → NaN），`fromNumeric = pow(10, num)`——Numeric 空间是对数量级。刻度在 log 空间生成：默认每数量级一个刻度（step=1），窄范围自动降半步（range<1.5 → 0.5）与 1/4 步（range<0.5 → 0.25）；tickLabels 把 Numeric 刻度逆变换回数值格式化（大/小量级用科学计数）。构造时把默认主刻度数调为 6（基类 5）。S0 未直接测试本轴（旧 test_qlogaxis 未纳入 S0 测试列表）。

## Constant Variables:
None.（本类无新增常量；继承 QChartAxis 常量）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_base` | （private）对数目底数（暂仅支持 10；标签逆变换与格式化使用） | `qreal(>1)` | `10.0` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QLogAxis` | 构造；默认 tickCount=6（比基类 5 少一格） | `QObject* parent=nullptr, Qt::Alignment alignment=AlignBottom` | public | — | 用户 | — |
| `qreal` | `toNumeric` | 覆写（内联）：Data(qreal>0) → `log10(v)`；`v≤0` 返回 NaN（无警告） | `QVariant data` | public | `qreal`/NaN | 虚调用链（Series/Widget 阶段） | — |
| `QVariant` | `fromNumeric` | 覆写（内联）：Numeric → `pow(10.0, num)` | `qreal num` | public | `QVariant(qreal)` | Widget 阶段消费 | — |
| `QVector<qreal>` | `tickValues` | 覆写：log 空间均匀步长 1/0.5/0.25；ceil 对齐；退化（min≥max）返回单刻度；不足 2 个保底端点 | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `drawAtPosition`/`drawAtEdge`（虚调用） | — |
| `QStringList` | `tickLabels` | 覆写：`pow(m_base, t)` 逆变换后格式化——值 ≥1e4 或 ≤1e-3 用 "%1e%2" 科学计数（尾数 g3 + floor(t) 指数），否则 'g' 4 位 | `const QVector<qreal>& ticks` | public | `QStringList` | `drawAtPosition`/`drawAtEdge`（虚调用） | — |
| `QVector<qreal>` | `subTickValues` | 覆写：m_subTickCount≤0 返回空；否则主刻度间 log 空间线性等分插入 | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `drawAtEdge`（待 Widget 阶段） | — |
| `qreal` | `base` | 底数访问器（内联） | 无 | public | `qreal` | 测试 | — |
| `void` | `setBase` | 设置底数；`b≤1` 忽略 + qWarning | `qreal b` | public | — | 用户 | — |

Notes:
- Q_PROPERTY：`base`（无 NOTIFY）。
- 数值化语义与 QValueAxis 不同（Data≠Numeric），同链不同轴并存时由各轴独立数值化；本轴 `m_base` 仅影响标签逆变换（Numeric 恒为 log10）。
- 该类无 S0 期测试直接覆盖（TestAxisPipeline/TestAxisMatrix 使用 QValueAxis）；经 QChartAxis 虚接口由 Layer/Widget 阶段消费，行为由旧 test_qlogaxis 参考。

## Overrided Qt Events:
无（QObject 非 QWidget；本类无 Qt 事件覆写）。

## Signals:
None.（无新增信号）
