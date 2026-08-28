# QChartProjectionFactory Documentation

## Brief Introduction:
投影工厂（2D）：根据 `CoordinateSystem` 枚举创建对应 Projection 实例；`createFunctional` 创建用户自定义投影（lambda 版）。**分派表语义**：Cartesian → QCartesianProjection；Polar → QPolarProjection；Functional → 拒绝（缺 lambda，返回 nullptr + qWarning）；未知枚举 → 回退 Cartesian + qWarning。静态纯函数集（无状态、非 QObject）。消费方：QChartWidget（默认投影）、demo、ProjectionToolKit（utils）。

## Constant Variables:
None.

## Member Variables:
None.（纯静态）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `std::unique_ptr<QChartProjection>` | `create` | **分派表**：`Cartesian`→QCartesianProjection；`Polar`→QPolarProjection；`Functional`→qWarning+nullptr（缺 lambda，提示用 createFunctional）；未知枚举→qWarning+回退 Cartesian。 | `CoordinateSystem type` | public static | `unique_ptr<QChartProjection>` <br> `nullptr`（Functional） | `QChartWidget`（默认投影）/demo/测试 | `QChartProjection` <br> `QCartesianProjection` <br> `QPolarProjection` |
| `std::unique_ptr<QChartProjection>` | `createFunctional` | 创建用户自定义投影（QFunctionalProjection）：forward 必传、backward 可选、defaultBounds、dataToView/viewToData 可选、轴名可选。 | `forward` <br> `backward=nullptr` <br> `defaultBounds=QRectF(0,0,10,10)` <br> `dataToView=nullptr` <br> `viewToData=nullptr` <br> `name0="x", name1="y"` | public static | `unique_ptr<QChartProjection>` | `ProjectionToolKit`（utils：恒等/Power2/Exp/Log）/demo（swirl 等） | `QFunctionalProjection` |

Notes:
- 分派完整流程见 docs/projection/QChartProjectionFactory_create_flow.md。
- Functional 拒绝语义：create(Functional) 是**契约性失败**（非回退）——调用方应改用 createFunctional；未知枚举才是回退。
- 无信号/事件（非 QObject）。

## Overrided Qt Events:
None.

## Signals:
None.
