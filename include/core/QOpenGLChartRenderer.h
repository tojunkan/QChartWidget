// QOpenGLChartRenderer.h —— GL 后端渲染器
#ifndef QOPENGLCHART_RENDERER_H
#define QOPENGLCHART_RENDERER_H

#include "QChartRenderer.h"
#include "QChartGL.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QVector>

class QOpenGLFunctions_3_3_Core;

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
    GLenum primitive = GL_POINTS;   // GL_POINTS / GL_LINES / GL_TRIANGLES
    GLsizei vertexCount = 0;
    int baseId = 0;
    ShaderKind shaderKind = ShaderKind::Point;
    float pointSize = 0.0f;         // 仅 Point 有效
    bool depthTest = true;
    float depthBias = 0.0f;
};

class QOpenGLChartRenderer : public QChartRenderer
{
public:
    explicit QOpenGLChartRenderer(QOpenGLWidget* host);
    ~QOpenGLChartRenderer() override;

    // ===== 基类虚函数实现 =====
    void transformNumericToCartesian(QChartScene& scene) override;
    void cullAndResolveLabels(QChartScene& scene) override;
    void drawPrimitives(QChartScene& scene,
                        QPaintDevice* device,
                        const QVector<bool>& visibility) override;
    void drawLabels(QChartScene& scene,
                    QPaintDevice* device) override;

    // ===== GL 生命周期（由 GlHost 调用） =====
    void initializeGL();
    void paintGL(const QChartScene& scene);
    void resizeGL(int w, int h);
    bool isReady() const { return m_glReady; }

private:
    // ---- 批次构建 ----
    void buildBatches(const QChartScene& scene);
    void uploadBatches(const QChartScene& scene);

    // ---- 绘制 ----
    void drawPass(const QChartScene& scene, ShaderKind kind);

    // ---- 辅助 ----
    QOpenGLFunctions_3_3_Core* glFuncs() const;
    void clearBatches();

    QOpenGLWidget* m_host = nullptr;
    QVector<GLBatch> m_batches;
    bool m_glReady = false;
    bool m_initAttempted = false;
    QSize m_viewportSize;
};

#endif