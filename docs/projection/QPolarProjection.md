# QPolarProjection Documentation

## Brief Introduction:
QPolarProjection 是**极坐标投影**（2D，header-only，继承 QChartProjection）：dim0 = 角度（度）、dim1 = 半径（物理单位）。`toCartesian(θ°, r)`：x=r·cos(θ·π/180)、y=r·sin(θ·π/180)；`fromCartesian(x,y)`：θ=atan2∈[0°,360°)（负角 +2π）、r=√(x²+y²)，**极点（r≈0）处 θ 无定义 → (NaN, 0)** + qCDebug(logProjection)。GLSL 镜像实现（radians/degrees 内建；fromCartesian 表达式以 length<1e-8 判奇点）。`computeDataBounds`：32×32 网格采样 fromCartesian，精确 rMin（视口含原点 → 0）、跨 0°/覆盖正 X 轴检测 → 返回完整圆盘角度域（QRectF(0, rMin, nextafter(360°,−∞), rMax−rMin)）；`computeViewRect`：内外弧 32 段采样求扇形 Cartesian 轴对齐包围盒。默认范围 (0,0,360,10)（完整圆盘 r=10）。注：header 内 qCDebug(logProjection) 需要类别定义——S0 临时定义在 QInterpolatedProjection.cpp（R12 遗留）。

## Constant Variables:
None.（采样网格 32、弧采样 32 为 cpp/头内局部常量，非类成员）

## Member Variables:
None.（无状态）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QPolarProjection` | 构造（内联）：父类传名 "θ","r" | 无 | public | — | 用户/测试夹具 | `QChartProjection` |
| `CoordinateSystem` | `type` | 覆写（内联）：Polar | 无 | public | `CoordinateSystem::Polar` | 测试分支判断 | — |
| `QPointF` | `toCartesian` | 覆写（内联）：θ 度转弧度 → (r·cos, r·sin) | `qreal num0, qreal num1` | public | `QPointF` | 经 final vec3 包装被 CPU 渲染器调用（逐顶点） | — |
| `QPointF` | `fromCartesian` | 覆写（内联）：r=√(x²+y²)；极点 → (NaN,0)+qCDebug；θ=atan2 负角转 [0,2π) → 度 | `qreal x, qreal y` | public | `QPointF` | computeDataBounds 采样内部 | — |
| `QString` | `glslToCartesian` | 覆写（内联）：`vec3(num.y*cos(radians(num.x)), num.y*sin(radians(num.x)), 0.0)`（num.x=θ°、num.y=r） | 无 | public | `QString` | QChartGL Shader 注入 | `QChartGL` |
| `QString` | `glslFromCartesian` | 覆写（内联）：三目——length(cart.xy)<1e-8 → vec3(0/0,0,0)（NaN 奇点）；否则 degrees(atan)+条件 2π 归位 + length | 无 | public | `QString` | 未来 GPU 反算 | — |
| `QRectF` | `computeDataBounds` | 覆写：原点在视口 → rMin=0；32×32 采样求 θMin/Max 与 rMax；跨 0°（θSpan>180°）或矩形覆盖正 X 轴 → 全圆盘角度域；否则正常 θ/r 域（θ 序错乱 swap 修正）；兜底 r 非有限 | `const QRectF& viewRect` | public | `QRectF` | Widget 阶段（S0 无调用方）；computeViewRect 等测试参考 | — |
| `QRectF` | `computeViewRect` | 覆写：内外弧（rMin/rMax）各 32 段采样 toCartesian 求扇形 Cartesian 轴对齐包围盒（径向边含于端点采样） | `const QRectF& dataBounds` | public | `QRectF` | Widget 阶段 | — |
| `QRectF` | `defaultDataBounds` | 覆写（内联）：(0, 0, 360, 10) 完整圆盘 r=10 | 无 | public | `QRectF` | Widget 首次初始化 | — |

Notes:
- 采样法包络为通用参考实现（QFunctionalProjection 同款）；跨 0° 处理与"覆盖正 X 轴"检测为 computeDataBounds 核心修复点（header 内注释）。
- S0 实测（TestAxisPipeline/TestAxisMatrix Polar 分支）：外环 r=10（dim0Axis.drawAtPosition(0,360,10,0,0,…)）+ θ=0 径向脊 + 网格同心环/辐条像素断言全绿。

## Overrided Qt Events:
无（非 QObject）。

## Signals:
None.（非 QObject，无信号）
