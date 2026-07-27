#pragma once
// QChartDebug.h
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(logAxis)    // 声明轴的日志分类
Q_DECLARE_LOGGING_CATEGORY(logWidget)  // 声明控件的日志分类
Q_DECLARE_LOGGING_CATEGORY(logGeometry)// 声明坐标系的日志分类
Q_DECLARE_LOGGING_CATEGORY(logSeries)  // 声明数据的日志分类

//调试时，去：项目 → 属性 → 配置属性 → 调试。
//在“环境”一栏写：
//QT_LOGGING_RULES=chart.axis.debug=true;chart.widget.debug=false等。

//以及，每个要写log的地方要引用这个头文件。