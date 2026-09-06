# QChartAbstractCamera Documentation

## Brief Introduction:
QChartAbstractCamera 是相机抽象基类（QObject，Q_OBJECT），统一 2D/3D 相机的两套消费接口：
- **GPU 矩阵接口**（纯虚）：`viewMatrix()`（世界→视图）、`projectionMatrix(aspect)`（视图→裁剪），并给出非纯虚的合并实现 `viewProjectionMatrix(aspect) = projectionMatrix × viewMatrix`——GL 后端以 `u_viewProj` 上传；
- **CPU 投影接口**（纯虚）：`project(cart, plotArea)`（Cartesian → 屏幕点+深度+原坐标回传）与 `unproject(pixel, plotArea)`（屏幕 → 世界射线；2D 下退化：原点在 z=0 平面、方向为 +z）；
- **适配接口**：`fitToPlotArea(plotArea)` 调整相机使视图覆盖绘图区（返回是否变化）。
`viewChanged` 信号在一切视图状态变化时发射。S0 派生：QChartCamera（2D，子集中）；QChartCamera3D（3D 阶段恢复）。S0 场景快照以 `const QChartAbstractCamera*` 挂在 `QChartScene::camera`（只读指针，生命周期由调用方保证）。

## Constant Variables:
None.（本类无常量；同头文件定义两个辅助结构，非类成员：`QChartProjectedPoint{screen:QPointF; depth:qreal; cart:QVector3D}` 与 `Ray{origin:QVector3D; direction:QVector3D}`）

## Member Variables:
None.（纯接口；无数据成员——`viewChanged` 为 moc 信号，非成员变量）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartAbstractCamera` | 构造（内联）：仅转交 parent | `QObject* parent=nullptr` | public | — | 派生类构造（QChartCamera） | — |
| — | `~QChartAbstractCamera` | 虚析构（内联 default） | 无 | public | — | — | — |
| `virtual QMatrix4x4` | `viewMatrix` | **纯虚**：世界 → 视图矩阵 | 无 | public | `QMatrix4x4` | `viewProjectionMatrix`（合并）、GL 相机消费方 | — |
| `virtual QMatrix4x4` | `projectionMatrix` | **纯虚**：视图 → 裁剪矩阵（2D 下单位阵，aspect 忽略） | `qreal aspect` | public | `QMatrix4x4` | `viewProjectionMatrix` | — |
| `virtual QMatrix4x4` | `viewProjectionMatrix` | 非纯虚（内联）：`projectionMatrix(aspect) * viewMatrix()` 合并；等价于 QChartCamera3D 中的覆写 | `qreal aspect` | public | `QMatrix4x4` | `QOpenGLChartRenderer::drawPass`（setUniformValue u_viewProj） | — |
| `virtual QChartProjectedPoint` | `project` | **纯虚**：Cartesian → 屏幕点（可能出 plotArea）+ 深度 + 原坐标回传 | `const QVector3D& cart, const QRectF& plotArea` | public | `QChartProjectedPoint` | `QPainterChartRenderer::drawPrimitives2D/drawLabels2D`、GL `drawLabels`、基类 drawLabels 参考定义 | — |
| `virtual Ray` | `unproject` | **纯虚**：屏幕像素 → 世界射线（拾取用；S0 拾取后置，无调用方） | `const QPointF& pixel, const QRectF& plotArea` | public | `Ray` | S0 无（拾取阶段恢复） | `QChartHitTester` |
| `virtual bool` | `fitToPlotArea` | **纯虚**：调整相机使视图覆盖绘图区；返回 true=发生调整 | `const QRectF& plotArea` | public | `true`/`false` | Widget 布局阶段（QChartAbstractWidget 已调用，未入 S0） | — |

## Overrided Qt Events:
无（QObject 非 QWidget；本类无 Qt 事件覆写）。

## Signals:

| Name | Description | Parameters | Emitted By | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `viewChanged` | 任何视图状态变化均发射 | 无 | 派生类各视图 setter（QChartCamera::setViewRect/setCenter/pan/zoom/fit 等实测；QChartCamera3D 同） | `QChartCamera` |
