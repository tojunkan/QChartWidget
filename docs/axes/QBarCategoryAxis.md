# QBarCategoryAxis Documentation

## Brief Introduction:
分类轴（2D，离散域）：`toNumeric(QString)` = 类别索引（`idx/(size−1)` 归一化到 [0,1] Numeric 范围）、`fromNumeric` = `categories[index]`。`tickValues` = 每个类别的整数索引（每类一个刻度）。`isInteractive()==false`（离散域 pan/zoom 撕裂标签与数据——Widget 在该维禁交互）。Q_PROPERTY×1（categories）。**Data≠Numeric 语义表**：QString ↔ 类别索引。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QStringList` | `m_categories` | 类别列表（顺序即索引；Q_PROPERTY categories）。 | `QStringList` | 空 | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QBarCategoryAxis` | 构造函数。 | `QObject* parent=nullptr` <br> `Qt::Alignment alignment=Qt::AlignBottom` | public | — | 用户/demo（柱状图） | — |
| `QStringList` | `categories` | 类别访问器（内联）。 | 无 | public | `QStringList` | 测试/用户 | — |
| `void` | `setCategories` | 设置类别（重设后触发 rangeChanged 等）。 | `const QStringList& cats` | public | — | 用户 | — |
| `qreal` | `toNumeric` | `idx/(size−1)` 归一化 [0,1]（未找到/空 → NaN）。 | `QVariant data` | public override | `qreal` | DrawContext 闭包 | — |
| `QVariant` | `fromNumeric` | `categories[index]`（非有限/空 → 空串）。 | `qreal num` | public override | `QVariant` | 交互反向 | — |
| `QVector<qreal>` | `tickValues` | **每类一刻度**：`0..size−1` 整数索引（忽略入参区间）。 | `qreal, qreal`（忽略） | public override | `QVector<qreal>` | Widget 2D / `QChartAxes3D::ticks` | `QChartAxes3D` |
| `QStringList` | `tickLabels` | 类别名列表。 | `const QVector<qreal>& ticks` | public override | `QStringList` | Widget 2D / `QChartAxes3D::tickLabelTexts` | `QChartAxes3D` |
| `bool` | `isInteractive` | **false**（离散域无交互意义）。 | 无 | public override | `false` | `QChartWidget::dimensionInteractive`（该维禁 pan/zoom） | `QChartWidget` |

Notes:
- 归一化语义：toNumeric 输出 [0,1]（首个 0、末个 1）——Numeric 空间是类别索引归一化，非数据值。
- 离散域红线：Widget 在 dimensionInteractive 检测到绑定分类轴后该维不平移/缩放（QChartWidget::mouseMoveEvent/wheelEvent 置 factor=1/dx=0）。
- 无新增信号/事件（继承 QChartAxis）。

## Overrided Qt Events:
None.

## Signals:
None.（继承 QChartAxis）
