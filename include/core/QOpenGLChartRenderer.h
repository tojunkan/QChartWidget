// QOpenGLChartRenderer.h —— GL 后端渲染器
#ifndef QOPENGLCHART_RENDERER_H
#define QOPENGLCHART_RENDERER_H

#include "QChartRenderer.h"
#include "QChartGL.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_3_Core>
#include <QVector>

class QOpenGLFunctions_4_3_Core;

/// VBO 顶点布局（16 字节对齐）
struct GLVertex {
    float x, y, z;          // Numeric 坐标（由 Shader 负责变换）
    uint8_t r, g, b, a;     // 颜色
};
static_assert(sizeof(GLVertex) == 16, "GLVertex must be 16 bytes");

/// 单个 VBO 批次
struct GLBatch {
    GLuint vao = 0;
    GLuint vbo = 0;
    /// 索引缓冲（仅"含 Grid 的 Triangle 批次"使用；其余批次走 glDrawArrays）
    GLuint ibo = 0;
    GLenum primitive = GL_POINTS;   // GL_POINTS / GL_LINES / GL_TRIANGLES
    GLsizei vertexCount = 0;
    GLsizei indexCount = 0;         // >0 ⇒ 用 glDrawElements(batch.primitive, indexCount, ...)
    int baseId = 0;
    ShaderKind shaderKind = ShaderKind::Point;
    float pointSize = 0.0f;         // 仅 Point 有效
    float lineWidth = 1.0f;         // 仅 Line 有效（图元 penWidth）
    /// 本批次是否含半透明顶点色（alpha < 255）。3D 下据此两件事：
    /// **不写深度**（否则挡住它后面的东西）+ **最后画**（否则混合顺序错）。
    /// 2D 不使用它：2D 的叠放完全由段序决定，两趟反而会打乱段序。
    bool translucent = false;
    // 深度：交给深度缓冲 + GL_LEQUAL + 段顺序（同深度后画的赢），**没有**偏置/分层开关。
};

class QOpenGLChartRenderer : public QChartRenderer
{
public:
    explicit QOpenGLChartRenderer() = default;
    ~QOpenGLChartRenderer() override;

    // ===== 基类虚函数实现 =====
    void transformNumericToCartesian(QChartScene& scene) override;
    void cullAndResolveLabels(QChartScene& scene) override;
    void drawPrimitives(QChartScene& scene,
                        QPaintDevice* device) override;
    void drawLabels(QChartScene& scene,
                    QPaintDevice* device) override;

    // renderer不负责管理OpenGL上下文的生命周期，外部保证在绘制时有有效的OpenGL上下文
    void clearBatches();

private:
    // ---- 批次构建 ----
    void buildBatches(const QChartScene& scene);
    /// 把一个段数组（轴段或系列段）打包成 GL 批次。
    /// ★ 轴段与系列段**分两次调用**，于是 m_batches 的顺序天然是"轴组在前、系列组在后"，
    ///   不需要给批次打"属于哪一侧"的标记字段。
    /// baseId 以引用传入：跨两次调用延续，避免两个段数组的批次 id 撞号。
    void uploadBatches(const QVector<QChartSegment>& segs, int& baseId);

    // ---- 绘制 ----
    /// 画一个批次（遍历顺序 = 打包顺序 = 段序：轴段在前、系列段在后）
    void drawBatch(const QChartScene& scene, const GLBatch& batch,
                   const QVector3D& numMin, const QVector3D& numMax);

    // ---- 辅助 ----
    QOpenGLFunctions_4_3_Core* glFuncs() const;

    QVector<GLBatch> m_batches;
};

#endif