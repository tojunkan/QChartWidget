# QChartScene Documentation

## Brief Introduction:
QChartScene 是**场景快照结构体**（非 QObject，纯 POD 容器）：渲染管线的"输入输出中转站"。收集端（轴 `drawAtPosition`/`QChartLayer::drawGrid`）向 `primitives`（Numeric 坐标）与 `labels`（Numeric 锚点）追加内容，并维护 `maxSourceId`（下一个新 sourceId）与 `PrimitiveIdPrefixSum`（各 sourceId 图元数前缀和，构造预留 {0} 便于前缀和计算）；渲染端读 `camera/projection/plotArea` 等环境指针完成 变换→裁剪→绘制。Renderer 步骤 2 把 cart* / label 状态写回 scene 内的图元与标签对象。**camera/projection 为只读指针，生命周期由调用方保证**（S0：测试栈/值成员相机；批次 A 后 QChartLayer 以值成员 `m_camera` 持有并把 `scene.camera=&m_camera` 注入自身 `m_scene`）。

## Constant Variables:
None.（无常量）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QVector<QChartPrimitive>` | `primitives` | 所有图元（Numeric 坐标；步骤 2 后填入 cart*） | `QVector<QChartPrimitive>` | 空 | `QChartPrimitive` |
| `QVector<QChartTextLabel>` | `labels` | 所有标签（Numeric 锚点；步骤 2 后解析 cartesianAnchor/visible） | `QVector<QChartTextLabel>` | 空 | `QChartTextLabel` |
| `int` | `maxSourceId` | 所有图元/标签的最大 sourceId（Widget buildScene 时分配新 ID 用） | `int` | `0` | — |
| `QVector<int>` | `PrimitiveIdPrefixSum` | 所有 sourceId 的 PrimitiveId 前缀和（分配新 PrimitiveId 用；默认构造预留 {0}） | `QVector<int>` | `{0}`（ctor `PrimitiveIdPrefixSum(1,0)`） | — |
| `const QChartAbstractCamera*` | `camera` | 映射相机只读指针（GPU 矩阵/CPU project 均经此） | 指针/`nullptr` | `nullptr` | `QChartAbstractCamera` <br> `QChartCamera` |
| `const QChartAbstractProjection*` | `projection` | 投影只读指针（Numeric→Cartesian 变换与 GLSL 注入） | 指针/`nullptr` | `nullptr` | `QChartAbstractProjection` |
| `QRectF` | `plotArea` | 像素坐标系绘制区域（裁剪/绘制/clip 基准） | `QRectF` | `QRectF()` | — |
| `QColor` | `backgroundColor` | 画布底色（invalid=透明；S0 由调用方填充测试底色） | `QColor` | 无效色 | — |
| `bool` | `exportMode` | 导出模式（跳过调试/交互元素） | `true`/`false` | `false` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartScene` | 构造：`PrimitiveIdPrefixSum(1,0)` 预留一个元素，方便前缀和计算 | 无 | public | — | 一切场景装配方 | — |

Notes:
- 本类无方法（除隐式构造）；字段全部 public。
- 数据流实测：`QChartAxis::drawAtPosition` 追加 Path/Point 与标签（直接下标引用 primitives）；`QChartLayer::drawGrid::addLine` 在 drawAtPosition 后回填 color/penWidth/sourceId 并 push 前缀和；Renderer `cullAndResolveLabels` 以 `scene.maxSourceId` 建 `lastVisibleIndex` 数组、以 `refPrimitiveId` 绑定标签；GL 上传批次直接遍历 primitives。

## Overrided Qt Events:
无（非 QObject，无事件）。

## Signals:
None.（非 QObject，无信号）
