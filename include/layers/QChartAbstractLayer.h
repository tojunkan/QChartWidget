// QChartAbstractLayer.h —— 图层抽象基类（4a：维度无关公共部分）
// 职责：每层一个场景快照（QChartScene m_scene）+ 渲染上下文注入（plotArea/背景/通用投影）
//      + 数据脏标记 + 图元收集/数据范围重算两个子类接口。
// 维度无关：二维层（layers/2d/QChartLayer.h）与三维层（layers/3d/QChartLayer3D.h）各自继承本类，
//      并补充自身专属成员与逻辑（相机、轴、绘制/收集细节均不下沉到本类）。
// 不含：相机、轴绑定、网格/系列/交互等任何维度相关逻辑。
// 说明：本类不声明 Q_OBJECT（无信号/槽/属性等元对象需求）；子类按需自行声明。
#ifndef QCHARTABSTRACTLAYER_H
#define QCHARTABSTRACTLAYER_H

#include <QObject>
#include <QRectF>
#include <QColor>
#include <QFont>
#include <QMatrix4x4>
#include <cstring>
#include "QChartScene.h"

class QChartAbstractProjection;

class QChartAbstractLayer : public QObject
{
public:
    explicit QChartAbstractLayer(QObject* parent = nullptr);
    ~QChartAbstractLayer() override;

    // ===== 场景快照（每层一个；渲染上下文由 widget 渲染前注入）=====
    const QChartScene& scene() const { return m_scene; }
    QChartScene& scene() { return m_scene; }

    /// 注入场景上下文（widget 渲染前调用）
    void setSceneProjection(const QChartAbstractProjection* p) { m_scene.projection = p; }
    void setScenePlotArea(const QRectF& plotArea) { m_scene.plotArea = plotArea; }
    void setSceneBackground(const QColor& c) { m_scene.backgroundColor = c; }

    // ===== 数据/视图脏标记（4g：从“死标记”改为有真实消费点的机制）=====
    // 4g 规则更正：**视图变化必须重收集背景**——网格脊/刻度/标签由“轴持有的可见 numeric 范围”
    //   生成，视图（相机窗口/plotArea/投影/背景）变化后旧图元即过期。
    //   当前实现为**粗粒度 viewDirty = 全量更新**（重收集 + 变换 + 裁剪）；背景/前景分级留待后续批次。
    // ===== 4g-fix（t58 F4）：内容贡献指纹 =====
    // 除了“视图指纹”，收集结果还取决于维度相关内容（轴范围/刻度数/子刻度/可见性/颜色、网格模式与
    // 网格样式、数据版本…）。这些输入改由 widget 组装为**内容指纹**注入并与视图指纹一并比较——
    // 把“逐信号接线”升级为“状态比较”，避免再漏接某类样式信号。
    /// 内容指纹注入：与上次不同 ⇒ 置脏（重收集）
    void setSceneContentState(quint64 contentKey)
    {
        const bool same = m_hasContentState && m_contentKey == contentKey;
        m_contentKey = contentKey;
        m_hasContentState = true;
        if (!same)
            m_dataDirty = true;
    }
    /// 数据版本（invalidateData()/invalidateView() 自增）：可并入内容指纹
    quint64 contentRevision() const { return m_contentRevision; }
    /// 指纹组装辅助（FNV/混合变体，纯计算）
    static quint64 contentHash(quint64 seed, quint64 v)
    {
        return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
    }
    static quint64 contentHashReal(quint64 seed, qreal v)
    {
        quint64 bits = 0;
        std::memcpy(&bits, &v, sizeof(bits) < sizeof(v) ? sizeof(bits) : sizeof(v));
        return contentHash(seed, bits);
    }

