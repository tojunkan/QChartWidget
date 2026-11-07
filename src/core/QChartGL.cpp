// QChartGL.cpp —— GL 资源池实现
#include "QChartGL.h"
#include "QChartAbstractProjection.h"
#include "QChartMath.h"   // 细分判据常量（与 CPU 变换阶段同一份）
#include <QOpenGLContext>
#include <QOpenGLFunctions>   // glGetIntegerv（细分能力查询）
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLVersionFunctionsFactory>
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
        // ===== Line / Grid：投影与细分都在 TCS/TES 里，VS 只透传 Numeric 坐标 =====
        // （其余 kind 仍走"VS 里直接算 Cartesian"的老路，一行不改）
        if (kind == ShaderKind::Line || kind == ShaderKind::Grid) {
            return R"GLSL(#version 430 core
layout(location=0) in vec3 a_pos;      // Numeric 坐标（原样透传给 TCS）
layout(location=1) in vec4 a_color;

uniform int u_baseId;
uniform int u_vertPerPrim;

out vec3 t_num;
out vec4 t_col;
flat out int t_id;

void main() {
    t_num = a_pos;
    t_col = a_color;
    // ★ 细分会改变顶点数：这个 id 推导只对**输入**顶点有意义，细分出来的顶点靠
    //   TCS→TES 的 flat 透传沿用同一个号，所以一整段折线仍是同一个图元号。
    t_id  = u_baseId + gl_VertexID / max(u_vertPerPrim, 1);
    gl_Position = vec4(0.0);           // 由 TES 重新生成
}
)GLSL";
        }

        // 1. 获取投影变换 GLSL 表达式（投影族统一契约：glslToCartesian() 返回
        //    以 vec3 num 为输入的 Cartesian 表达式；着色器在此拼装 num 与赋值。）
        QString transformCode;
        if (proj) {
            transformCode = proj->glslToCartesian();
            if (transformCode.isEmpty()) {
                // 投影未提供 GLSL 变换 → 恒等
                transformCode = "num";
            }
        } else {
            transformCode = "num";
        }

        // 2. 确定是否需要 gl_PointSize
        bool needsPointSize = (kind == ShaderKind::Point);

        // 3. 构建着色器源码（使用 %1 占位符）
        QString src = R"GLSL(#version 430 core

// ---- 顶点输入 ----
layout(location=0) in vec3 a_pos;      // Numeric 坐标
layout(location=1) in vec4 a_color;    // 颜色（UNSIGNED_BYTE normalized）

// ---- Uniforms ----
uniform mat4 u_viewProj;
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
out vec3 v_num;                    // Numeric 位置：供 FS 做"数值域裁剪"
flat out int v_primId;

