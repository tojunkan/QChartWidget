// QOpenGLChartRenderer.h —— GL 后端渲染器（design_phase3.md §2.1；: QChartRenderer 与 QPainter 后端并列）
// ⚠ 职责边界（A4）：GL 只做 3D 屏显；导出（PNG/SVG/PDF）与 2D 路径一律走 QPainterChartRenderer
//   （renderUncached），本类不参与（renderUncached → qWarning 拒绝）。
// ⚠ 生命周期（A3 惰性）：宿主 QOpenGLWidget 回调驱动——initializeGL（首帧：上下文可用性检查 +
//   格式核对；shader 编译属实现③ t44）、paintGL（buildBatches + 空绘制占位）、resizeGL（视口）。
// ⚠ 非 Q_OBJECT（无信号/槽；moc 约定：新 Q_OBJECT 类只进库 AUTOMOC，本类不进）。
#ifndef QOPENGLCHART_RENDERER_H
#define QOPENGLCHART_RENDERER_H

#include "QChartRenderer.h"
#include "QChartHitTester.h"   // PickRecord（§8.1）
#include "QChartGL.h"          // ShaderKind（程序池键，§4）
#include <QOpenGLWidget>       // 宿主类型（qobject_cast 需要完整类型）
#include <QElapsedTimer>
#include <QSize>
#include <QVector>

class QChartLayer3D;
class QChartSeries3D;
class QOpenGLFunctions_3_3_Core;   // 3.3 Core 上下文函数集（VAO/VBO/attribute，§7.3）

// ===== VBO 批次结构与图元→顶点映射（design_phase3.md §3；t42 骨架）=====

/// 顶点布局（§3.2，interleaved 16B/顶点；A5：World float3 + 颜色）：
/// attribute0 = vec3 position（World，float）；attribute1 = vec4 color（UNSIGNED_BYTE normalized）
struct GLVertex {
    float x, y, z;          // World 坐标（toWorld 已在 CPU 缓存完成，§4：仅 u_viewProj 变换）
    uint8_t r, g, b, a;     // 颜色（收集时已按系列主题/override 展开）
};
static_assert(sizeof(GLVertex) == 16, "A5: GLVertex 必须 16B（World float3 12B + 颜色 4B）");

/// 批次（§3.1）：每批次一种 Layer + 一种 primitive
struct GLBatch {
    GLuint vao = 0;
    GLuint vbo = 0;                 // interleaved 顶点（§3.2）
    GLenum primitive = GL_POINTS;   // GL_POINTS / GL_LINES
    GLsizei vertexCount = 0;
    int baseId = 0;                 // 图元 ID 基址（id = baseId + gl_VertexID/vertPerPrim，§5.3；零内存）
    QChartPrimitive::Layer layer = QChartPrimitive::Layer::Series;   // 深度语义（§5.2）
    bool depthTest = true;          // ForegroundDecor = false（A4 恒可见）
    qreal depthBias = 0.0;          // Grid 批次 > 0（u_depthBias uniform，等价 kGridDepthBias 语义）
    GLuint ebo = 0;                 // 可选索引缓冲（曲面线框顶点去重，Phase 3 优化项）
    float pointSize = 0.0f;         // Point 批次 uniform u_pointSize（§3.1：同批次点尺寸一致；
                                    //   Line 批次 0——t44 合并键含 markerSize 保证）
    bool idSentinel = false;        // ID 帧（t46，§5.3 定案）：非 Series 批次（轴/网格/Decor，
                                    //   dataIndex=-1）输出 0xFFFFFF 哨兵——不参与拾取但仍渲染以维持深度
};

/// 批次池（数据变化重建，A5）
struct BatchPool {
    QVector<GLBatch> batches;
    QVector<QChartHitTester::PickRecord> pickTable;   // 图元 ID → 命中结果（§8.1）
    bool dirty = true;              // invalidateForeground/Background 置位
};

class QOpenGLChartRenderer : public QChartRenderer {
public:
    explicit QOpenGLChartRenderer(QWidget* host = nullptr);   // 宿主 = 内嵌 QOpenGLWidget（A2）
    ~QOpenGLChartRenderer() override;

