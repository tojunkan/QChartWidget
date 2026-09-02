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

    // ---- 标识 ----
    int id = -1;                // ★ 全局唯一 ID（收集时赋值为向量下标）
    int sourceId = -1;          // 归属（Axis 或 Series 的 ID）
    // int PrimitiveId = -1;       // 数据点索引（拾取用）
    // Scene实际上是一个巨大的数组，下标就可以当索引了

    // ---- 样式 ----
    QColor color;               // 主色（线条/边框/点色）
    QColor fillColor;           // 填充色（Rect/Ellipse/Polygon/Mesh 用）
    float penWidth = 1.0f;      // 线宽（像素）
    float markerSize = 4.0f;    // Point 标记大小（半径，像素）
    float depth = 0.0f;         // 3D CPU 排序用（GPU 后端忽略）

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
};
