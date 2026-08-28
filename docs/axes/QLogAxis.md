# QLogAxis Documentation

## Brief Introduction:
对数数值轴（2D）：`toNumeric = log10(v)`（Data qreal>0）、`fromNumeric = pow(10, v)`（design_notes §Data≠Numeric 语义表）。**刻度在对数域（Numeric 空间）生成**——tickValues 的 range 是 log 数量级跨度：默认每数量级一个刻度（step=1），窄范围降半/1/4 数量级（step=0.5/0.25）。tickLabels 把 Numeric 刻度逆变换回数值后格式化（科学计数/有效数字）。Q_PROPERTY×1（base）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `qreal` | `m_base` | 对数底（Q_PROPERTY base；toNumeric 用 log10 恒为 10 底——base 用于标签回显）。 | `qreal > 0` | 构造传入（默认 10） | — |

Notes:
- toNumeric 恒为 log10（Numeric 空间是 10 的对数）；m_base 影响 fromNumeric/tickLabels 的底回显（base≠10 时数值回显为 m_base^t）。

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QLogAxis` | 构造函数。 | `QObject* parent=nullptr` <br> `Qt::Alignment alignment=Qt::AlignBottom` <br>（可能含 base 参数） | public | — | 用户/demo/测试 | — |
| `qreal` | `toNumeric` | `log10(data)`（Data qreal>0；≤0 → NaN 策略）。 | `QVariant data` | public override | `qreal` | DrawContext 闭包 | — |
| `QVariant` | `fromNumeric` | `pow(m_base, num)`。 | `qreal num` | public override | `QVariant` | 交互反向 | — |
| `QVector<qreal>` | `tickValues` | **对数域刻度**：range（log 跨度）<1.5 → step=0.5（半数量级）；<0.5 → step=0.25（1/4 数量级）；否则 step=1（每数量级）；ceil 对齐 + 0.001 容差；保障 ≥2 刻度。 | `qreal numericMin, qreal numericMax` | public override | `QVector<qreal>` | Widget 2D / `QChartAxes3D::ticks`（3D 委托） | `QChartAxes3D` |
| `QStringList` | `tickLabels` | 回显：`value=m_base^t`；`value≥1e4 或 ≤1e-3` → 科学计数 `%1e%2`；否则 `'g',4` 有效数字。 | `const QVector<qreal>& ticks` | public override | `QStringList` | Widget 2D / `QChartAxes3D::tickLabelTexts` | `QChartAxes3D` |

Notes:
- **与 QValueAxis 的对数域差异**（flow 重点）：QValueAxis 在数据域算 niceStep；QLogAxis 在 **log 数量级域**定步长（跨数量级/半数量级）——范围窄时降档避免刻度过稀。
- 无新增信号/事件（继承 QChartAxis）。

## Overrided Qt Events:
None.

## Signals:
None.（继承 QChartAxis）