    // ===== QChartRenderer 接口（与 QPainter 后端并列）=====
    /// ⚠ device 必须是宿主 QOpenGLWidget（GL 渲染无通用 QPaintDevice 目标）；否则 qWarning 返回
    void render(const QChartScene& scene, QPaintDevice* device) override;
    /// ⚠ GL 不参与导出：qWarning 提示改用 QPainterChartRenderer（A4）
    void renderUncached(const QChartScene& scene, QPaintDevice* device) override;
    void invalidateBackground() override;    // 场景/数据脏标记（触发 VBO 重建，视图变化不触发）
    void invalidateForeground() override;    // 同（GL 无 QPixmap 缓存；数据/样式变化 → VBO 重建）
    void setCachingEnabled(bool) override;   // GL 无 QPixmap 缓存：no-op（保留接口一致）
    bool isCachingEnabled() const override;  // 恒 true（GL 无缓存语义）

    // ===== GL 生命周期（宿主 QOpenGLWidget 回调驱动；A3 惰性）=====
    /// 首帧/首 show：上下文可用性检查 + 格式核对 + 编译 3 shader 程序（§4）
    void initializeGL();
    /// 主渲染入口（宿主 paintGL 调用；scene 由 QChartWidget3D::buildScreenScene 提供）
    void paintGL(const QChartScene& scene);
    /// 视口随尺寸（宿主 resizeGL 调用；深度缓冲由 QOpenGLWidget 默认 FBO 自带，按尺寸重建）
    void resizeGL(int w, int h);
    /// GL 就绪（上下文可用 + initializeGL 成功）——GlHost 显隐判定（§5.1 ⚠ 透明语义教训）
    bool isReady() const { return m_glReady; }
    /// 拾取表（§8.1；updateHover 解码用——与批次同步构建）
    const QVector<QChartHitTester::PickRecord>& pickTable() const { return m_batches.pickTable; }
    /// billboard 标签（§6 overlay；GlHost paintGL 绘制——dirty 时随 collectScene 重建）
    const QVector<QChartTextLabel>& labels() const { return m_cachedLabels; }

    // ===== 拾取（A6；渲染执行在本类，解码在 QChartHitTester）=====
    /// 鼠标移动时：渲染 ID 帧 → 光标 1×1 readback → 返回 RGB24。
    /// ⚠ 实现③④（t46）落地；本任务骨架：qWarning + 哨兵（0xFFFFFF = 无命中）
    QRgb pickIdAt(const QPoint& pos, const QChartScene& scene);

private:
    /// 图元列表 → VBO 批次（数据/样式变化才重建，A5）：collectScene + uploadBatches
    void buildBatches(const QChartScene& scene);
    /// 收集图元 + 构建 pickTable（纯 CPU，无 GL 依赖；§3.2「首次 dirty 收集一次」）
    void collectScene(const QChartScene& scene);
    /// 同 (Layer, primitive) 合并、64K 分片 → 打包 16B 顶点 → glBufferData 上传（需上下文 current）
    void uploadBatches();
    /// 主 pass / ID pass 共用批次遍历（分层：Grid→Series→Decor；pick 属 t46）
    void drawPass(const QChartScene& scene, ShaderKind kind);
    /// 当前上下文 GL 函数（3.3 Core；null 安全）
    QOpenGLFunctions_3_3_Core* glFuncs() const;

    friend class TestQOpenGLRenderer;   // 单测直查批次结构（图元→顶点数/合并/baseId，§13.2）

    QWidget* m_host = nullptr;
    QVector<QChartPrimitive> m_cachedPrimitives;   // 收集缓存（dirty 才重建，§3.2）
    QVector<QChartTextLabel> m_cachedLabels;       // billboard 标签（§6；overlay 绘制源）
    BatchPool m_batches;                           // §3.1
    QSize m_viewportSize;
    bool m_glReady = false;        // 惰性初始化标记（A3）
    bool m_initAttempted = false;  // initializeGL 已尝试（防每帧重试刷 qWarning）
    // 拾取限流状态（§5.3 第二道闸：位移 ≥1px + 距上次 ≥16ms；第三道闸 m_glReady）
    QElapsedTimer m_pickClock;
    QPoint m_lastPickPos;
    qint64 m_lastPickMs = -1000000;   // 首帧前（未初始化）→ 远过去
    QRgb m_lastPickResult = qRgb(255, 255, 255);   // 哨兵（无命中）
    bool m_lastPickValid = false;
};

#endif // QOPENGLCHART_RENDERER_H
