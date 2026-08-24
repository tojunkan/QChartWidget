#pragma once
// QChartDebug.h
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(logAxis)       // 轴的日志分类
Q_DECLARE_LOGGING_CATEGORY(logWidget)     // 控件的日志分类
Q_DECLARE_LOGGING_CATEGORY(logLayer)      // 图层的日志分类
Q_DECLARE_LOGGING_CATEGORY(logSeries)     // 数据的日志分类
Q_DECLARE_LOGGING_CATEGORY(logProjection) // 投影的日志分类
Q_DECLARE_LOGGING_CATEGORY(logFactory)    // 工厂的日志分类
Q_DECLARE_LOGGING_CATEGORY(logValueAxis)    // 数值轴的日志分类
Q_DECLARE_LOGGING_CATEGORY(logAxisVerbose)   // 轴每帧细节（TICK SKIP等）——默认关
Q_DECLARE_LOGGING_CATEGORY(logRenderVerbose) // 渲染每帧细节（createPath采样等）——默认关
Q_DECLARE_LOGGING_CATEGORY(logSeriesVerbose)  // Series 每帧绘制细节——默认关
Q_DECLARE_LOGGING_CATEGORY(logDateTimeAxis)  // 日期时间轴的日志分类
Q_DECLARE_LOGGING_CATEGORY(logCategoryAxis)  // 分类轴的日志分类
Q_DECLARE_LOGGING_CATEGORY(logLogAxis)       // 对数轴的日志分类

Q_DECLARE_LOGGING_CATEGORY(logRender)     // 绘制部分的日志分类

//调试时，去：项目 → 属性 → 配置属性 → 调试。
//在”环境”一栏写：
//QT_LOGGING_RULES=chart.axis.debug=true;chart.widget.debug=false等。

//以及，每个要写log的地方要引用这个头文件。