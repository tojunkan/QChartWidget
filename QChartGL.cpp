// QChartGL.cpp —— GL 资源池实现（design_phase3.md §7.3，A2/A3）
// 共享根 = QOffscreenSurface + QOpenGLContext（3.3 Core + depth24）：registerHost 首实例惰性创建，
// 末实例 unregisterHost 释放（程序池引用计数归零）。无 GL 环境（offscreen 等）创建失败 →
// 共享根为 nullptr（QPainter 路径共存兜底，A9），不崩溃。
// 程序池（§4/§7.3）：GLSL 330 三程序（line/point/pick），首次 program(kind) 于调用方当前上下文
// 惰性编译（t44；pick 供 t46 ID 帧）。⚠ 上下文一致性（t43 O2）：Qt 6.4.x 无 setShareContext 共享 →
// 程序在编译它的上下文内有效；单 widget demo（单一活跃上下文）成立，多实例共享需 Qt ≥6.5。
#include "QChartGL.h"
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QHash>
#include <QDebug>

namespace {
    int s_instanceCount = 0;                     // 存活宿主实例数（引用计数）
    QOpenGLContext* s_sharedContext = nullptr;   // 共享根（惰性；无实例 = nullptr）
    QOffscreenSurface* s_shareSurface = nullptr; // 共享根挂载 surface
    // 程序池（§7.3）：上下文 → kind → 程序。⚠ 上下文一致性（t43 O2）：Qt 6.4.x 无 setShareContext，
    // 各 QOpenGLWidget 上下文独立 → 程序必须按编译它的上下文建池，跨上下文复用会产生无效程序
    // （uniformLocation "not linked"）；Qt ≥6.5 共享后同一程序多上下文可用（池按上下文冗余，正确性优先）
    QHash<QOpenGLContext*, QHash<int, QOpenGLShaderProgram*>> s_programs;

    // ===== GLSL 330 字符串（design_phase3.md §4；运行期编译，A1/A5）=====
    // 顶点核心（line/point 共用逻辑；A5：仅 u_viewProj——toWorld 已在 CPU 缓存完成，World 直进 attribute）
    const char* kVertexCore = R"GLSL(#version 330 core
layout(location=0) in vec3 a_pos;      // World
layout(location=1) in vec4 a_color;    // UNSIGNED_BYTE normalized
uniform mat4 u_viewProj;
uniform float u_depthBias;             // Grid 批次 > 0（NDC z 偏置，等价 kGridDepthBias 语义）
uniform int  u_baseId;
uniform int  u_vertPerPrim;
out vec4 v_color;
flat out int v_primId;
void main() {
    vec4 clip = u_viewProj * vec4(a_pos, 1.0);
    clip.z += u_depthBias * clip.w;    // 网格偏置：同深度处"更远"，系列优先（与 QPainter 路径一致）
    gl_Position = clip;
    v_color = a_color;
    v_primId = u_baseId + gl_VertexID / u_vertPerPrim;
}
)GLSL";

    const char* kLineFrag = R"GLSL(#version 330 core
in vec4 v_color;
out vec4 fragColor;
void main() { fragColor = v_color; }
)GLSL";

    // 点顶点：核心 + 点尺寸（GL_PROGRAM_POINT_SIZE 需 glEnable，渲染器 initializeGL 处理）
    const char* kPointVert = R"GLSL(#version 330 core
layout(location=0) in vec3 a_pos;
layout(location=1) in vec4 a_color;
uniform mat4 u_viewProj;
uniform float u_depthBias;
uniform int  u_baseId;
uniform int  u_vertPerPrim;
uniform float u_pointSize;
out vec4 v_color;
flat out int v_primId;
void main() {
    vec4 clip = u_viewProj * vec4(a_pos, 1.0);
    clip.z += u_depthBias * clip.w;
    gl_Position = clip;
    gl_PointSize = u_pointSize;
    v_color = a_color;
    v_primId = u_baseId + gl_VertexID / u_vertPerPrim;
}
)GLSL";

    // 点片段：圆形化（边缘 discard）
    const char* kPointFrag = R"GLSL(#version 330 core
in vec4 v_color;
out vec4 fragColor;
void main() {
    if (length(gl_PointCoord - vec2(0.5)) > 0.5) discard;
    fragColor = v_color;
}
)GLSL";

    // 拾取顶点（t46 ID 帧；顶点数据复用主 pass 批次，仅用 a_pos）。
    // u_sentinel：轴/网格/Decor 批次（dataIndex=-1，§5.3 定案）输出 0xFFFFFF 哨兵——不参与拾取，
    //   但仍渲染以维持深度语义（ID 帧与主 pass 同批次顺序/深度，遮挡正确）
    const char* kPickVert = R"GLSL(#version 330 core
