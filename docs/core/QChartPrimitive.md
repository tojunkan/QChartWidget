# QChartPrimitive Documentation

## Brief Introduction:
QChartPrimitive 是**图元结构体**（非 QObject，纯数据）：渲染管线的几何单元，携带 双空间几何 + 样式 + 归属标识。收集端填 **Numeric 空间**（numA/numB/numRect/numVerts/numIndices），Renderer 步骤 2 填 **Cartesian 空间**（cartA/cartB/cartRect/cartVerts/cartIndices，与 Numeric 一一对应）。类型分 9 种（Type 枚举：点/线/矩形/椭圆/多边形/路径/三类网格）。注释契约：`id` 为全局唯一 ID（收集时赋值为向量下标，S0 收集路径尚未回填，默认 -1）；`sourceId` 归属（Axis/Series 的 ID）；**scene 即巨大数组，下标即索引**（`refPrimitiveId` 标签绑定用图元下标，S0 实测以向量下标为准）。样式：主色/填充色/线宽/点大小（绘制半径=markerSize×0.5）/3D CPU 深度。

## Constant Variables:
None.（Type 枚举为类型定义非常量成员：`Point/Line/Rect/Ellipse/Polygon/Path/TriangleMesh/TriangleFan/TriangleStrip`）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `Type` | `type` | 几何类型 | 9 值 | `Type::Point` | — |
| `int` | `id` | （注释契约）全局唯一 ID，收集时赋值为向量下标；**S0 未回填，恒 -1**（下标即索引，id 待 Widget/拾取阶段落实） | `int` | `-1` | — |
| `int` | `sourceId` | 归属（Axis/Series 的 ID；layer addLine 回填 maxSourceId，直接 drawAtPosition 装配时 -1） | `int(≥0)`/`-1` | `-1` | — |
| `QColor` | `color` | 主色（线/边框/点色） | `QColor` | 无效 | — |
| `QColor` | `fillColor` | 填充色（Rect/Ellipse/Polygon/Mesh 用） | `QColor` | 无效 | — |
| `float` | `penWidth` | 线宽（像素） | `float` | `1.0f` | — |
| `float` | `markerSize` | Point 标记大小（头注释称半径；绘制实测半径 = markerSize×0.5，即按直径语义消费） | `float` | `4.0f` | — |
| `float` | `depth` | 3D CPU 排序用深度（GPU 后端兼作 layer 推断：>0.5=Grid、>1.0=Decor） | `float` | `0.0f` | — |
| `QVector3D` | `numA` | Numeric 空间：Point 位置 / Line 起点 / Rect Ellipse 左上角 | `QVector3D` | `{0,0,0}` | — |
| `QVector3D` | `numB` | Numeric 空间：Line 终点 | `QVector3D` | `{0,0,0}` | — |
| `QRectF` | `numRect` | Numeric 空间：Rect/Ellipse 轴对齐盒（左上+宽高） | `QRectF` | `{0,0,0,0}` | — |
| `QVector<QVector3D>` | `numVerts` | Numeric 空间：Polygon/Path/Mesh 顶点序列 | `QVector<QVector3D>` | 空 | — |
| `QVector<int>` | `numIndices` | TriangleMesh/Fan/Strip 索引 | `QVector<int>` | 空 | — |
| `QVector3D` | `cartA` | Cartesian 空间：对应 numA（Renderer 步骤 2） | `QVector3D` | `{0,0,0}` | — |
| `QVector3D` | `cartB` | Cartesian 空间：对应 numB | `QVector3D` | `{0,0,0}` | — |
| `QRectF` | `cartRect` | Cartesian 空间：对应 numRect（恒等投影直接复制；否则退化转换，见渲染器文档） | `QRectF` | `{0,0,0,0}` | — |
| `QVector<QVector3D>` | `cartVerts` | Cartesian 空间：对应 numVerts | `QVector<QVector3D>` | 空 | — |
| `QVector<int>` | `cartIndices` | 通常直接复用 numIndices，仅变换后需重排时使用 | `QVector<int>` | 空 | — |

## Member Functions (signals and overrided Qt events are not included):
None.（纯数据结构，无成员函数）

Notes:
- 头部注释含已删字段残注（`// int PrimitiveId = -1;`），S0 以向量下标代替——文档按现行代码描述。
- 生产方式实测：轴 `drawAtPosition` 产出 1 条 Path 轴脊（segments+1 顶点）+ 每主刻度 7 个 Point（中心 markerSize=2.0 + 6 方向臂长 (dimMax-dimMin)×TICK_LENGTH）+ 绑定标签；网格脊由 layer addLine 回填 sourceId/color。
- 消费方式实测：CPU 2D 按类型转像素绘制；GL 按类型打包 VBO（Path→逐段 GL_LINES、Rect→6 顶点双三角、Ellipse→16 扇区 48 顶点等）。

## Overrided Qt Events:
无（非 QObject，无事件）。

## Signals:
None.（非 QObject，无信号）
