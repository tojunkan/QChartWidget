#pragma once
#include <QString>
#include <QColor>
#include <QVector3D>
#include <QtNumeric>
#include <cmath>

// ===== 标签锚点契约（批次1 定案：三级优先级；批次2 注释如实化；t31 恢复 GL 既定契约）=====
// Renderer 步骤 2（cullAndResolveLabels）按下列优先级解析 cartesianAnchor 与 visible，
// CPU（QPainterChartRenderer）与 GL（QOpenGLChartRenderer）各自的 cull 路径同契约：
//
//   tier1 显式锚点：numericAnchor 任一分量非 NaN
//         → cartesianAnchor = projection->toCartesian(numericAnchor)（2D/3D 分支同一入口）
//         → visible = 锚点投影像素落在非退化 plotArea 内（两后端共用基类
//           QChartRenderer::anchorVisibleInPlotArea 判定，非有限投影视为不可见）
//   tier2 图元绑定：numericAnchor 全 NaN 且 refPrimitiveId ≥ 0
//         → 继承该图元（下标 = refPrimitiveId）的 cartesianAnchor
//         → visible：CPU 后端 = 该图元的精确裁剪结果（继承）；**GL 后端使用较粗的可见性
//           判断**——图元可见性归 GPU 裁剪，CPU 侧一律视为可见，实际落屏由标签绘制时的
//           plotArea 命中检查兜底（不区分图元是否被 GPU 裁掉）
//         → refPrimitiveId 越界（≥ primitives.size()）视为非法绑定 → 不可见
//   tier3 自由标签：numericAnchor 全 NaN 且 refPrimitiveId == -1
//         → CPU：沿用"同 sourceId 组尾最后可见图元"算法（组内无可见图元 → 不可见）
//         → **GL（纯 GPU 后端）：无能力渲染自由标签——GL cull 一律置不可见**；二维网格脊
//           标签因此在 GL 后端不显示（既定已接受差异，批次1 过审契约；t31 按用户裁定恢复）
//         → 未来"混合后端（CPU 前置 projection+裁剪）"预研旁路：
//           QChartRenderer::hybridResolveFreeLabelAnchor —— 当前不启用，正常渲染路径不得调用
//
// ★ NaN 哨兵（写死规则）：**未设置坐标必须写 NaN**——numericAnchor 默认显式初始化为全 NaN，
//   表示"无显式锚点"；**{0,0,0} 视为显式坐标（原点）**，不是"未设置"。
//   若用 {0,0,0} 表示未设置，会被 tier1 误判为显式锚点并把绑定/自由标签锚到原点。
struct QChartTextLabel
{
    // ---- 文字内容与样式 ----
    QString text;
    QColor color;
    float fontSize = 10.0f;
    Qt::Alignment alignment = Qt::AlignCenter;

    // ---- 定位与归属 ----
    int sourceId = -1;          // 归属（Axis 或 Series 的 ID）
    int refPrimitiveId = -1;    // ★ 绑定的图元 ID（-1 表示自由标签；见上方三级优先级）

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
