# QChartLayer3D Documentation

## Brief Introduction:
QChartLayer3D 是 **3D 图层**（继承 QChartLayer，Q_OBJECT；批次 B1 起入编译面）：**纯 Numeric 图元组装、无投影**——`collectPrimitives()` 把轴/网格/盒边框经 2D 轴 `drawAtPosition` 生成 Numeric 图元写入 `m_scene3D`（scene3D.camera 构造注入本层相机值成员 m_camera3D，模式同 2D layer 批次 A）。配置面：三轴（setAxisX/Y/Z + 专用 Z；重绑时同步 `m_axes3D` 编排器配置）、域盒 `setDataBounds`（同步 axes3D.dataBounds 与工作副本）、网格模式 `GridMode{Box, Lattice}`（默认 Lattice）、3D 投影指针（仅用于采样提示/默认盒；图元组装不依赖）。收集内容：三主轴（各维 min 角出发，带刻度点+标签）、网格（Box=底面 zMin 双族；Lattice=三族固定 (v,w) 网格脊）、盒 12 边（QChartAxes3D::boxCorners/boxEdges，spine 边 {0,4,8} 按轴色 2px、其余灰 1px）。系列管理（addSeries3D/removeSeries3D 声明于头、定义与调用待 3D 系列阶段——**当前勿引用**）、ProjectFn3D/轴标题/worldCache 块注释保留。

## Constant Variables:
None.（嵌套 `GridMode{Box, Lattice}` 为类型定义非类常量）

## Member Variables:

