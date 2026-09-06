# QCube Documentation

> 值类型文档形态（非 QObject：无 Qt 事件/信号；总括登记见 `docs/stages/S0_axis_pipeline.md` §5 第 22 项）。本类为 3D 轴对齐包围盒工具（QRectF 的 3D 类比），随 3D 阶段全面投入使用；S0 内仅随 QPainterChartRenderer 的 #if 0 3D 块保留引用（QCube 引用随 3D 恢复）。

## Brief Introduction:
QCube 是 **3D 轴对齐包围盒值类型**（struct，公开成员 min/max:QVector3D）：默认构造生成**无效立方体**（min=+∞、max=−∞）；两角点构造自动归一化（min≤max）；点集构造（点数≤1 → 无效盒 + qWarning）。提供 有效性/空判（isValid：各轴 min≤max；isNull：无效或任一处 qFuzzyIsNull 尺寸）、center/size/width/height/depth、normalized（归一化副本）、translate/translated/scale（相对中心缩放）/moveCenter/adjust（六向边界调整，类 QRect::adjust）、contains（点/盒，含边界）、intersects（含边界接触）、intersected/united（不相交交集 → 无效盒）、赋值/比较运算符。调试流输出 `operator<<(QDebug, QCube)`。

## Constant Variables:
None.（头内被注释的"中心+尺寸构造"为历史残注）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVector3D` | `min` | 最小角点（公开；默认构造置 +∞） | `QVector3D` | `{+∞,+∞,+∞}` | — |
| `QVector3D` | `max` | 最大角点（公开；默认构造置 −∞） | `QVector3D` | `{−∞,−∞,−∞}` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QCube` | 默认构造：无效盒（±inf） | 无 | public | — | 无效盒兜底路径（intersected 不相交等） | — |
| — | `QCube` | 复制构造 | `const QCube& other` | public | — | 值传递 | — |
| — | `QCube` | 两角点构造（自动归一化 min≤max） | `const QVector3D& p1, const QVector3D& p2` | public | — | QChartCamera3D setViewCube、3D 图元盒 | — |
| — | `QCube` | 点集构造：空/单点 → 无效盒 + qWarning；否则遍历取各轴 min/max | `const QVector<QVector3D>& points` | public | — | 3D 图元包围盒（QCube(prim.cartVerts).center() 深度等，3D 阶段） | — |
| `bool` | `isValid` | 有效性：三轴均 min≤max（内联） | 无 | public | `true`/`false` | 归一化/空判 | — |
| `bool` | `isNull` | 空判：无效或宽/高/深任一 qFuzzyIsNull（内联） | 无 | public | `true`/`false` | — | — |
| `QVector3D` | `center` | 中心 = (min+max)×0.5（内联） | 无 | public | `QVector3D` | 3D 深度排序（#if0 块） | — |
| `QVector3D` | `size` | 尺寸 = max−min（内联） | 无 | public | `QVector3D` | — | — |
| `qreal` | `width`/`height`/`depth` | 各轴跨度（内联） | 无 | public | `qreal` | 空判 | — |
| `QCube` | `normalized` | 归一化副本（已有效则原样返回；否则按 min/max 交换生成合法盒）（内联） | 无 | public | `QCube` | 非规范化输入兜底 | — |
| `void` | `translate` | 原地平移（内联） | `const QVector3D& offset` | public | — | 3D 变换链 | — |
| `QCube` | `translated` | 平移返回新对象（内联） | `const QVector3D& offset` | public | `QCube` | — | — |
| `void` | `scale` | 相对中心缩放（半尺寸×factor）（内联） | `qreal factor` | public | — | 3D 相机适配 | — |
| `void` | `moveCenter` | 移动中心保持尺寸（内联） | `const QVector3D& newCenter` | public | — | — | — |
| `void` | `adjust` | 六向调整边界（左/上/前/右/下/后）（内联） | `qreal left, qreal top, qreal front, qreal right, qreal bottom, qreal back` | public | — | — | — |
| `bool` | `contains` | 点含边界（内联） | `const QVector3D& point` | public | `true`/`false` | 3D 裁剪（#if0 块） | — |
| `bool` | `contains` | 盒包含（min/max 均含）（内联） | `const QCube& other` | public | `true`/`false` | — | — |
| `bool` | `intersects` | 相交（边界接触算相交）（内联） | `const QCube& other` | public | `true`/`false` | 3D 裁剪（#if0 块） | — |
| `QCube` | `intersected` | 交集；不相交 → 无效盒（内联） | `const QCube& other` | public | `QCube` | — | — |
| `QCube` | `united` | 并集最小包围盒（内联） | `const QCube& other` | public | `QCube` | — | — |
| `QCube&` | `operator=` | 赋值（显式；自赋值守卫）（内联） | `const QCube& other` | public | 引用 | — | — |
| `bool` | `operator==`/`operator!=` | 逐 min/max 比较（内联） | `const QCube& other` | public | `true`/`false` | — | — |
| `QDebug` | `operator<<` | 自由函数：`QCube(min=…, max=…)` 流输出（内联 QDebug + QDebugStateSaver） | `QDebug debug, const QCube& cube` | 自由函数 | `QDebug` | 调试 | — |

Notes:
- S0 运行时无调用方（3D 未入子集）；实测引用点：QPainterChartRenderer #if0 3D 块（QCube(prim.cartVerts).center()/lineCube/rectCube/aabb）、QChartLayer3D/QChartCamera3D/QChartWidget3D/QChartAxes3D（均 3D 阶段）。
- 头文件含注释残件（中心+尺寸构造），文档按现行代码（未启用）记录。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
