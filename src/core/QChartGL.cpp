// QChartGL.cpp —— GL 资源池实现
#include "QChartGL.h"
#include "QChartAbstractProjection.h"
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logGL, "chart.gl")

namespace {

    
    // 共享根状态（惰性创建，引用计数）
    
    int s_instanceCount = 0;
    QOpenGLContext* s_sharedContext = nullptr;
    QOffscreenSurface* s_shareSurface = nullptr;

    // 程序池：context → (kind, projectionHash) → program
    using ShaderKey = QPair<int, quint64>;  // <ShaderKind, projectionTypeHash>
    QHash<QOpenGLContext*, QHash<ShaderKey, QOpenGLShaderProgram*>> s_programs;

    
    // Shader 构建函数
    

    /// 构建顶点着色器源码
    static QString buildVertexShader(ShaderKind kind,
                                     const QChartAbstractProjection* proj)
    {
        // 1. 获取投影变换 GLSL 代码
        QString transformCode;
        if (proj) {
            transformCode = proj->glslToCartesian();
            if (transformCode.isEmpty()) {
                // 如果投影没有提供 GLSL 变换，使用恒等
                transformCode = "vec3 cart = a_pos;";
            }
        } else {
            transformCode = "vec3 cart = a_pos;";
        }

        // 2. 确定是否需要 gl_PointSize
        bool needsPointSize = (kind == ShaderKind::Point);

        // 3. 构建着色器源码（使用 %1 占位符）
        QString src = R"GLSL(#version 330 core

// ---- 顶点输入 ----
layout(location=0) in vec3 a_pos;      // Numeric 坐标
layout(location=1) in vec4 a_color;    // 颜色（UNSIGNED_BYTE normalized）

// ---- Uniforms ----
uniform mat4 u_viewProj;
uniform float u_depthBias;             // 网格深度偏置（Grid > 0）
uniform int  u_baseId;                 // 图元 ID 基址
uniform int  u_vertPerPrim;            // 每个图元的顶点数（1/2/3）
uniform float u_blendAlpha;            // 用于插值投影
)GLSL";

        if (needsPointSize) {
            src += "uniform float u_pointSize;\n";
        }

        src += R"GLSL(
// ---- 输出 ----
out vec4 v_color;
flat out int v_primId;

void main() {
    // ★ 投影变换注入点（Numeric → Cartesian）★
    // 期望输出：vec3 cart
    %1

    // 应用视图投影矩阵
    vec4 clip = u_viewProj * vec4(cart, 1.0);
    clip.z += u_depthBias * clip.w;
    gl_Position = clip;

    // 点大小
)GLSL";

        if (needsPointSize) {
            src += "    gl_PointSize = u_pointSize;\n";
        }

        src += R"GLSL(
    // 传递颜色和图元 ID
    v_color = a_color;
    v_primId = u_baseId + gl_VertexID / u_vertPerPrim;
}
)GLSL";

        return src.arg(transformCode);
    }

    /// 构建片段着色器源码
    static QString buildFragmentShader(ShaderKind kind)
    {
        switch (kind) {
        case ShaderKind::Line:
        case ShaderKind::Triangle:
            return R"GLSL(#version 330 core
in vec4 v_color;
out vec4 fragColor;
void main() {
    fragColor = v_color;
}
)GLSL";

        case ShaderKind::Point:
            return R"GLSL(#version 330 core
in vec4 v_color;
out vec4 fragColor;
void main() {
    // 圆形裁剪：距离中心 > 0.5 时丢弃
    if (length(gl_PointCoord - vec2(0.5)) > 0.5) discard;
    fragColor = v_color;
}
)GLSL";

        case ShaderKind::Pick:
            return R"GLSL(#version 330 core
flat in int v_primId;
out vec4 fragColor;
void main() {
    int id = v_primId;
    fragColor = vec4(float(id & 255) / 255.0,
                     float((id >> 8) & 255) / 255.0,
                     float((id >> 16) & 255) / 255.0,
                     1.0);
}
)GLSL";

        default:
            return R"GLSL(#version 330 core
