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

    // ===== 数据脏标记 =====
    void invalidateData() { m_dataDirty = true; }

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
    bool m_dataDirty = true;    // 数据脏标记（invalidateData() 置位；由各层自身消费）
};

#endif // QCHARTABSTRACTLAYER_H
