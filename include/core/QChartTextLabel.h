#pragma once
#include <QString>
#include <QColor>
#include <QVector3D>
struct QChartTextLabel
{
    // ---- 文字内容与样式 ----
    QString text;
    QColor color;
    float fontSize = 10.0f;
    Qt::Alignment alignment = Qt::AlignCenter;

    // ---- 定位与归属 ----
    int sourceId = -1;          // 归属（Axis 或 Series 的 ID）
    int refPrimitiveId = -1;    // ★ 绑定的图元 ID（-1 表示自由标签）

    // ---- 双空间锚点 ----
    QVector3D numericAnchor{0, 0, 0};   // 收集时填入
    QVector3D cartesianAnchor{0, 0, 0}; // 步骤 2 由 Renderer 填入
    QPointF pixelpos{0, 0};             // 步骤 3 由 Renderer 填入

    // ---- 运行时状态（由 Renderer 基类在步骤 2 维护） ----
    bool visible = true;
};