out vec4 fragColor;
void main() { fragColor = vec4(1,0,0,1); }
)GLSL";
        }
    }

    /// 编译单个 Shader 程序
    static QOpenGLShaderProgram* compileProgram(ShaderKind kind,
                                                const QChartAbstractProjection* proj)
    {
        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        if (!ctx) {
            qWarning() << "QChartGL::compileProgram: no current OpenGL context";
            return nullptr;
        }

        QString vs = buildVertexShader(kind, proj);
        QString fs = buildFragmentShader(kind);

        auto* prog = new QOpenGLShaderProgram();

        if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vs)) {
            qWarning() << "QChartGL: Vertex shader compilation failed for kind="
                       << int(kind) << "\n" << prog->log();
            delete prog;
            return nullptr;
        }

        if (!prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)) {
            qWarning() << "QChartGL: Fragment shader compilation failed for kind="
                       << int(kind) << "\n" << prog->log();
            delete prog;
            return nullptr;
        }

        if (!prog->link()) {
            qWarning() << "QChartGL: Shader program linking failed for kind="
                       << int(kind) << "\n" << prog->log();
            delete prog;
            return nullptr;
        }

        qCDebug(logGL) << "QChartGL: Compiled shader program kind=" << int(kind)
                       << "proj=" << (proj ? proj->dimensionName(0) : "null");

        return prog;
    }

    /// 计算 Projection 的类型哈希（用于缓存 key）
    static quint64 projectionTypeHash(const QChartAbstractProjection* proj)
    {
        if (!proj) return 0;
        // 用 dimension() + 类型名的组合作为标识
        // 使用 typeid 的 hash_code（C++11 起可用）
        return qHash(typeid(*proj).name()) ^ (quint64(proj->dimension()) << 32);
    }

} // namespace



// 公共接口实现


QSurfaceFormat QChartGL::surfaceFormat()
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(0);
    return fmt;
}

void QChartGL::registerHost()
{
    ++s_instanceCount;
    if (s_sharedContext) return;

    s_shareSurface = new QOffscreenSurface();
    s_shareSurface->setFormat(surfaceFormat());
    s_shareSurface->create();

    s_sharedContext = new QOpenGLContext();
    s_sharedContext->setFormat(surfaceFormat());

    if (!s_sharedContext->create() || !s_shareSurface->isValid()) {
        static bool s_warned = false;
        if (!s_warned) {
            qWarning() << "QChartGL::registerHost: 共享上下文创建失败（无 GL 环境？QPainter 路径共存兜底）";
            s_warned = true;
        }
        delete s_sharedContext;
        s_sharedContext = nullptr;
        delete s_shareSurface;
        s_shareSurface = nullptr;
        return;
    }
}

void QChartGL::unregisterHost()
{
    if (s_instanceCount <= 0) return;
    --s_instanceCount;
    if (s_instanceCount > 0) return;

    releasePrograms();
    delete s_sharedContext;
    s_sharedContext = nullptr;
    delete s_shareSurface;
    s_shareSurface = nullptr;
}

QOpenGLContext* QChartGL::sharedContext()
{
    return s_instanceCount > 0 ? s_sharedContext : nullptr;
}

QOpenGLShaderProgram* QChartGL::program(ShaderKind kind,
                                        const QChartAbstractProjection* projection)
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qWarning() << "QChartGL::program: 无当前 OpenGL 上下文";
        return nullptr;
    }

    // 构建缓存 key
    ShaderKey key(int(kind), projectionTypeHash(projection));

    // 查找已有程序
    QHash<ShaderKey, QOpenGLShaderProgram*>& ctxPrograms = s_programs[ctx];
    auto it = ctxPrograms.constFind(key);
    if (it != ctxPrograms.constEnd()) {
        return it.value();
    }

    // 编译新程序
    QOpenGLShaderProgram* prog = compileProgram(kind, projection);
    if (prog) {
        ctxPrograms.insert(key, prog);
    }
    return prog;
}

void QChartGL::releasePrograms()
{
    for (auto ctxIt = s_programs.begin(); ctxIt != s_programs.end(); ++ctxIt) {
        qDeleteAll(ctxIt.value());
    }
    s_programs.clear();
}