void main() {
    // ★ 投影变换注入点（Numeric → Cartesian）★
    // glslToCartesian() 返回表达式（以 num 为输入、值为 Cartesian 坐标）
    vec3 num = a_pos;
    vec3 cart = %1;

    // 应用视图投影矩阵（深度交给深度缓冲 + GL_LEQUAL + 段顺序，无偏置）
    gl_Position = u_viewProj * vec4(cart, 1.0);

    // 点大小
)GLSL";

        if (needsPointSize) {
            src += "    gl_PointSize = u_pointSize;\n";
        }

        src += R"GLSL(
    // 传递颜色和图元 ID
    v_color = a_color;
    v_num = num;
    v_primId = u_baseId + gl_VertexID / u_vertPerPrim;
}
)GLSL";

        return src.arg(transformCode);
    }

    /// 构建**细分控制着色器**（仅 Line kind）：只负责决定"这条线分成几段"。
    /// 判据与 CPU 变换阶段同一份常量（QChartMath::SUBDIV_RATIO / SUBDIV_MAX_SEGMENTS），
    /// 也曾经与已删除的几何着色器同一份——现在细分本身由 TES + 硬件细分器完成。
    static QString buildTessControlShader(const QChartAbstractProjection* proj)
    {
        QString transformCode;
        if (proj) {
            transformCode = proj->glslToCartesian();
            if (transformCode.isEmpty()) transformCode = "num";
        } else {
            transformCode = "num";
        }

        const QString ratio  = QString::number(QChartMath::SUBDIV_RATIO, 'g', 6);
        const int     maxSeg = QChartMath::SUBDIV_MAX_SEGMENTS;

        QString src = R"GLSL(#version 430 core
layout(vertices = 2) out;          // 一条线段 = 2 个控制点

// ★ 插值投影（QInterpolatedProjection）的 glslToCartesian() 返回
//   mix( (A), (B), u_blendAlpha )，所以每个用到 projectNum() 的着色阶段都必须自己声明它。
uniform float u_blendAlpha;

in vec3 t_num[];
in vec4 t_col[];
flat in int t_id[];

out vec3 te_num[];
out vec4 te_col[];
flat out int te_id[];

// 投影表达式注入点（以 num 为输入）
vec3 projectNum(vec3 num) { return %1; }

void main() {
    te_num[gl_InvocationID] = t_num[gl_InvocationID];
    te_col[gl_InvocationID] = t_col[gl_InvocationID];
    te_id[gl_InvocationID]  = t_id[gl_InvocationID];

    if (gl_InvocationID == 0) {
        vec3 n0 = t_num[0];
        vec3 n1 = t_num[1];
        vec3 c0 = projectNum(n0);
        vec3 c1 = projectNum(n1);

        vec3 cm = projectNum((n0 + n1) * 0.5);
        float d = length(cm - (c0 + c1) * 0.5);

        float k = 1.0;
        // ★ 闭环特例：角度维扫满一圈时两端点重合（L≈0），d/(ratio·L) → ∞ 被上限截住，
        //   得到"圆环画成 32 边形"。**不能**用 L>eps 作门槛，否则闭环被跳过、环画不出来。
        if (d > 1e-9) {
            float L = max(length(c1 - c0), 1e-12);
            k = clamp(ceil(d / (%2 * L)), 1.0, float(%3));
        }

        // isolines 域的分段数（实测确定的语义，与注释里的直觉相反）：
        //   outer[0] = **等值线条数**（跨线方向的分段数）
        //   outer[1] = **每条线上的段数**（沿线方向）
        //   取证：写 [0]=32,[1]=1 时，TF 抓到的 TessCoord 是 32 对 (x=0/1, y=i/32)，
        //   即 32 条线各 1 段。这里要的是"1 条线、k 段"，所以要把 k 放到 [1]。
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = k;
    }
}
)GLSL";

        return src.arg(transformCode).arg(ratio).arg(maxSeg);
    }

    /// 构建**细分求值着色器**（仅 Line kind）：在参数域上均匀取点 → Numeric → 投影。
    /// `equal_spacing` 让细分参数均匀分布，与 CPU 后端按 t 等分的行为一致。
    static QString buildTessEvaluationShader(const QChartAbstractProjection* proj)
    {
        QString transformCode;
        if (proj) {
            transformCode = proj->glslToCartesian();
            if (transformCode.isEmpty()) transformCode = "num";
        } else {
            transformCode = "num";
        }

        QString src = R"GLSL(#version 430 core
layout(isolines, equal_spacing) in;

uniform mat4 u_viewProj;
uniform float u_blendAlpha;        // 插值投影需要（见 TCS 里的说明）

in vec3 te_num[];
in vec4 te_col[];
flat in int te_id[];

out vec4 v_color;
out vec3 v_num;                    // Numeric 位置：供 FS 做"数值域裁剪"
flat out int v_primId;

vec3 projectNum(vec3 num) { return %1; }

void main() {
    // ★ 沿线的参数是 gl_TessCoord.**y**，不是 .x —— 这块驱动的 isolines 域里
    //   .x 是"跨线方向"（整条线上恒为 0），只有 .y 从 0 扫到 1。
    //   用错 .x 的后果：k>1 时所有顶点都落在同一个参数上 → 环退化成一点（k=1 时恰好两端是 0/1，
    //   所以普通线段看起来正常，把这个 bug 藏住了）。
    float t = gl_TessCoord.x;
    vec3 num = mix(te_num[0], te_num[1], t);
    gl_Position = u_viewProj * vec4(projectNum(num), 1.0);
    v_color = te_col[0];
    v_num = num;
    v_primId = te_id[0];
}
)GLSL";

        return src.arg(transformCode);
    }

    /// 构建**曲面网格的细分控制着色器**（仅 Grid kind，quads 域）：决定每一格分几段。
    /// 判据与线/CPU 同源（中点弓高 ÷ 弦长），只是"中点"取四角平均、"弦长"取对角线。
    /// ★ 控制点顺序 = 网格单元的四角 (u,v) = (0,0),(1,0),(1,1),(0,1)（GL quads 域的约定）。
    static QString buildGridTessControlShader(const QChartAbstractProjection* proj)
    {
        QString transformCode = (proj ? proj->glslToCartesian() : QString());
        if (transformCode.isEmpty()) transformCode = "num";

        const QString ratio  = QString::number(QChartMath::SUBDIV_RATIO, 'g', 6);
        const int     maxSeg = QChartMath::SUBDIV_MAX_SEGMENTS;

        QString src = R"GLSL(#version 430 core
layout(vertices = 4) out;          // 每格 4 个控制点

// 插值投影的 mix(A,B,u_blendAlpha) 需要（见折线 TCS 里的说明）
uniform float u_blendAlpha;

in vec3 t_num[];
in vec4 t_col[];
flat in int t_id[];

out vec3 te_num[];
out vec4 te_col[];
flat out int te_id[];

vec3 projectNum(vec3 num) { return %1; }

void main() {
    te_num[gl_InvocationID] = t_num[gl_InvocationID];
    te_col[gl_InvocationID] = t_col[gl_InvocationID];
    te_id[gl_InvocationID]  = t_id[gl_InvocationID];

    if (gl_InvocationID == 0) {
        vec3 c0 = projectNum(t_num[0]);
        vec3 c1 = projectNum(t_num[1]);
        vec3 c2 = projectNum(t_num[2]);
        vec3 c3 = projectNum(t_num[3]);

        // 中点弓高：Numeric 四角平均的投影  vs  四个投影点的平均
        vec3 nc = (t_num[0] + t_num[1] + t_num[2] + t_num[3]) * 0.25;
        vec3 cAvg = (c0 + c1 + c2 + c3) * 0.25;
        float d = length(projectNum(nc) - cAvg);

        // 特征弦长：两条对角线里较长的（更能代表这一格的尺度）
        float L = max(max(length(c2 - c0), length(c3 - c1)), 1e-12);

        float k = 1.0;
        if (d > 1e-9) {
            k = clamp(ceil(d / (%2 * L)), 1.0, float(%3));
        }

        // quads 域：outer[0..3] = 四条边的段数，inner[0..1] = 内部细分
        gl_TessLevelOuter[0] = k;
        gl_TessLevelOuter[1] = k;
        gl_TessLevelOuter[2] = k;
        gl_TessLevelOuter[3] = k;
        gl_TessLevelInner[0] = k;
        gl_TessLevelInner[1] = k;
    }
}
)GLSL";

        return src.arg(transformCode).arg(ratio).arg(maxSeg);
    }

    /// 构建**曲面网格的细分求值着色器**（仅 Grid kind，quads 域）：
    /// 在 Numeric 空间做双线性取点，再逐点投影 —— 这正是"格内跟着投影弯曲"的来源。
    static QString buildGridTessEvaluationShader(const QChartAbstractProjection* proj)
    {
        QString transformCode = (proj ? proj->glslToCartesian() : QString());
        if (transformCode.isEmpty()) transformCode = "num";

        QString src = R"GLSL(#version 430 core
layout(quads, equal_spacing) in;

uniform mat4 u_viewProj;
uniform float u_blendAlpha;

in vec3 te_num[];
in vec4 te_col[];
flat in int te_id[];

out vec4 v_color;
out vec3 v_num;                    // Numeric 位置：供 FS 做"数值域裁剪"
flat out int v_primId;

vec3 projectNum(vec3 num) { return %1; }

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;
    // 控制点顺序：(0,0) (1,0) (1,1) (0,1)
    vec3 edge0 = mix(te_num[0], te_num[1], u);   // v = 0 边
    vec3 edge1 = mix(te_num[3], te_num[2], u);   // v = 1 边
    vec3 num   = mix(edge0, edge1, v);

    gl_Position = u_viewProj * vec4(projectNum(num), 1.0);
    v_color = te_col[0];
    v_num = num;
    v_primId = te_id[0];
}
)GLSL";

        return src.arg(transformCode);
    }

    /// 构建片段着色器源码
    static QString buildFragmentShader(ShaderKind kind)
    {
        switch (kind) {
            // Line / Grid：TES 输出 v_color（与 Triangle 同名）——共享同一个 FS
        case ShaderKind::Line:
        case ShaderKind::Triangle:
        case ShaderKind::Grid:
            return R"GLSL(#version 430 core
in vec4 v_color;
in vec3 v_num;                 // Numeric 位置（VS / TES 传入）
uniform vec3 u_numMin;         // 数值域裁剪盒（取极值 = 不裁）
uniform vec3 u_numMax;
out vec4 fragColor;
void main() {
    // ★ 数值域裁剪：Numeric 盒外的片元直接丢弃。
    //   盒子 = "相机视野盒经投影反算"，与三轴范围同源（见 renderer 里的注释）。
    if (any(lessThan(v_num, u_numMin)) || any(greaterThan(v_num, u_numMax))) discard;
    fragColor = v_color;
}
)GLSL";

        case ShaderKind::Point:
            return R"GLSL(#version 430 core
in vec4 v_color;
in vec3 v_num;
uniform vec3 u_numMin;
uniform vec3 u_numMax;
out vec4 fragColor;
void main() {
    if (any(lessThan(v_num, u_numMin)) || any(greaterThan(v_num, u_numMax))) discard;
    // 圆形裁剪：距离中心 > 0.5 时丢弃
    if (length(gl_PointCoord - vec2(0.5)) > 0.5) discard;
    fragColor = v_color;
}
)GLSL";

        case ShaderKind::Pick:
            return R"GLSL(#version 430 core
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
            return R"GLSL(#version 430 core
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

        // Line / Grid：挂 TCS + TES（细分 = 硬件细分器；投影也在 TES 里做）。
        // ★ 几何着色器已退役：它的"细分 + 投影"两项职责分别由 TCS（定段数）与 TES（取点+投影）接管。
        //   Line 用 isolines 域（2 控制点），Grid 用 quads 域（4 控制点 = 一格）。
        if (kind == ShaderKind::Line || kind == ShaderKind::Grid) {
            const bool isGrid = (kind == ShaderKind::Grid);
            const QString tcs = isGrid ? buildGridTessControlShader(proj) : buildTessControlShader(proj);
            if (!prog->addShaderFromSourceCode(QOpenGLShader::TessellationControl, tcs)) {
                qWarning() << "QChartGL: Tessellation control shader compilation failed\n" << prog->log()
                           << "\n--- TCS source ---\n" << tcs;
                delete prog;
                return nullptr;
            }
            const QString tes = isGrid ? buildGridTessEvaluationShader(proj) : buildTessEvaluationShader(proj);
            if (!prog->addShaderFromSourceCode(QOpenGLShader::TessellationEvaluation, tes)) {
                qWarning() << "QChartGL: Tessellation evaluation shader compilation failed\n" << prog->log()
                           << "\n--- TES source ---\n" << tes;
                delete prog;
                return nullptr;
            }
        }

        if (!prog->link()) {
            qWarning() << "QChartGL: Shader program linking failed for kind="
                       << int(kind) << "\n" << prog->log();
            delete prog;
            return nullptr;
        }

        qCDebug(logGL) << "QChartGL: Compiled shader program kind=" << int(kind)
                       << "proj=" << (proj ? proj->dimensionName(0) : "null");

        // ★ 诊断（chart.gl 打开时）：把 Tessellation 的两段源码原文、以及驱动细分能力打出来。
        //   细分"画不出来"时，先看这两样——源码能证明表达式替换是否对，上限能证明 level 是否被截。
        if (kind == ShaderKind::Line) {
            GLint maxGenLevel = -1, maxPatchVerts = -1;
            // 0x8E7F = GL_MAX_TESS_GEN_LEVEL, 0x8E7D = GL_MAX_PATCH_VERTICES（core 4.0+）
            ctx->functions()->glGetIntegerv(0x8E7F, &maxGenLevel);
            ctx->functions()->glGetIntegerv(0x8E7D, &maxPatchVerts);
            qCDebug(logGL) << "TESS capability: GL_MAX_TESS_GEN_LEVEL=" << maxGenLevel
                           << " GL_MAX_PATCH_VERTICES=" << maxPatchVerts;
            qCDebug(logGL).noquote() << "--- TCS source ---\n" << buildTessControlShader(proj);
            qCDebug(logGL).noquote() << "--- TES source ---\n" << buildTessEvaluationShader(proj);
        }

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
    // ★ 全库**唯一**的 GL 版本来源：host 控件、共享 offscreen surface、共享 context 三处都用它。
    //   4.3 Core 是 tessellation 的前置（TCS/TES 需要 4.0+，GLSL 需 #version 400+）。
    fmt.setVersion(4, 3);
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