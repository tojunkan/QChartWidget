#pragma once
#include <QColor>
#include <QVector3D>
#include <QRectF>
struct QChartPrimitive
{
    // ---- 几何类型 ----
    // ★ 折线只有一种表示：Path（2 点折线也是 Path）。曾经的 Line 特例已被删除——
    //   它逼出"渲染期把 Line 提升成 Path"这种不可重入的形态转换（B1：提升后 numVerts 为空，
    //   下一次投影失效时 cartVerts 被清成 0，线段永久消失）。删掉类型 = 删掉这类 bug 的成因。
    enum class Type {
        Point,          // 单点
        Rect,           // 矩形（轴对齐）
        Ellipse,        // 椭圆（轴对齐）
        Polygon,        // 多边形（任意顶点数）
        Path,           // 路径（折线/曲线采样后的顶点序列；2 点即一条线段）
        TriangleMesh,   // 三角网格（带索引）
        TriangleFan,    // 扇形网格（隐式索引）
        TriangleStrip,  // 条带网格（隐式索引）
        Grid            // 规则网格（rows×cols 共享控制点；索引由渲染端派生，每格 6 个）
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

    // ===== Numeric 空间（收集时由 Axis 数值化后填入） =====
    // Point
    QVector3D numA{0, 0, 0};

    // Rect / Ellipse（轴对齐，存左上角+宽高）
    QRectF numRect{0, 0, 0, 0};

    // Polygon / Path / TriangleMesh（顶点序列）
    // ★ Grid 也用 numVerts，但语义是"rows×cols 的**共享**控制点表"（顶点序 = r*cols + c）
    QVector<QVector3D> numVerts;

    // TriangleMesh / TriangleFan / TriangleStrip 专用索引
    QVector<int> numIndices;

    // Grid 专用：行列数（只在 Type::Grid 下有意义）
    //   numVerts 有 rows*cols 个控制点；每格 4 个控制点、派生 6 个索引（2 个三角形 patch）。
    //   控制点非有限（NaN/Inf）的格子由渲染端派生索引时跳过——与旧 TriangleMesh 时代的收集期跳过等价。
    int rows = 0;
    int cols = 0;

    // ===== Cartesian 空间（由 Renderer 基类在步骤 2 填入） =====
    // 结构与 Numeric 一一对应
    QVector3D cartA{0, 0, 0};
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

// ===== 收集的"第 2 步"：由 Numeric 数据生成图元 =====
// 所有 collectPrimitives 都是这两步：① 经 axis 翻译（Data→Numeric，各系列不同）；
// ② 由 Numeric 生成图元（同类图元完全通用，与数据来源无关）。这里就是第 2 步的共用入口。
//
/// - `fill`：填充色（Grid/Polygon/TriangleMesh 用），**同时写进 color**，供"不区分
///   fill/border"的后端兜底（GL 的打包就是这样取色的）。
/// - `rows`/`cols`：仅 `Type::Grid` 使用——顶点表是 rows×cols 的**共享**控制点，顶点序 = r*cols + c。
/// - ★ 将来 "wireframe（网格连线作为曲面的辅助显示）" 也在**这一处**派生：
///   行内相邻 + 列内相邻的线段（数量 = rows·(cols−1) + cols·(rows−1)）。
///   surface 系列与坐标系面共用本函数，避免两处各改一遍。
inline QChartPrimitive generatePrimitive(QChartPrimitive::Type type,
                                         const QVector<QVector3D>& numVerts,
                                         const QColor& fill,
                                         int rows = 0,
                                         int cols = 0)
{
    QChartPrimitive p;
    p.type = type;
    p.numVerts = numVerts;
    p.rows = rows;
    p.cols = cols;
    p.fillColor = fill;
    p.color = fill;
    return p;
}

/// 由网格图元派生**网格连线**（曲面的辅助显示）：行方向 rows 条 + 列方向 cols 条 Path。
/// 顶点与面完全同源（顶点序 r*cols+c），所以线的拐点必然落在格点上，不会和面错位。
inline QVector<QChartPrimitive> generateWireframe(const QChartPrimitive& grid,
                                                  const QColor& lineColor)
{
    QVector<QChartPrimitive> out;
    const int rows = grid.rows;
    const int cols = grid.cols;
    const QVector<QVector3D>& verts = grid.numVerts;
    if (rows < 2 || cols < 2 || verts.size() != rows * cols) return out;
    out.reserve(rows + cols);

    auto makePath = [&lineColor](const QVector<QVector3D>& pts) {
        QChartPrimitive p;
        p.type = QChartPrimitive::Type::Path;
        p.numVerts = pts;
        p.color = lineColor;
        // ★ 笔宽刻意区别于普通网格线的 1.0：GL 批次按 (kind, markerSize, penWidth) 分组，
        //   独立笔宽才能形成独立批次、并排在"面"批次之后（3D 两趟里线才压在面上）。
        p.penWidth = 1.25f;
        return p;
    };

    for (int r = 0; r < rows; ++r) {          // 行方向
        QVector<QVector3D> line;
        line.reserve(cols);
        for (int c = 0; c < cols; ++c) line.append(verts[r * cols + c]);
        out.append(makePath(line));
    }
    for (int c = 0; c < cols; ++c) {          // 列方向
        QVector<QVector3D> line;
        line.reserve(rows);
        for (int r = 0; r < rows; ++r) line.append(verts[r * cols + c]);
        out.append(makePath(line));
    }
    return out;
}
