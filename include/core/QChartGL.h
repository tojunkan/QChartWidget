// QChartGL.h —— GL 资源池（design_phase3.md §7.3，A2/A3：共享/惰性/引用计数；t42 骨架）
// 职责：
//   - sharedContext()：共享根（惰性：registerHost 首实例创建 QOffscreenSurface+QOpenGLContext；
//     后续 GlHost 经 setShareContext 共享（Qt ≥6.5 才有该 API）；无存活实例返回 nullptr——零资源）
//   - program(ShaderKind)：程序池（share group 内复用，引用计数随实例增减；shader 编译属实现③ t44，
//     本任务留接口 + 引用计数骨架，program() 暂返回 nullptr 占位）
//   - surfaceFormat()：QSurfaceFormat 统一（3.3 Core、depth 24，§7.3）
// 非 Q_OBJECT（moc 约定：无信号/槽；新 Q_OBJECT 类只进库 AUTOMOC——本类不进）。
#ifndef QCHARTGL_H
#define QCHARTGL_H

#include <QSurfaceFormat>

class QOpenGLContext;
class QOpenGLShaderProgram;

/// 程序池键（design_phase3.md §4，§7.3）：主 pass line/point + ID pass pick
enum class ShaderKind { Line, Point, Pick };

class QChartGL {
public:
    /// 共享上下文（A2/A3 惰性）：registerHost() 首实例创建共享根并设为共享根，
    /// 后续 QOpenGLWidget share 之（Qt ≥6.5 setShareContext）；无存活实例返回 nullptr（零资源）
    static QOpenGLContext* sharedContext();
    /// 程序池：按 kind 缓存已编译程序（share group 内复用，引用计数随实例增减）。
    /// ⚠ t44 填充（shader 编译属实现③）；当前返回 nullptr 占位（一次性 qWarning）
    static QOpenGLShaderProgram* program(ShaderKind kind);
    /// 释放程序池（最后实例析构时自动调用；也可显式）
    static void releasePrograms();
    /// QSurfaceFormat 统一（§7.3）：3.3 Core、depth 24（宿主 GlHost 构造时 setFormat）
    static QSurfaceFormat surfaceFormat();

    /// GlHost 构造/析构注册（实例计数；共享根与程序池生命周期随计数）——内部，宿主调用
    static void registerHost();
    static void unregisterHost();
};

#endif // QCHARTGL_H
