#pragma once
#include <QObject>
#include <QRectF>
#include <QVector>
#include <QColor>
#include "QChartPrimitive.h"
#include "QChartTextLabel.h"
#include "QChartCamera.h"
#include "QChartAbstractProjection.h"   // QChartWorldBox（按值）

// =============================================================================
// 场景组织契约（S1–S10）—— 本文件的组织规则。改 scene 之前请先读完。
// -----------------------------------------------------------------------------
// 定位：scene 是**渲染与拾取的唯一共同输入**，由 layer 收集、由 renderer 消费。
//
// S1  段序 = 绘制顺序；axisSegments 恒在 seriesSegments 之前。
// S2  段是更新的原子单位：重建一段不得影响其它段的图元与标签。
// S3  段内图元同属一个 owner，且最多两种 ShaderKind（例外仅两个：
//     axisSegments、QPolygonSeries 的描边段）。
// S4  变换/细分 与 裁剪 由不同输入驱动：前者看段内容 + 投影；后者看相机 + plotArea。
// S5  细分只改"供绘制的几何"，不改段结构、不改图元数量、不改段号。
// S6  可见性：作者意图（series.visible / gridVisible / axis.visible）在收集期过滤，
//     **压根不收集**；视口裁剪只在渲染步骤 2 标 visible=false，**永不删元素**。
//     （删了段内下标就抖，标签绑定与拾取都会随之错位。）
// S7  m_segmentBatches 与段一一对应（数量与顺序同步）—— GL 后端落地时生效。
// S8  标签随所属段一起重建；绑定用段内偏移（segPrim）或段尾，不引用任何全局编号。
// S9  渲染器与拾取器不得改变段与图元的数量、顺序、所有者（结构只读）；
//     渲染器可以写运行时字段：prim.cart* / prim.visible / 标签的 cartesianAnchor·
//     visible·pixelpos；拾取器一个字段都不写。
// S10 拾取编号 = seriesSegments 的下标；不跨帧稳定；轴段不编码。
//
// ★ 为什么没有"组号 + 前缀和"：段本身就是组，段自带 owner，段内位置天然由下标表达。
//   历史上那套 sourceId + PrimitiveIdPrefixSum + groupSources 是为"从数组下标反查归属"
//   服务的；段化之后反查变成"取段的 owner"，三样东西一起失去消费者，故删除。
// =============================================================================

/// 段：一个生产者一次性产出的一批图元 + 标签（更新与重建的原子单位）。
struct QChartSegment
{
    /// 归属：QChartAxis 或 QChartSeries（非拥有指针；生命周期由使用方保证）。
    /// 消费方用 qobject_cast<QChartSeries*> 区分：非空 = 可拾取的系列，
    /// 空或转不成系列 = 装饰（轴 / 网格 / 盒边）。
    QObject* owner = nullptr;

    QVector<QChartPrimitive> prims;     // 收集期填 Numeric，渲染期填 Cartesian
    QVector<QChartTextLabel> labels;    // 收集期填 Numeric 锚点，渲染期填 Cartesian
};

struct QChartScene
{
    // ---- 核心负载（顺序即绘制顺序，见契约 S1）----
    QVector<QChartSegment> axisSegments;     // 轴脊 / 网格脊 / 3D 盒边（一个脊一段）
    QVector<QChartSegment> seriesSegments;   // 每条可见系列一段

    // ---- 映射工具（只读指针，生命周期由调用方保证） ----
    const QChartAbstractCamera* camera = nullptr;
    const QChartAbstractProjection* projection = nullptr;

    // ---- 绘制目标信息 ----
    QRectF plotArea;             // 像素坐标系下的绘制区域

    // ---- 环境参数 ----
    QColor backgroundColor;      // 画布底色（invalid = 透明）
    bool exportMode = false;     // 导出模式（跳过调试/交互元素）
};
