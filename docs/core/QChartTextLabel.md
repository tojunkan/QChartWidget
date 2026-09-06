# QChartTextLabel Documentation

## Brief Introduction:
QChartTextLabel 是**文本标签结构体**（非 QObject，纯数据）：附着于场景的排版单元，含 文字内容/样式 + 归属标识 + 双空间锚点 + 运行时可见性。收集端（轴 drawAtPosition）填 `text/color/fontSize/alignment/numericAnchor/sourceId/refPrimitiveId`；Renderer 步骤 2 填 `cartesianAnchor` 与 `visible`（cullAndResolveLabels）。绑定语义：`refPrimitiveId ≥ 0` = 绑定图元下标（锚=该图元 cartA，可见性=该图元可见性——S0 轴标签走此路线，R2 修复）；`refPrimitiveId == -1` = **自由标签**（CPU 端以 sourceId 查该组最后可见图元定位/显隐；GPU 端不支持自由标签，直接不可见）。`pixelpos` 声明注释为"步骤 3 填入"，但 S0 双后端实际在 drawLabels 内以 `camera->project(cartesianAnchor)` 得局部像素坐标，该字段未被写入（保留字段）。

## Constant Variables:
None.

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QString` | `text` | 标签文字 | `QString` | 空 | — |
| `QColor` | `color` | 文字颜色 | `QColor` | 无效 | — |
| `float` | `fontSize` | 字号（点） | `float` | `10.0f` | — |
| `Qt::Alignment` | `alignment` | 对齐/避让方向（AlignCenter=自动避让四方最远侧；边对齐按位判定） | `Qt::Alignment` | `Qt::AlignCenter` | — |
| `int` | `sourceId` | 归属（Axis/Series 的 ID；自由标签定位用） | `int` | `-1` | — |
| `int` | `refPrimitiveId` | 绑定的图元 ID（**实际语义：图元向量下标**；-1 = 自由标签） | `int(≥0)`/`-1` | `-1` | `QChartPrimitive` |
| `QVector3D` | `numericAnchor` | 收集时填入的 Numeric 锚点 | `QVector3D` | `{0,0,0}` | — |
| `QVector3D` | `cartesianAnchor` | 步骤 2 由 Renderer 填入的 Cartesian 锚点（绑定标签=所绑图元 cartA；自由标签=组内最后可见图元 cartA） | `QVector3D` | `{0,0,0}` | — |
| `QPointF` | `pixelpos` | 声明"步骤 3 由 Renderer 填入"；**S0 双后端均未写入**（局部变量计算像素位），保留字段 | `QPointF` | `{0,0}` | — |
| `bool` | `visible` | 运行时可见性（Renderer 步骤 2 维护） | `true`/`false` | `true` | — |

## Member Functions (signals and overrided Qt events are not included):
None.（纯数据结构，无成员函数）

Notes:
- CPU 端可见性规则实测（QPainterChartRenderer::cullAndResolveLabels）：绑定标签 → `visible = visibility[refPrimitiveId]`；自由标签 → sourceId 组内存在可见图元才可见。
- GL 端（QOpenGLChartRenderer::cullAndResolveLabels）：绑定标签锚=`proj->toCartesian(prim.numA)` 且恒可见；自由标签恒不可见（不支持）。
- 轴标签样式实测：fontSize=10、AlignCenter、颜色=轴色；空文本不产生标签。

## Overrided Qt Events:
无（非 QObject，无事件）。

## Signals:
None.（非 QObject，无信号）
