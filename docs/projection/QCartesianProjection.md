# QCartesianProjection Documentation

## Brief Introduction:
QCartesianProjection 是**笛卡尔坐标投影**（2D，header-only，继承 QChartProjection）：Numeric 空间 ≡ View Cartesian 空间——`toCartesian/fromCartesian` **恒等映射**，`computeDataBounds/computeViewRect` 互为恒等（dataBounds ≡ viewRect）。GLSL 表达式相应为恒等包装。`isIdentityMapping()==true`（CPU 渲染器 Rect/Ellipse 直算快速通道）。默认范围 (0,0,10,10)。S0 实测：CPU/GL 轴管线矩阵的 Cartesian 分支夹具（-10..10 视窗）。

## Constant Variables:
None.

## Member Variables:
None.（无状态——恒等映射不需成员）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QCartesianProjection` | 构造（内联）：父类传名 "x","y" | 无 | public | — | 用户/测试夹具（unique_ptr 实例化） | `QChartProjection` |
| `CoordinateSystem` | `type` | 覆写（内联）：Cartesian | 无 | public | `CoordinateSystem::Cartesian` | 测试分支判断 | — |
| `bool` | `isIdentityMapping` | 覆写（内联）：true（恒等快速通道） | 无 | public | `true` | CPU transformNumericToCartesian | — |
| `QPointF` | `toCartesian` | 覆写（内联）：恒等 `(num0, num1)` | `qreal num0, qreal num1` | public | `QPointF` | 经 final vec3 包装被渲染器调用 | — |
| `QPointF` | `fromCartesian` | 覆写（内联）：恒等 `(x, y)` | `qreal x, qreal y` | public | `QPointF` | 反算路径 | — |
| `QString` | `glslToCartesian` | 覆写（内联）：`"vec3(num.x, num.y, 0.0)"` | 无 | public | `QString` | QChartGL Shader 注入 | `QChartGL` |
| `QString` | `glslFromCartesian` | 覆写（内联）：`"vec3(cart.x, cart.y, 0.0)"` | 无 | public | `QString` | 未来 GPU 反算 | — |
| `QRectF` | `computeDataBounds` | 覆写（内联）：恒等返回 viewRect | `const QRectF& viewRect` | public | `QRectF` | Widget 阶段 | — |
| `QRectF` | `computeViewRect` | 覆写（内联）：恒等返回 dataBounds | `const QRectF& dataBounds` | public | `QRectF` | Widget 阶段 | — |
| `QRectF` | `defaultDataBounds` | 覆写（内联）：(0,0,10,10)（同基类默认） | 无 | public | `QRectF` | Widget 首次初始化 | — |

Notes:
- 全部实现内联于头文件（header-only，无 cpp——QCHART_SOURCES 无本类源文件）。
- S0 实测（TestAxisPipeline/TestAxisMatrix Cartesian 分支）：轴脊/刻度点像素断言与网格采样点全部基于本投影恒等语义。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
