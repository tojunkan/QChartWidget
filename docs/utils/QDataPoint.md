# QDataPoint Documentation

## Brief Introduction:
Data 空间 2D 点（值类型，design_notes §五空间模型）：存 QVariant 二元组——兼容 QValueAxis(qreal)/QDateTimeAxis(QDateTime)/QBarCategoryAxis(QString) 等所有 Axis 类型。**Series 不关心 Data 具体类型**（由 Axis::toNumeric 渲染时转换——"Series 只存 Data"零耦合红线）。非 QObject，无 moc，无 .cpp（header-only）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVariant` | `m_x` | dim0 数据（任意 Axis 类型）。 | `QVariant` | `{}`（空） | `QChartAxis` |
| `QVariant` | `m_y` | dim1 数据。 | `QVariant` | `{}`（空） | `QChartAxis` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QDataPoint` | 构造函数（QVariant 默认空）。 | `QVariant x={}, QVariant y={}` | public | — | QXYSeries 数据/用户 | — |
| `QVariant` | `x`/`y` | 分量访问器（内联）。 | 无 | public | `QVariant` | Series/测试 | — |
| `void` | `setX`/`setY` | 分量设置（内联）。 | `QVariant v` | public | — | 用户 | — |

Notes:
- **值语义**：纯值类型（拷贝语义，无指针/资源）；QVariant 空值 = "未设置"（QDataRect::isValid 检查）。
- **Data ≠ Numeric**：本类存 Data（QDateTime/QString 可入）；数值化发生在渲染时（Axis::toNumeric）——见 docs/axes/QChartAxis.md 语义表。
- 非类成员（无信号/事件）；与 QDataPoint3D 对称（定案：3D 新建类不扩展本类，零影响 2D，design_3d §6.1）。

## Overrided Qt Events:
None.

## Signals:
None.（非 QObject）