layout(location=0) in vec3 a_pos;
uniform mat4 u_viewProj;
uniform float u_depthBias;
uniform int  u_baseId;
uniform int  u_vertPerPrim;
uniform int  u_sentinel;
flat out int v_primId;
void main() {
    vec4 clip = u_viewProj * vec4(a_pos, 1.0);
    clip.z += u_depthBias * clip.w;
    gl_Position = clip;
    v_primId = (u_sentinel != 0) ? 0xFFFFFF : u_baseId + gl_VertexID / u_vertPerPrim;
}
)GLSL";

    // 拾取片段：图元 ID 编码 RGB24（§5.3；id=0xFFFFFF 哨兵 = 未命中，A6）
    const char* kPickFrag = R"GLSL(#version 330 core
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

    QOpenGLShaderProgram* compileProgram(ShaderKind kind) {
        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        if (!ctx) return nullptr;   // 编译需 current context（t43 O2：调用方 ensure current 后使用）
        const char* vs = nullptr;
        const char* fs = nullptr;
        switch (kind) {
        case ShaderKind::Line:  vs = kVertexCore; fs = kLineFrag;  break;
        case ShaderKind::Point: vs = kPointVert; fs = kPointFrag;  break;
        case ShaderKind::Pick:  vs = kPickVert;  fs = kPickFrag;   break;
        }
        auto* prog = new QOpenGLShaderProgram();
        if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vs) ||
            !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fs) ||
            !prog->link()) {
            qWarning() << "QChartGL::program: shader 编译/链接失败 kind=" << int(kind)
                       << prog->log();
            delete prog;
            return nullptr;
        }
        return prog;
    }
}

QSurfaceFormat QChartGL::surfaceFormat() {
    // §7.3：3.3 Core、depth 24（默认 FBO 深度缓冲按此分配，A3）
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(0);   // 不申请 MSAA（A3 最小内存；线框渲染无需抗锯齿）
    return fmt;
}

void QChartGL::registerHost() {
    ++s_instanceCount;
    if (s_sharedContext) return;   // 共享根已建（首实例或其后）
    // 惰性创建共享根（A3）：QOffscreenSurface + QOpenGLContext
    s_shareSurface = new QOffscreenSurface();
    s_shareSurface->setFormat(surfaceFormat());
    s_shareSurface->create();
    s_sharedContext = new QOpenGLContext();
    s_sharedContext->setFormat(surfaceFormat());
    if (!s_sharedContext->create() || !s_shareSurface->isValid()) {
        static bool s_warned = false;   // 一次性告警（offscreen/无 GL 环境不刷屏）
        if (!s_warned) {
            qWarning() << "QChartGL::registerHost: 共享上下文创建失败（无 GL 环境？QPainter 路径共存兜底，A9）";
            s_warned = true;
        }
        delete s_sharedContext; s_sharedContext = nullptr;
        delete s_shareSurface;  s_shareSurface = nullptr;
        return;
    }
    // 不做 makeCurrent：上下文电流归属宿主 paintGL；程序编译于宿主上下文（调用方 ensure current）
}

void QChartGL::unregisterHost() {
    if (s_instanceCount <= 0) return;
    --s_instanceCount;
    if (s_instanceCount > 0) return;   // 仍有存活实例 → 保留共享根/程序池
    releasePrograms();                 // 最后实例析构 → 程序池释放（§7.3 引用计数）
    delete s_sharedContext; s_sharedContext = nullptr;
    delete s_shareSurface;  s_shareSurface = nullptr;
}

QOpenGLContext* QChartGL::sharedContext() {
    // 无存活实例 → nullptr（§7.3「无 GL 部件实例时返回 nullptr，零资源」）
    return s_instanceCount > 0 ? s_sharedContext : nullptr;
}

QOpenGLShaderProgram* QChartGL::program(ShaderKind kind) {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) return nullptr;
    // 按当前上下文取/建（t43 O2：程序在编译它的上下文内有效；6.4.x 无共享 → 每上下文一份）
    QHash<int, QOpenGLShaderProgram*>& byKind = s_programs[ctx];
    const auto it = byKind.constFind(int(kind));
    if (it != byKind.constEnd()) return it.value();
    QOpenGLShaderProgram* prog = compileProgram(kind);
    if (prog) byKind.insert(int(kind), prog);
    return prog;
}

void QChartGL::releasePrograms() {
    // 程序随其上下文释放（QOpenGLShaderProgram 析构在上下文销毁后是安全的——GL 资源已随上下文回收）
    for (auto ctxIt = s_programs.begin(); ctxIt != s_programs.end(); ++ctxIt)
        qDeleteAll(ctxIt.value());
    s_programs.clear();
}
