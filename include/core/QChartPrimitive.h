#pragma once
#include <QColor>
#include <QVector3D>
#include <QRectF>
struct QChartPrimitive
{
    // ---- 几何类型 ----
    enum class Type {
        Point,          // 单点
        Line,           // 线段
        Rect,           // 矩形（轴对齐）
        Ellipse,        // 椭圆（轴对齐）
        Polygon,        // 多边形（任意顶点数）
        Path,           // 路径（折线/曲线采样后的顶点序列）
        TriangleMesh,   // 三角网格（带索引）
        TriangleFan,    // 扇形网格（隐式索引）
        TriangleStrip   // 条带网格（隐式索引）
    };
    Type type = Type::Point;

    // ===== 标识 =====
    // ★ 图元身上**没有任何编号**。它是谁产出的、属于第几段，全由它所在的 QChartSegment 表达
    //   （段自带 owner）；拾取只要"哪一段"，段号就是 seriesSegments 的下标，不需要图元级编号。
    //   历史上这里有过 sourceId / dataIndex / id 等字段，段化之后一并删除，别再往回加。

    // ---- 样式 ----
    QColor color;               // 主色（线条/边框/点色）
    QColor fillColor;           // 填充色（Rect/Ellipse/Polygon/Mesh 用）
    float penWidth = 1.0f;      // 线宽（像素）
    float markerSize = 4.0f;    // Point 标记大小（半径，像素）
    float depth = 0.0f;         // 3D CPU 排序用（GPU 后端忽略；按设计将在渲染器内部现算后删除）

    // ===== Numeric 空间（收集时由 Axis 数值化后填入） =====
    // Point / Line
    QVector3D numA{0, 0, 0};
    QVector3D numB{0, 0, 0};

    // Rect / Ellipse（轴对齐，存左上角+宽高）
    QRectF numRect{0, 0, 0, 0};

    // Polygon / Path / TriangleMesh（顶点序列）
    QVector<QVector3D> numVerts;

    // TriangleMesh / TriangleFan / TriangleStrip 专用索引
    QVector<int> numIndices;

    // ===== Cartesian 空间（由 Renderer 基类在步骤 2 填入） =====
    // 结构与 Numeric 一一对应
    QVector3D cartA{0, 0, 0};
    QVector3D cartB{0, 0, 0};
    QRectF cartRect{0, 0, 0, 0};
    QVector<QVector3D> cartVerts;
    QVector<int> cartIndices;   // 通常直接复用 numIndices，仅在变换后需要重排时使用

    // ===== 运行时状态（渲染步骤 2 写回；收集期不碰）=====
    /// 本帧裁剪结果：由 renderer 在 cull 阶段写回（契约 S5/S6）。
    /// ★ 视口裁剪只能"标 false"，**永远不能从段里删图元**——删了段内下标就抖，
    ///   标签的段内绑定与拾取的段遍历都会随之错位。
    /// ★ GL 后端恒为 true：真正的裁剪在 GPU 上做（深度/视锥由硬件判），该字段不承载语义。
    ///   CPU 后端才真正使用它。
    bool visible = true;
};