    /// 数据变化（系列/轴数据、样式数据等）→ 下一次渲染重收集。
    /// 边界说明（4g）：二维轴范围变化经 4e 驱动链改写相机窗口（合法范围）→ 视图指纹变化 → 重收集；
    /// 退化范围（min==max，不 fit、相机不变）时轴刻度已变但层未置脏——二维轴→层置脏通路需在
    /// QChartWidget 内补（本任务 inScope 不含该文件，留待后续批次）；三维已由 QChartWidget3D 接通
    /// “轴范围变化 → invalidateData()”。
    void invalidateData() { m_dataDirty = true; ++m_contentRevision; }
    /// 视图变化（相机操作、resize 经 plotArea、投影/背景切换）→ 同样必须重收集（规则更正的落点）
    void invalidateView() { m_dataDirty = true; ++m_contentRevision; }

    /// 视图状态指纹注入（widget 渲染前调用）：相机视图投影矩阵 × plotArea × 投影 × 背景。
    /// 与上次不同 ⇒ 视图变化 ⇒ 置脏（相机 orbit/dolly/pan/zoom/fit、resize、投影/背景切换全覆盖；
    /// 无变化时不置脏 → 跳过重收集，避免每帧强制更新）。
    void setSceneViewState(const QMatrix4x4& viewProj, const QRectF& plotArea,
                           const QChartAbstractProjection* proj, const QColor& bg)
    {
        const bool same = m_hasViewState
                          && m_viewProj == viewProj
                          && m_viewPlotArea == plotArea
                          && m_viewProjection == proj
                          && m_viewBackground == bg;
        m_viewProj = viewProj;
        m_viewPlotArea = plotArea;
        m_viewProjection = proj;
        m_viewBackground = bg;
        m_hasViewState = true;
        if (!same)
            m_dataDirty = true;   // 视图变化（含首帧）→ 必须重收集背景
    }

    /// 场景快照收集入口 —— m_dataDirty 的**唯一消费点**：脏则收集并计数，净则跳过（返回 false）。
    bool ensureSceneCollected()
    {
        if (!m_dataDirty)
            return false;
        collectPrimitives();
        m_dataDirty = false;
        ++m_collectCount;
        return true;
    }
    int collectCount() const { return m_collectCount; }   // 诊断：重收集次数（单测计数断言用）
    void resetCollectCount() { m_collectCount = 0; }

    // ===== 子类必须实现 =====
    /// 图元收集：把本层内容写入 scene()（每帧重收集；各维度层自有实现）
    virtual void collectPrimitives() = 0;
    /// 数据范围重算：各维度层按自身语义实现（当前迁移期允许空实现）
    virtual void recomputeDataBounds() = 0;

    // ===== 维度无关的布局查询（4a）=====
    /// 边框轴外边距占用：widget 计算 plotArea 时对每层调用并把四项累加。
    /// 默认不占边距；二维层用 axisX/axisY 的 sizeHint 实现，三维层用其自身轴绑定实现
    /// （保持各自 plotArea 与迁移前逐位一致）。
    virtual void borderAxisSizeHint(const QFont& font, qreal& left, qreal& top,
                                    qreal& right, qreal& bottom) const;

protected:
    QChartScene m_scene;        // 层场景快照（4a：二维/三维共用；原二维层成员与三维 m_scene3D 并入此处）
    bool m_dataDirty = true;    // 数据/视图脏标记（invalidateData()/invalidateView()/视图指纹变化置位；
                                //   4g 起由 ensureSceneCollected() 唯一消费——脏才重收集）
    int m_collectCount = 0;     // 诊断：重收集次数（4g 计数断言）

    // 4g：视图状态指纹缓存（用于判定“视图是否变化”）
    bool m_hasContentState = false;   // 4g-fix：内容贡献指纹缓存
    quint64 m_contentKey = 0;
    quint64 m_contentRevision = 0;    // 数据版本（invalidateData/View 自增）
    bool m_hasViewState = false;
    QMatrix4x4 m_viewProj;
    QRectF m_viewPlotArea;
    const QChartAbstractProjection* m_viewProjection = nullptr;
    QColor m_viewBackground;
};

#endif // QCHARTABSTRACTLAYER_H
