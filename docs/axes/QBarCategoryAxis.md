# QBarCategoryAxis Documentation

## Brief Introduction:
QBarCategoryAxis 是分类轴（2D，离散域，继承 QChartAxis）：Data = QString 类别名（"苹果"、"香蕉"）或 qreal/qint 直通；Numeric = 类别**索引经线性映射到 [m_numericMin, m_numericMax]**（旧域 [0,n-1] 归一化；映射区间由 `setNumericMapping`/`setCategories` 维护）。`tickValues` = 每类别一个整数索引刻度；`tickLabels` = 对应类别名。离散域无 pan/zoom 意义 → `isInteractive()==false`（Widget 在该维禁交互）；基类 qreal 版语法糖 `setRange` 被同名覆写**屏蔽**（类别由数据决定，调用仅 qWarning）。类别增删改均会触发 rangeChanged+styleChanged。S0 未直接测试本轴。

## Constant Variables:
None.（本类无新增常量）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QStringList` | `m_categories` | （private）类别列表（顺序即索引） | `QStringList` | 空 | — |
| `qreal` | `m_numericMin` | （private）线性映射区间下界 | `qreal` | `0.0` | — |
| `qreal` | `m_numericMax` | （private）线性映射区间上界（setCategories 时按 n-1 自动更新；类别数为 0 时钳 0） | `qreal` | `0.0` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QBarCategoryAxis` | 构造 | `QObject* parent=nullptr, Qt::Alignment alignment=AlignBottom` | public | — | 用户 | — |
| `QStringList` | `categories` | 类别访问器（内联） | 无 | public | `QStringList` | 测试 | — |
| `void` | `setCategories` | 整体设置类别：更新 m_numericMax=n-1、语法糖范围 [-0.5, n-0.5]；发 rangeChanged + styleChanged | `const QStringList& cats` | public | — | 用户/数据链 | — |
| `void` | `appendCategory` | 尾部追加类别；更新 sugarMax=size-0.5；发 rangeChanged + styleChanged | `const QString& cat` | public | — | 用户/数据链 | — |
| `void` | `insertCategory` | 指定位置插入（越界 qWarning 忽略）；发 rangeChanged + styleChanged | `int index, const QString& cat` | public | — | 用户 | — |
| `void` | `removeCategory` | 移除指定位置（越界 qWarning 忽略）；发 rangeChanged + styleChanged | `int index` | public | — | 用户 | — |
| `void` | `clearCategories` | 清空；语法糖范围复位 [-0.5,-0.5]；发 rangeChanged + styleChanged | 无 | public | — | 用户 | — |
| `qreal` | `toNumeric` | 覆写（内联）：QVariant 为 Double/Int/LongLong → 直通小数索引；否则按类别名 indexOf → 归一化线性映射到 [m_numericMin,m_numericMax]；查无/空类别 → NaN | `QVariant data` | public | `qreal`/NaN | 虚调用链（Series/Widget 阶段） | — |
| `QVariant` | `fromNumeric` | 覆写（内联）：非有限或空类别 → 空串；否则逆线性映射 round 取索引 → 类别名（越界空串） | `qreal num` | public | `QVariant(QString)` | Widget 阶段消费 | — |
| `void` | `setNumericMapping` | 设置线性映射区间；发 styleChanged | `qreal numericMin, qreal numericMax` | public | — | 用户/数据链 | — |
| `qreal` | `numericMin` | 映射下界访问器（内联） | 无 | public | `qreal` | 测试 | — |
| `qreal` | `numericMax` | 映射上界访问器（内联） | 无 | public | `qreal` | 测试 | — |
| `QVector<qreal>` | `tickValues` | 覆写：忽略传入范围，返回 0..n-1 整数索引刻度 | `qreal numericMin, qreal numericMax` | public | `QVector<qreal>` | `drawAtPosition`/`drawAtEdge`（虚调用） | — |
| `QStringList` | `tickLabels` | 覆写：刻度整数索引取类别名（越界空串） | `const QVector<qreal>& ticks` | public | `QStringList` | `drawAtPosition`/`drawAtEdge`（虚调用） | — |
| `QVector<qreal>` | `subTickValues` | 覆写（内联）：恒空（分类轴无次刻度） | `qreal, qreal` | public | 空 | — | — |
| `void` | `setRange` | 覆写（内联，**隐藏**基类 qreal 版）：忽略参数 + qWarning（类别由数据决定） | `qreal, qreal` | public | — | 用户误用路径 | — |
| `bool` | `isInteractive` | 覆写（内联）：恒 false（离散域禁 pan/zoom） | 无 | public | `false` | Widget 阶段交互判定 | — |

Notes:
- Q_PROPERTY：`categories`（无 NOTIFY）。
- 类别变更通过 sugarMin=-0.5/sugarMax=n-0.5 语法糖 + rangeChanged 通知 Widget（类别"带半格"中心对齐）；继承的 `min()/max()` 返回该语法糖值。
- 该轴在 S0 测试矩阵之外（QValueAxis 充当夹具轴），行为由旧 test_qbarcategoryaxis 参考。

## Overrided Qt Events:
无（QObject 非 QWidget；本类无 Qt 事件覆写）。

## Signals:
None.（无新增信号；类别变更触发继承的 `rangeChanged(qreal,qreal)` 与 `styleChanged`）
