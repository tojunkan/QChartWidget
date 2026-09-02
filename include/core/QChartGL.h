// QChartGL.h —— GL 资源池（共享上下文 + Shader 程序池）
// 职责：
//   1. 共享根上下文管理（QOffscreenSurface + QOpenGLContext，惰性创建，引用计数）
//   2. Shader 程序池：按 (上下文, ShaderKind, Projection类型) 缓存编译好的程序
//   3. 动态拼接 Shader：从 Projection 获取 glslToCartesian() 注入顶点着色器
#ifndef QCHARTGL_H
#define QCHARTGL_H

#include <QSurfaceFormat>
#include <QHash>
#include <QString>
#include <QPair>

class QOpenGLContext;
class QOpenGLShaderProgram;
class QChartAbstractProjection;

/// 程序池键（扩展版）：包含 Shader 种类 + Projection 类型标识
enum class ShaderKind {
    Line,      // GL_LINES / GL_LINE_STRIP
    Point,     // GL_POINTS（带点大小）
    Triangle,  // GL_TRIANGLES / GL_TRIANGLE_FAN / GL_TRIANGLE_STRIP
    Pick       // ID 帧拾取（复用 Line 顶点，输出 RGB24 ID）
};

class QChartGL {
public:
    // ===== 共享上下文（生命周期） =====
    /// 注册/注销一个 GL 宿主（QOpenGLWidget），内部维护引用计数
    /// 首实例注册时惰性创建共享根 QOffscreenSurface + QOpenGLContext
    static void registerHost();
    static void unregisterHost();

    /// 返回共享根上下文（无存活实例时返回 nullptr）
    static QOpenGLContext* sharedContext();

    /// 统一 SurfaceFormat：3.3 Core + depth 24
    static QSurfaceFormat surfaceFormat();

    // ===== Shader 程序池 =====
    /// 获取/编译一个 Shader 程序。
    /// - kind: 图元类型（Line/Point/Triangle/Pick）
    /// - projection: 用于提供 glslToCartesian() 变换代码（可空，空时使用恒等变换）
    /// - 编译结果按 (当前上下文, kind, projection 类型) 缓存，同类型复用
    static QOpenGLShaderProgram* program(ShaderKind kind,
                                         const QChartAbstractProjection* projection);

    /// 显式释放所有已编译程序（通常由 unregisterHost 在末实例时自动调用）
    static void releasePrograms();

private:
    // 禁止实例化
    QChartGL() = delete;
    ~QChartGL() = delete;
    QChartGL(const QChartGL&) = delete;
    QChartGL& operator=(const QChartGL&) = delete;
};

#endif // QCHARTGL_H