| Type | Name | Description | Available Value | Default Value | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `QChartAxis*` | `m_axisZ` | （protected）Z 轴绑定（axisX/Y 继承自基类） | 指针/`nullptr` | `nullptr` | `QChartAxis` |
| `QList<QChartSeries3D*>` | `m_series3D` | （protected）3D 系列列表（本阶段空；系列阶段恢复） | 空 | 空 | `QChartSeries3D` |
| `const QChartProjection3D*` | `m_projection3D` | （protected）3D 投影指针（非持有；采样提示/默认盒用） | 指针/`nullptr` | `nullptr` | `QChartProjection3D` |
| `QChartCamera3D` | `m_camera3D` | （protected）★ 3D 相机值成员（批次 B1：相机归层；scene3D.camera=&m_camera3D 于构造注入） | `QChartCamera3D` | 构造创建 | `QChartCamera3D` |
| `QChartScene` | `m_scene3D` | （protected）3D 场景快照（collectPrimitives 填充；camera 恒指向 m_camera3D，projection/plotArea/背景由容器注入） | `QChartScene` | 构造创建 | `QChartScene` |
| `std::unique_ptr<QChartAxes3D>` | `m_axes3D` | （protected）轴参照系编排器（构造创建；axes3D() 暴露） | — | 构造创建 | `QChartAxes3D` |
| `GridMode` | `m_gridMode` | （protected）网格模式 | Box/Lattice | `Lattice` | — |
| `QVector3D` | `m_axesDataMin/Max` | （protected）collect 工作副本（setDataBounds 同步） | `QVector3D` | `(0,0,0)`/`(10,10,10)` | — |

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartLayer3D` | 构造：建 m_axes3D（dataBounds=投影默认盒或 (0,0,0)-(10,10,10)）；axis(0..2) 初始指向 m_axisX/Y/Z（均空）；scene3D.camera=&m_camera3D；工作副本默认 0..10 | `QObject* parent=nullptr` | public | — | QChartWidget3D 构造 | — |
| `void` | `setAxisX/setAxisY` | 覆写：基类绑定 + 同步 axes3D axis(0/1).axis | `QChartAxis* a` | public | — | QChartWidget3D::setAxisX3D/Y3D、用户 | `QChartAxes3D` |
| `void` | `setAxisZ` | Z 轴绑定 + 同步 axes3D axis(2).axis | `QChartAxis* a` | public | — | QChartWidget3D::setAxisZ3D、用户 | — |
| `QChartAxis*` | `axisZ` | Z 轴访问器（内联） | 无 | public | 指针 | 测试 | — |
| `void` | `addSeries3D/removeSeries3D` | 3D 系列管理——**头声明存在，cpp 未定义**（本阶段无编译引用；待 3D 系列阶段实现） | `QChartSeries3D* s` | public | — | 勿引用（链接错误） | `QChartSeries3D` |
| `QList<QChartSeries3D*>` | `series3DList` | 系列列表访问器（内联） | 无 | public | 空 | 系列阶段 | — |
| `void` | `setProjection3D` | 设 3D 投影指针（采样提示/默认盒；不参与组装） | `const QChartProjection3D* proj` | public | — | QChartWidget3D::setProjection3D | — |
| `const QChartProjection3D*` | `projection3D` | 投影访问器（内联） | 无 | public | 指针 | pushContext（经容器）、widget | — |
| `QChartAxes3D*` | `axes3D` | 编排器访问器（内联；非 const/const） | 无 | public | 指针 | QChartWidget3D::fitWorld（数据盒回退）、collect | `QChartAxes3D` |
| `void` | `setGridMode` | 设网格模式（内联） | `GridMode m` | public | — | demo_axis3d、矩阵测试 | — |
| `GridMode` | `gridMode` | 网格模式访问器（内联） | 无 | public | Box/Lattice | 测试 | — |
| `void` | `setDataBounds` | 设轴/网格数据盒：axes3D.dataBounds + 工作副本同步 | `const QVector3D& dataMin, const QVector3D& dataMax` | public | — | QChartWidget3D::setDomainBox/clearDomainBox | — |
| `bool` | `hasValidDataBounds` | 数据盒有效性（axes3D.dataBounds.isValid） | 无 | public | `true`/`false` | collect 守卫 | — |
| `void` | `collectPrimitives` | 图元收集（纯 Numeric）：场景负载复位（primitives/labels/maxSourceId=0/前缀和 {0}）→ axes3D 配置重同步 → 校验（axes3D 可见 + 数据盒有效）→ 主轴 3 条（drawAtPosition、标签开、轴色 2px）→ 网格（Box：z=zMin 底平面 X/Y 两族 tick 脊；Lattice：U/V/W 三族固定 (v,w) 脊，gridColor 1px）→ 盒 12 边（corners/edges/spineIdx；spine 边轴色 2px、非 spine 灰 1px）；addLine 回填 color/penWidth/sourceId/前缀和 | 无 | public | — | QChartWidget3D::renderLayers/renderLayersGL、冒烟/矩阵测试直接调用 | `QChartAxis` <br> `QChartAxes3D` |
| `QChartCamera3D*` | `camera3D` | 相机访问器（内联；非 const/const，返回 &m_camera3D） | 无 | public | 指针 | QChartWidget3D 转发、矩阵测试（setYaw/Pitch） | — |
| `void` | `setScene3DProjection/PlotArea/Background` | scene3D 上下文注入（内联） | 指针/`QRectF`/`QColor` | public | — | QChartWidget3D::pushContextToLayers | — |
| `const QChartScene&`/`QChartScene&` | `scene3D` | 3D 快照访问器（collectPrimitives 之后使用） | 无 | public | — | 容器渲染（拷贝）、测试 | — |

Notes:
- 轴仍为 2D QChartAxis 实例（默认 QValueAxis）：Numeric 空间即轴数值化结果；dim0/1/2 对应 X/Y/Z（addLine dimIndex 0/1/2）。
- 与 2D QChartLayer 关系：继承其轴绑定/网格样式/相机归层骨架；3D 不走基类 drawGrid（自实现三族网格）；基类 gridColor()/theme 默认色 (220,220,220) 用于网格脊。
- S0 实测调用方：QChartWidget3D（构造/渲染链）、demo_axis3d（setGridMode）、TestAxes3DSmoke/TestAxes3dMatrix/TestWidget3D 系列（直接构造、collectPrimitives 后渲染断言）。

## Overrided Qt Events:
无（QObject）。

## Signals:
None.（无新增；继承 gridChanged 等基类信号）
