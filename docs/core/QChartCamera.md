# QChartCamera Documentation

## Brief Introduction:
相机基类：2D/3D 视图状态变化共用信号 `viewChanged` 的抽象基类（QObject）。本身**不持有任何视图状态**——2D 状态在子类 QChartCamera2D（viewRect 几何），3D 状态在 QChartCamera3D（viewCube + orientation + fovY）。设计定案（D1/R5）：**相机 = 纯映射器**，不反算 dataBounds、不拥有 plotArea、不碰事件（事件由 Widget 层转发几何操作）。

## Constant Variables:
None.

## Member Variables:
（基类无成员变量——状态全部在子类）

## Member Functions (signals and overrided Qt events are not included):

| Return Value Type | Name | Description | Parameters | Declared Field | Available Value | Called By | Related Classes |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| — | `QChartCamera` | 构造函数（空实现，仅传递 parent）。 | `QObject* parent` | public | — | 子类构造 | — |
| — | `~QChartCamera` | 虚析构（default）。 | — | public | — | 子类析构 | — |

Notes:
- 本类存在的意义 = 统一 `viewChanged` 信号 + 多态基类（`unique_ptr<QChartCamera>` 可容纳 2D/3D 相机）；公共接口全部在子类。

## Overrided Qt Events:
None.（非 QWidget，无 Qt 事件）

## Signals:
| Name | Description | Parameters | Connected slots | Related Classes |
| :---: | :---: | :---: | :---: | :---: |
| `viewChanged` | 视图状态变化广播：2D 相机（viewRect/center/zoom 变化）与 3D 相机（viewCube/yaw/pitch/fovY 变化）共用。 | — | 2D：内部未连线（QChartWidget 自行 emit 自身 viewChanged；外部/动画按需连接） <br> 3D：`QChartWidget3D` 连接 ×2（反算 dataBounds + 推轴盒 + 重绘，src/core/QChartWidget3D.cpp:95/142） | `QChartCamera2D` <br> `QChartCamera3D` <br> `QChartWidget3D` |

Notes:
- 发射点：子类各 setter 在**值实际变化**时才 emit（避免无效重算风暴）。
- 2D 链路中 Widget 不依赖本信号（pan/zoom 由 Widget 方法内联 emit 自身 viewChanged），本信号主要服务 QPropertyAnimation（center/zoom 属性动画）与外部监听。
