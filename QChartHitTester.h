// QChartHitTester.h —— 统一反向映射引擎（pixel→data；Phase 3 任务 0，纯重构）
// 背景：2D hitTest（QChartLayer::hitTest + series->hitTest 像素 in 多边形）与 3D hover
// （QChartWidget3D::updateHover 屏幕近邻）本质是同一个反向映射，散落两处；抽取统一引擎。
// GL 后端上马后，拾取将实现为同一接口的第三种实现（GPU 颜色编码）——拾取不进 renderer。
// 命名向 2D 看齐（用户定案）：方法统一叫 hitTest（重载）、结果统一叫 HitResult。
// 纯重构：零行为变化（2D 顶层优先/可见性过滤逻辑原样搬入；3D Series 层过滤/点与线段距离/
// 8px 阈值/dataIndex 透传原样搬入）。
#ifndef QCHARTHITTESTER_H
#define QCHARTHITTESTER_H

#include "QChartRenderer.h"   // QChartPrimitive（PickRecord::layer 需完整类型；§8.1）
#include <QPointF>
#include <QList>
#include <QVector>
#include <QVariant>
#include <functional>

class QChartSeries;
struct DrawContext;

class QChartHitTester {
public:
    /// 统一命中结果：series + index（2D）/ dataIndex（3D；2D 下 dataIndex == index）
    /// ⚠ series 非 const：既有调用方（QChartWidget 悬停）做 `QChartSeries* s = result.series`
    /// 赋值，const 会破坏「现有调用方零改动」红线（任务描述草稿的 const 按此修正）。
    struct HitResult {
        QChartSeries* series = nullptr;
        int index = -1;
        int dataIndex = -1;
    };

    // ===== 2D：像素 → Data（现 QChartLayer::hitTest 逻辑搬入，零行为变化）=====
    /// 顶层可见系列优先遍历（自后向前）+ series->hitTest（像素 in 多边形）；
    /// 未命中返回空 HitResult（dataIndex == -1）
    static HitResult hitTest(const QPointF& pixel,
                             const QList<QChartSeries*>& seriesList,
                             std::function<QPointF(QVariant, QVariant)> toPixel,
                             const struct DrawContext* ctx);

    // ===== 3D：像素 → 屏幕近邻图元（现 QChartWidget3D::updateHover 近邻核心搬入）=====
    /// 只扫 QChartPrimitive::Layer==Series 层图元（Grid/ForegroundDecor 排除）；
    /// 点到点/点到线段距离；距离 < maxDistPx 取最近；dataIndex 透传；未命中返回空 HitResult
    static HitResult hitTest(const QPointF& pixel,
                             const QVector<QChartPrimitive>& primitives,
                             qreal maxDistPx = 8.0);

    /// 像素到图元距离（Point=|pos−a|；LineSegment=点到线段距离）——供调用方在命中后
    /// 收紧全局阈值（保持跨系列全局最近语义）等场景复用
    static qreal distanceToPrimitive(const QPointF& pos, const QChartPrimitive& prim);

    // ===== GPU 拾取记录（design_phase3.md §8.1，t42 落地）=====
    /// 图元 ID → 命中结果（与 GL 批次同步构建；轴/网格不编码 → dataIndex=-1）
    struct PickRecord {
        QChartSeries* series = nullptr;   // 系列（series 层）；轴/网格装饰 = nullptr
        int dataIndex = -1;               // 数据索引（同 2D HitResult.dataIndex 语义）
        QChartPrimitive::Layer layer;     // 备用（调试/断言）
    };

    // ===== GPU 拾取解码（design_phase3.md §8.1，t46 落地）=====
    /// 纯函数（无 GL 依赖，可单测）：光标 1×1 读回 RGB24 → ID → 查表 → HitResult。
    /// id = r | g<<8 | b<<16；id==0xFFFFFF（哨兵：背景/轴网格 Decor 片段，§5.3 定案）或越界 → 空 HitResult
    static HitResult hitTestGPU(uint8_t r, uint8_t g, uint8_t b,
                                const QVector<PickRecord>& pickTable);
};

#endif // QCHARTHITTESTER_H
