#pragma once
#include <QString>
#include <QColor>
#include <QVector3D>
#include <QtNumeric>
#include <cmath>

// ===== 标签锚点契约（tier1/tier2/tier3 三级优先级；段化后绑定介质改为"段内偏移"）=====
// 标签住在 QChartSegment::labels 里，**归属由所在段表达**，因此没有组号字段。
// Renderer 步骤 2（cullAndResolveLabels）按下列优先级解析 cartesianAnchor 与 visible：
//
//   tier1 显式锚点：numericAnchor 任一分量非 NaN
//         → cartesianAnchor = projection->toCartesian(numericAnchor)
//         → visible = 锚点投影像素落在非退化 plotArea 内（两后端共用基类
//           QChartRenderer::anchorVisibleInPlotArea 判定，非有限投影视为不可见）
//   tier2 段内绑定：numericAnchor 全 NaN 且 segPrim ≥ 0
//         → 继承**本段第 segPrim 个图元**的 cartesianAnchor
//         → segPrim 越界（≥ 本段图元数）视为非法绑定 → 不可见
//   tier3 自由标签：numericAnchor 全 NaN 且 segPrim == -1
//         → CPU：锚到**本段最后一个可见图元**的组尾锚点（段内无可见图元 → 不可见）
//         → **GL（纯 GPU 后端）：无能力渲染自由标签——GL cull 一律置不可见**；二维网格脊
//           标签因此在 GL 后端不显示（既定已接受差异）
//
// ★ NaN 哨兵（写死规则）：**未设置坐标必须写 NaN**——numericAnchor 默认显式初始化为全 NaN，
//   表示"无显式锚点"；**{0,0,0} 视为显式坐标（原点）**，不是"未设置"。
struct QChartTextLabel
{
    // ---- 文字内容与样式 ----
    QString text;
    QColor color;
    float fontSize = 10.0f;
    Qt::Alignment alignment = Qt::AlignCenter;

    // ---- 段内绑定 ----
    /// 本段内第几个图元（**段内偏移**，不是任何全局下标）。
    /// -1 = 自由标签（不绑定具体图元，由段尾可见图元定位）。
    /// ★ 为什么是段内偏移：段是重建的原子单位，"本段第几个图元"在段重建后依然成立；
    ///   而"场景数组下标"跨重建必失效（历史字段 refPrimitiveId 的坑）。
    int segPrim = -1;

    // ---- 双空间锚点 ----
    // numericAnchor：收集时填入；**未设置必须写 NaN**（全 NaN = 无显式锚点，默认哨兵见契约）；
    //   {0,0,0} 是"显式坐标（原点）"，禁止用来表示未设置。
    QVector3D numericAnchor{static_cast<float>(qQNaN()),
                            static_cast<float>(qQNaN()),
                            static_cast<float>(qQNaN())};
    QVector3D cartesianAnchor{0, 0, 0}; // 步骤 2 由 Renderer 填入
    QPointF pixelpos{0, 0};             // 步骤 3 由 Renderer 填入

    // ---- 运行时状态（由 Renderer 基类在步骤 2 维护） ----
    bool visible = true;

    /// tier1 判定：numericAnchor 任一分量非 NaN = 显式锚点（全 NaN = 交给 tier2/tier3）
    bool hasExplicitAnchor() const
    {
        return !(std::isnan(numericAnchor.x())
                 && std::isnan(numericAnchor.y())
                 && std::isnan(numericAnchor.z()));
    }
};
