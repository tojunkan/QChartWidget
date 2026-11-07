// QChartAbstractLayer.h —— 图层抽象基类（维度无关公共部分）
// 职责：每层一个场景快照（QChartScene m_scene）+ 渲染上下文注入（plotArea/背景/通用投影）
//      + 脏表（部分收集）+ 收集/数据范围重算两个子类接口。
// 维度无关：二维层（layers/2d/QChartLayer.h）与三维层（layers/3d/QChartLayer3D.h）各自继承本类，
//      并补充自身专属成员与逻辑（相机、轴、绘制/收集细节均不下沉到本类）。
// 不含：相机、轴绑定、网格/系列/交互等任何维度相关逻辑。
// 说明：本类不声明 Q_OBJECT（无信号/槽/属性等元对象需求）；子类按需自行声明。
//
// ===== 脏表（部分收集的唯一依据）=====
// dataDirty 不是一个布尔，而是"按贡献者的脏表"：
//   · axis 一行：所有脊永远同建同弃（域变全变、任一轴属性变也全变）→ 一个 bool 够。
//   · 每个系列一行：谁的 data/visible/color 变了就标谁 → 只重建那一段。
// 后果落在 QChartSegment::cartValid 上（重建即失效），renderer 据此只重算该段的 cart。
// 不变量：**脏表非空 ⇒ 必须有一帧 paint 在排队**（置脏后立刻调 scheduleRepaint）。
#ifndef QCHARTABSTRACTLAYER_H
#define QCHARTABSTRACTLAYER_H

#include <QObject>
#include <QRectF>
#include <QColor>
#include <QFont>
#include <QMatrix4x4>
#include <QSet>
#include <functional>
#include "QChartScene.h"
#include "QChartDebug.h"   // 脏变量观测（chart.dirty）

class QChartAbstractProjection;
class QChartSeries;

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

    // ===== 重绘请求（widget 在 addLayer 时注入；本类置脏后调用）=====
    // 不新增信号：layer 只需知道"我要重画"，不需要知道调用者是谁。
    std::function<void()> scheduleRepaint;

    // ===== 脏表 =====
    /// 轴段要重建（域变 / 轴属性变 / 网格样式变 / 首帧）
    void invalidateAxis()
    {
        m_axisDirty = true;
        QCHART_DIRTY_MSG(QStringLiteral("layer: invalidateAxis()（axis 行置脏）"));
        if (scheduleRepaint) scheduleRepaint();
    }

    /// 某系列的内容要重建（数据 / 可见性 / 颜色 / 新增）
    void invalidateSeries(QChartSeries* s)
    {
        if (!s) return;
        m_dirtySeries.insert(s);
        QCHART_DIRTY_MSG(QStringLiteral("layer: invalidateSeries()（该系列行置脏）"));
        if (scheduleRepaint) scheduleRepaint();
    }

    /// 脏表的唯一消费点：脏则按行重建，净则跳过（返回 false）。
    bool ensureSceneCollected()
    {
        if (!m_axisDirty && m_dirtySeries.isEmpty())
            return false;

        QCHART_DIRTY_MSG(QStringLiteral("layer: collect BEGIN axis=%1 series=%2")
                             .arg(m_axisDirty ? 1 : 0).arg(m_dirtySeries.size()));
        if (m_axisDirty)
            collectAxisPrimitives();
        if (!m_dirtySeries.isEmpty())
            collectSeriesPrimitives();

        m_axisDirty = false;
        m_dirtySeries.clear();
        ++m_collectCount;

        int prims = 0;
        for (const QChartSegment& s : m_scene.axisSegments) prims += s.prims.size();
        for (const QChartSegment& s : m_scene.seriesSegments) prims += s.prims.size();
        QCHART_DIRTY_MSG(QStringLiteral("layer: collect END axisSegs=%1 seriesSegs=%2 prims=%3 (第 %4 次)")
                             .arg(m_scene.axisSegments.size())
                             .arg(m_scene.seriesSegments.size())
                             .arg(prims).arg(m_collectCount));
        return true;
    }
    int collectCount() const { return m_collectCount; }   // 诊断：重收集次数（单测计数断言用）
    void resetCollectCount() { m_collectCount = 0; }

    // ===== 子类必须实现 =====
    /// 只重建轴段（清 axisSegments 后重建；由脏表的 axis 行驱动）
    virtual void collectAxisPrimitives() = 0;
    /// 只重建脏系列对应的段，然后按系列顺序归并 seriesSegments
    virtual void collectSeriesPrimitives() = 0;
    /// 数据范围重算：各维度层按自身语义实现（当前迁移期允许空实现）
    virtual void recomputeDataBounds() = 0;

    // ===== 维度无关的布局查询 =====
    /// 边框轴外边距占用：widget 计算 plotArea 时对每层调用并把四项累加。
    virtual void borderAxisSizeHint(const QFont& font, qreal& left, qreal& top,
                                    qreal& right, qreal& bottom) const;

protected:
    QChartScene m_scene;                 // 层场景快照（二维/三维共用）
    bool m_axisDirty = true;             // 脏表的 axis 行（首帧为脏）
    QSet<QChartSeries*> m_dirtySeries;   // 脏表的系列行（非拥有指针）
    int m_collectCount = 0;              // 诊断：重收集次数
};

#endif // QCHARTABSTRACTLAYER_H
