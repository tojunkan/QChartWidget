# QChartDebug Documentation

## Brief Introduction:
日志分类声明头（工具）：14 个 `Q_DECLARE_LOGGING_CATEGORY` 分类（QLoggingCategory 体系），供全库 qCDebug/qCWarning 使用（D8：`*` 通配符只能出现在模式末尾；verbose 分类默认静默）。运行期用 `QT_LOGGING_RULES` 环境变量控制（如 `chart.axis.debug=true`）。文档结构按队长指示调整为**宏清单表**（无类）。

## Constant Variables:
None.

## Member Variables:
None.（纯宏声明，无状态）

## Macros List (Q_DECLARE_LOGGING_CATEGORY):

| Macro Name | Description | Default Level | Used By |
| :---: | :---: | :---: | :---: |
| `logAxis` | 轴的日志分类 | 默认 | QChartAxis.cpp（样式/绑定调试） |
| `logWidget` | 控件的日志分类 | 默认 | QChartWidget.cpp / QChartWidget3D.cpp（投影切换/布局/fit） |
| `logLayer` | 图层的日志分类 | 默认 | QChartLayer.cpp（绘制/网格） |
| `logSeries` | 数据的日志分类 | 默认 | QChartSeries 族 |
| `logProjection` | 投影的日志分类 | 默认 | QChartProjection.h（Polar 极点等） |
| `logFactory` | 工厂的日志分类 | 默认 | QChartProjectionFactory.h（创建分派） |
| `logValueAxis` | 数值轴的日志分类 | 默认 | QValueAxis.cpp（tick 生成） |
| `logDateTimeAxis` | 日期时间轴的日志分类 | 默认 | QDateTimeAxis.cpp（刻度/单位选择） |
| `logCategoryAxis` | 分类轴的日志分类 | 默认 | QBarCategoryAxis.cpp |
| `logLogAxis` | 对数轴的日志分类 | 默认 | QLogAxis.cpp |
| `logRender` | 绘制部分的日志分类 | 默认 | QPainterChartRenderer.cpp 等 |
| `logAxisVerbose` | 轴每帧细节（TICK SKIP 等） | **默认关** | 轴绘制细节 |
| `logRenderVerbose` | 渲染每帧细节（createPath 采样等） | **默认关** | createPath（QChartProjection.h）/渲染 |
| `logSeriesVerbose` | Series 每帧绘制细节 | **默认关** | 系列绘制细节 |

Notes:
- **D8 约束**：`QT_LOGGING_RULES` 的 `*` 通配符只能出现在模式末尾（如 `chart.*.verbose=false`），否则整条规则被 Qt 忽略。
- **verbose 纪律**：每帧细节一律走 `*Verbose` 分类（默认关）——避免刷屏；测试/demo 按需开（Test/test.cpp 设 `*.verbose=false` 兜底）。
- 定义处：QChartDebug.h 仅声明；各分类的 `Q_LOGGING_CATEGORY` 定义在对应 .cpp（使用方需 include 本头）。
- 无类/无信号/无事件。

## Overrided Qt Events:
None.

## Signals:
None.
