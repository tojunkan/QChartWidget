# QDataRect Documentation

## Brief Introduction:
Data 空间矩形（值类型，QBarSeries 柱数据）：**左下角 + 右上角两角存储**（QDataPoint 对）——构造/设置顺序为 `(left, bottom, right, top)`（**注意与 QRectF 的 (left, top, right, bottom) 顺序不同**）。提供四角/四边访问与设置、`isValid`（全分量非空）。非 QObject，header-only。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QDataPoint` | `m_bottomLeft` | 左下角（x=left, y=bottom）。 | `QDataPoint` | 默认构造（空） | `QDataPoint` |
| `QDataPoint` | `m_topRight` | 右上角（x=right, y=top）。 | `QDataPoint` | 默认构造（空） | `QDataPoint` |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QDataRect` | 构造函数（默认/两角/四分量/QRectF 转换——explicit）。 | `无` <br> `QDataPoint bottomLeft, topRight` <br> `QVariant left, bottom, right, top` <br> `const QRectF& rect` | public | — | QBarSeries 数据/用户 | `QDataPoint` <br> `QRectF` |
| `QDataPoint` | `bottomLeft`/`bottomRight`/`topLeft`/`topRight` | 四角访问（bottomRight/topLeft 由两角分量合成）。 | 无 | public | `QDataPoint` | QBarSeries::rectangles/渲染 | `QDataPoint` |
| `QVariant` | `left`/`right`/`bottom`/`top` | 四边访问（内联）。 | 无 | public | `QVariant` | QBarSeries/测试 | — |
| `void` | `setBottomLeft`/`setTopRight`/`setBottomRight`/`setTopLeft` | 四角设置（bottomRight 只改 x 与 y；topLeft 只改 x 与 y——单分量更新语义）。 | `const QDataPoint& p` | public | — | 用户 | `QDataPoint` |
| `void` | `setLeft`/`setRight`/`setBottom`/`setTop` | 四边设置（内联）。 | `const QVariant& v` | public | — | 用户 | — |
| `void` | `setRect` | 一次性设置（`(left, bottom, right, top)` 顺序 / QRectF 重载）。 | `QVariant×4` <br> `const QRectF& rect` | public | — | 用户 | `QRectF` |
| `bool` | `isValid` | 有效性：四分量均非空（具体数值有效性由调用者判断）。 | 无 | public | `true`/`false` | QBarSeries/测试 | — |

Notes:
- **顺序陷阱**：`(left, bottom, right, top)`——与 QRectF `(left, top, width, height)` 完全不同（本类第二分量是 bottom 不是 top、第三/四是 right/top 不是 width/height）；QRectF 转换构造/设置处理了语义映射。
- **单分量更新语义**：setBottomRight 只更新 right(x) 与 bottom(y)（不动 left/top）——角更新保持"矩形的对边不变"语义。
- 值语义纯拷贝；非类成员（无信号/事件）。

## Overrided Qt Events:
None.

## Signals:
None.（非 QObject）
