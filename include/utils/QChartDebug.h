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

// 脏变量追踪（chart.dirty）——默认关；只用于验证"layout/camera 变化影响到哪一步"。
// 打开方式：QT_LOGGING_RULES="chart.*=false;chart.dirty=true"
Q_DECLARE_LOGGING_CATEGORY(logDirty)

#include <QElapsedTimer>
#include <QString>
#include <QDebug>

// 每条脏变量日志都带"进程启动以来的毫秒数"——人工操作（Win+→ / 拖动 / 滚轮）
// 无法脚本化，只有时间戳才能把用户动作与阶段执行对上。
namespace QChartDebug {
inline qint64 dirtyClockMs()
{
    static QElapsedTimer timer = []() { QElapsedTimer t; t.start(); return t; }();
    return timer.elapsed();
}
} // namespace QChartDebug

#define QCHART_DIRTY_MSG(text)                                                                     \
    qCDebug(logDirty).noquote()                                                                    \
        << (QStringLiteral("[dirty] t=%1ms ").arg(QChartDebug::dirtyClockMs()) + QString(text))

//调试时，去：项目 → 属性 → 配置属性 → 调试。
//在”环境”一栏写：
//QT_LOGGING_RULES=chart.axis.debug=true;chart.widget.debug=false等。

//以及，每个要写log的地方要引用这个头文件。