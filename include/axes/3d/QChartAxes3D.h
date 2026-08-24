// QChartAxes3D.h —— 3D 轴参照系编排器（非 Q_OBJECT，design_3d_axes.md §8.2 定案）
// 职责：①复用 2D Axis 的刻度生成/标签格式化/样式（组合持有 QChartAxis*，非继承——
//       2D drawAtEdge/drawAtPosition 语义与 3D 不兼容，继承会带进误导性绘制接口）；
//       ②产出 Numeric 空间几何（盒 8 角/12 边/spine/刻度锚点）。
// 三层分离红线：本类只产 Numeric 空间几何，不做 toWorld/投影（Layer3D 做）、不数值化、不绘制。
//   （无 QPainter / 无 QChartCamera3D / 无 QChartProjection3D 引用——reviewer grep 验证点）
#ifndef QCHARTAXES3D_H
#define QCHARTAXES3D_H

#include <QVector3D>
#include <QVector>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QPointF>

class QChartAxis;

class QChartAxes3D {
public:
    QChartAxes3D();

    // ===== 每维配置槽（dim∈{0,1,2}；组合复用 QChartAxis*）=====
    struct AxisConfig {
        QChartAxis* axis = nullptr;        // 复用刻度/标签/样式（非持有）；null = 该维不生成刻度/标签
        bool visible = true;               // 单轴隐藏（A4），默认 true
        qreal markerSizePx = 4.0;          // tick 点标记半径（px，v2 反馈 1 定案：屏幕固定像素点标记）
        QPointF labelOffsetPx{0, 0};       // 标签偏移（px）；(0,0)=自动（沿投影轴外 10px）
        bool axisTitleVisible = true;
        QString axisTitle;                 // 空 = axis->title()，再空 = dimensionName
    };
    AxisConfig& axis(int dim) { return m_cfg[dim]; }
    const AxisConfig& axis(int dim) const { return m_cfg[dim]; }

    // ===== 总开关（demo 'A' 键）=====
    bool visible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }

    // ===== Numeric 空间几何（静态工具 + 委托，Layer3D 调用）=====
    /// 盒 8 角；约定 index = u | (v<<1) | (w<<2)（bit0=u、bit1=v、bit2=w，置位取 dataMax 分量）
    static QVector<QVector3D> boxCorners(const QVector3D& dataMin, const QVector3D& dataMax);
    /// 12 条边（角索引对）：u∥(0,1)(2,3)(4,5)(6,7)；v∥(0,2)(1,3)(4,6)(5,7)；w∥(0,4)(1,5)(2,6)(3,7)
    static QVector<QPair<int, int>> boxEdges();
    /// 3 条强调 spine（min 角出发）：{u∥边0, v∥边4, w∥边8}
    static QVector<int> spineEdgeIndices();
    /// 该维刻度值（Numeric）：= axis->tickValues(dimMin, dimMax)；axis 为 null 返回空
    QVector<qreal> ticks(int dim, qreal dimMin, qreal dimMax) const;
    /// 刻度标签：= axis->tickLabels(ticks)
    QStringList tickLabelTexts(int dim, qreal dimMin, qreal dimMax) const;
    /// 刻度锚点（Numeric）：dataMin 的 dim 分量替换为 tickValue（min 角 spine 边上）
    static QVector3D tickAnchor(int dim, qreal tickValue, const QVector3D& dataMin);

private:
    AxisConfig m_cfg[3];
    bool m_visible = true;
};

#endif // QCHARTAXES3D_H
