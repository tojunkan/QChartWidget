#pragma once
#include <QRectF>
#include "QChartPrimitive.h"
#include "QChartTextLabel.h"
#include "QChartCamera.h"
#include "QChartAbstractProjection.h"   // QChartWorldBox（按值）
struct QChartScene
{
    QChartScene() : PrimitiveIdPrefixSum(1, 0) {} // 预留一个元素，方便计算前缀和
    // ---- 核心负载 ----
    QVector<QChartPrimitive> primitives;         // 所有图元（Numeric 坐标）
    QVector<QChartTextLabel> labels;             // 所有标签（Numeric 锚点）

    int maxSourceId = 0;                        // ★ 所有图元/标签的最大 sourceId，Widget调用buildScene的时候分配（用于分配新 ID）
    QVector<int> PrimitiveIdPrefixSum;           // ★ 所有 sourceId 的 PrimitiveId 前缀和，Widget调用buildScene的时候分配（用于分配新 PrimitiveId）

    // ---- 映射工具（只读指针，生命周期由调用方保证） ----
    const QChartAbstractCamera* camera = nullptr;
    const QChartAbstractProjection* projection = nullptr;

    // ---- 绘制目标信息 ----
    QRectF plotArea;             // 像素坐标系下的绘制区域

    // ---- 环境参数 ----
    QColor backgroundColor;      // 画布底色（invalid = 透明）
    bool exportMode = false;     // 导出模式（跳过调试/交互元素）
};
