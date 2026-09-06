# QFunctionalProjection Documentation

## Brief Introduction:
QFunctionalProjection 是**用户自定义坐标投影**（2D，header-only，继承 QChartProjection）：以 lambda 定义 Numeric ↔ View Cartesian 映射，无需子类化——最简用法只传 `forward`（+可选 backward）；适用于鱼眼、扭曲 Cartesian 等场景。`backward` 缺省时 fromCartesian 返回 (NaN,NaN) + qWarning；包络 `dataToView/viewToData` 缺省时**回退采样法**：computeDataBounds 32×32 采样 fromCartesian、computeViewRect 16×16 采样 toCartesian（全 NaN → 恒等回退）。NaN/Inf 自然传播（调用方负责跳过）。默认域 (0,0,10,10)。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `std::function<QPointF(qreal,qreal)>` | `m_forward` | （private）正向映射（Numeric → Cartesian） | 函数对象/`nullptr` | 构造必传 | — |
| `std::function<QPointF(qreal,qreal)>` | `m_backward` | （private）反向映射（Cartesian → Numeric） | 函数对象/`nullptr` | `nullptr` | — |
| `QRectF` | `m_defaultBounds` | （private）默认 Numeric 范围 | `QRectF` | 构造入参默认 (0,0,10,10) | — |
| `std::function<QRectF(const QRectF&)>` | `m_dataToView` | （private）dataBounds → viewRect 包络 | 函数对象/`nullptr` | `nullptr`（=恒等） | — |
| `std::function<QRectF(const QRectF&)>` | `m_viewToData` | （private）viewRect → dataBounds 包络 | 函数对象/`nullptr` | `nullptr`（=恒等） | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QFunctionalProjection` | 构造：收 5 个 lambda + 维度名（move 进成员；QChartProjection(name0,name1)） | `std::function<QPointF(qreal,qreal)> forward, std::function<QPointF(qreal,qreal)> backward=nullptr, QRectF defaultBounds=(0,0,10,10), std::function<QRectF(const QRectF&)> dataToView=nullptr, std::function<QRectF(const QRectF&)> viewToData=nullptr, QString name0="x", QString name1="y"` | public | — | 用户（lambda 场景：鱼眼/扭曲等） | — |
| `CoordinateSystem` | `type` | 覆写（内联）：Functional | 无 | public | `CoordinateSystem::Functional` | — |
| `QPointF` | `toCartesian` | 覆写：m_forward 空 → qWarning + (NaN,NaN)；否则调 forward（NaN/Inf 自然传播） | `qreal num0, qreal num1` | public | `QPointF` | 经 final vec3 包装被渲染器调用 | — |
| `QPointF` | `fromCartesian` | 覆写：m_backward 空 → qWarning + (NaN,NaN)；否则调 backward | `qreal x, qreal y` | public | `QPointF` | 反算/包络采样 | — |
| `QRectF` | `computeDataBounds` | 覆写：m_viewToData 非空 → 直接调用；否则 32×32 采样 fromCartesian 求域（有限点）；全 NaN → 恒等回退 viewRect | `const QRectF& viewRect` | public | `QRectF` | Widget 阶段 | — |
| `QRectF` | `computeViewRect` | 覆写：m_dataToView 非空 → 直接调用；否则 16×16 采样 toCartesian 求 Cartesian 包围盒；全 NaN → 恒等回退 dataBounds | `const QRectF& dataBounds` | public | `QRectF` | Widget 阶段 | — |
| `QRectF` | `defaultDataBounds` | 覆写（内联）：返回 m_defaultBounds | 无 | public | `QRectF` | Widget 首次初始化 | — |

Notes:
- glslToCartesian/glslFromCartesian 未覆写——基类两纯虚悬空，**本类为抽象类，不可实例化**（S0 亦无实例；如未来 GPU 使用须补 GLSL 表达式覆写，或仅走 CPU 且经子类化补齐）。文档按现行头文件记录。
- S0 无测试实例；行为由旧 test_qchartprojection 参考。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
