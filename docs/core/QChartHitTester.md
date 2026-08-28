# QChartHitTester Documentation

## Brief Introduction:
统一反向映射引擎（pixel→data；D27 定案：拾取不是 renderer 职责，hitTest 统一进本类）。三实现：①2D `hitTest(pixel, seriesList, toPixel, ctx)`（顶层可见系列优先遍历 + series->hitTest 像素 in 多边形，原 QChartLayer::hitTest 逻辑零行为搬入）；②3D `hitTest(pixel, primitives, maxDistPx=8)`（只扫 `Layer==Series` 图元，点到点/点到线段距离 < 阈值取最近，dataIndex 透传，原 QChartWidget3D::updateHover 近邻核心搬入）；③GPU `hitTestGPU(r,g,b, pickTable)`（RGB24 → ID → 查表 → HitResult，纯函数可单测）。统一结果 `HitResult{series, index, dataIndex}`（2D 下 dataIndex==index）。**非 Q_OBJECT**（纯静态引擎，无状态）。

## Constant Variables:
None.

## Member Variables:
None.（纯静态函数集，无实例状态）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `HitResult` | `hitTest` | **2D**：顶层可见系列优先（自后向前）+ `series->hitTest`（像素 in 多边形）；未命中返回空（dataIndex==-1）。 | `const QPointF& pixel` <br> `const QList<QChartSeries*>& seriesList` <br> `std::function<QPointF(QVariant,QVariant)> toPixel` <br> `const DrawContext* ctx` | public static | `HitResult{series,index,dataIndex}` | `QChartWidget::mouseMoveEvent`（悬停，经 QChartLayer::hitTest 兼容入口） | `QChartSeries` <br> `DrawContext` |
| `HitResult` | `hitTest` | **3D**：只扫 `Layer==Series` 层图元；点到点/点到线段距离 < maxDistPx 取最近；dataIndex 透传；未命中返回空。 | `const QPointF& pixel` <br> `const QVector<QChartPrimitive>& primitives` <br> `qreal maxDistPx`（默认 8.0） | public static | `HitResult` | `QChartWidget3D::updateHover`（CPU 后端分支） | `QChartPrimitive` |
| `qreal` | `distanceToPrimitive` | 像素到图元距离（Point=|pos−a|；LineSegment=点到线段距离）——命中后收紧全局阈值等复用。 | `const QPointF& pos` <br> `const QChartPrimitive& prim` | public static | `qreal` | 3D hitTest/调用方复用 | `QChartPrimitive` |
| `HitResult` | `hitTestGPU` | **GPU 解码（纯函数，无 GL 依赖可单测）**：`id = r | g<<8 | b<<16`；`id==0xFFFFFF`（哨兵：背景/轴网格 Decor 片段）或越界 → 空 HitResult。 | `uint8_t r, g, b` <br> `const QVector<PickRecord>& pickTable` | public static | `HitResult` | `QChartWidget3D::updateHover`（GL 后端分支，读 1×1 RGB） | `PickRecord` |

Notes:
- **共享类型**（本头定义）：`HitResult{QChartSeries* series; int index; int dataIndex}`——⚠ `series` **非 const**（任务描述草稿的 const 按此修正：既有调用方 `QChartSeries* s = result.series` 赋值零改动，audit C1）；`PickRecord{QChartSeries* series; int dataIndex; QChartPrimitive::Layer layer}`（GPU 拾取表项：与 GL 批次同步构建；轴/网格不编码 → dataIndex=-1）。
- 纯重构红线：三实现均为既有逻辑原样搬入（2D 顶层优先/可见性过滤、3D Series 层过滤/点与线段距离/8px 阈值/dataIndex 透传），行为零变化。

## Overrided Qt Events:
None.（非 QWidget）

## Signals:
None.（非 QObject）
