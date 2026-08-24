// QChartSeries3D.h —— 3D 系列基类（: QChartSeries，白捡 name/visible/opacity/color/主题色/图例）
// 两层数据组织（design_3d.md §6.2，D-3D-6；t11 按 designer 修订执行全链闭包）：
//   Data 层（存储）：QVector<QDataPoint3D>（QVariant 三元组，任意 Axis 类型 u/v 可为 QDateTime 等）
//   World 层（渲染时产生）：全链闭包 ProjectFn3D 内完成
//     QVariant×3 --[axisX/Y/Z::toNumeric]→ qreal×3 --[projection3D::toWorld]→ QVector3D
//     --[camera3D::project]→ QChartProjectedPoint{screen, depth}
//   闭包由 QChartLayer3D 组装注入（系列只存 Data、零耦合——与 2D toPixel 同构）。
//   系列循环内：p = projectFn(m_points[i]); if (!finite(p.screen)) skip;
//               primitive{depth=p.depth, dataIndex=i, ...}。
// ⚠ draw 名字隐藏红线（§6.2）：本类 draw(ProjectFn3D 闭包) 与 QChartSeries::draw(QVariant×2 闭包)
//   签名不同 → 重载隐藏而非覆盖。调用方一律持有 QChartSeries3D*（QChartLayer3D 经 m_series3D 遍历），
//   绝不通过 QChartSeries* 多态调用 3D draw。基类纯虚 draw 以「告警 no-op 桩」满足（可实例化兜底），
//   任何误经 QChartSeries* 的 2D 绘制都会响亮失败而非静默画错。
//
// ★ Phase 3 双存储（design_phase3.md §9，A3 内存预算的必要条件；t51）：
//   数值型路径（append(qreal×3) 便捷重载）→ m_numericCache（float3 权威存储，12B/点，QVector<QVector3D>）；
//   QVariant 路径（append(QVariant×3)/append(QDataPoint3D)/setPoints/insert/replace）→ m_points
//   （QDataPoint3D 列表权威，Phase 2 边界不变）；首个 QVariant 路径 append 使 numericCacheActive()==false
//   回退（此前 float3 一次性物化为 QDataPoint3D）。points()/at()/count() API 语义与 Phase 2 完全一致：
//   numeric-only 时 QVariant 按需物化（points() 物化视图 / at() 单点物化），GL 渲染路径直读
//   numericCache（免 QVariant 解包）。append 增量维护；clear 失效复位；replace/insert 失效回退；
//   remove 增量维护（保持数值型）。
//   World 缓存（VBO 源，§9）：worldCache 由 QChartLayer3D 渲染时填充（数值型：= toWorld(numericCache)；
//   曲面：= toWorld(toNumeric(grid))），series 零耦合红线——本类不持 Axis/Projection 引用；
//   投影/数据变化才重建（数据变化本类自清 + Layer3D 置脏）。
#ifndef QCHARTSERIES3D_H
#define QCHARTSERIES3D_H

#include "QChartSeries.h"
#include "QDataPoint3D.h"
#include "QChartCamera3D.h"   // QChartProjectedPoint（ProjectFn3D 返回类型）
#include "QChartRenderer.h"   // QChartPrimitive（§7.3）
#include <QVector>
#include <QVector3D>
#include <functional>

struct DrawContext3D;   // t11 QChartLayer3D 定义；此处仅指针参数（前向声明足够）

/// 3D 全链投影闭包：Data(QDataPoint3D) → {screen, depth}（Layer3D 组装，系列零耦合）
using ProjectFn3D = std::function<QChartProjectedPoint(const QDataPoint3D&)>;

class QChartSeries3D : public QChartSeries {
    Q_OBJECT
public:
    explicit QChartSeries3D(const QString& name = {}, QObject* parent = nullptr);

    // ===== 数据（Data 空间：QVariant 三元组）=====
    int count() const override;
    const QVector<QDataPoint3D>& points() const;
    QDataPoint3D at(int i) const;
    void append(const QDataPoint3D& pt);
    void append(qreal x, qreal y, qreal z);            // 便捷（QValueAxis 场景）
    void append(QVariant x, QVariant y, QVariant z);
    void insert(int index, const QDataPoint3D& pt);
    void remove(int index);
    void replace(int index, const QDataPoint3D& pt);
    void clear();
    void setPoints(const QVector<QDataPoint3D>& pts);

    // ===== Phase 3：数值预转换缓存（design_phase3.md §9；A3 内存预算前提）=====
    /// Numeric 三元组（float3，12B/点；仅全部数值型 append 时权威有效）
    const QVector<QVector3D>& numericCache() const { return m_numericCache; }
    /// 全部数值型 append → true（QVariant 路径/混合 → false；clear 后复位 true）
    bool numericCacheActive() const { return m_numericCacheActive; }

    // ===== Phase 3：World 缓存（VBO 源；Layer3D 渲染时填充——series 零耦合：本类不持 Axis/Projection）=====
    /// 数值型：worldCache = toWorld(numericCache)；曲面：toWorld(toNumeric(grid))；混合系列：空
    const QVector<QVector3D>& worldCache() const { return m_worldCache; }
    QVector<QVector3D>& worldCache() { return m_worldCache; }   // Layer3D 填充入口（内部）

    // ===== 图元收集（Renderer 3D 主路径：不排序、不直接绘制，D-3D-9）=====
    /// projectFn: 全链闭包（Data→Numeric→World→{screen,depth}，Layer3D 组装）
    /// 输出图元：type/a/b/depth/dataIndex/color/markerSize/penWidth 已填
    virtual void collectPrimitives(const ProjectFn3D& projectFn,
                                   QVector<QChartPrimitive>& out) const = 0;

    // ===== 直接绘制入口（painter's algorithm 关闭/单系列调试；⚠ 重载隐藏基类 draw，见头注释）=====
    virtual void draw(QPainter* painter,
                      const ProjectFn3D& projectFn,
                      const DrawContext3D* ctx = nullptr) const;

    // ⚠ 基类纯虚 draw 的桩（2D 签名）：3D 系列不可走 2D 绘制路径，误用 → qWarning + no-op
    void draw(QPainter* painter,
              std::function<QPointF(QVariant, QVariant)> toPixel,
              const struct DrawContext* ctx = nullptr) const override;

signals:
    void dataChanged();

protected:
    /// QVariant 路径批量赋值（setPoints/setGrid 共用）：m_points 权威、numericCache 失效（不发信号）
    void setPointsInternal(const QVector<QDataPoint3D>& pts);

    /// numeric-only → m_points 权威回退：float3 一次性物化为 QDataPoint3D 列表（§9 按需物化语义）
    void materializeToQVariant();

    QVector<QDataPoint3D> m_points;       // QVariant 权威存储（QVariant/混合路径；numeric-only 保持空）
    QVector<QVector3D> m_numericCache;    // float3 权威存储（数值型路径；12B/点）
    bool m_numericCacheActive = true;     // 全部数值型 append → true；QVariant 路径回退 → false；clear 复位
    QVector<QVector3D> m_worldCache;      // World 层缓存（Layer3D 渲染时填充；VBO 源；数据变化本类自清）
    mutable QVector<QDataPoint3D> m_pointsView;  // numeric-only 按需物化视图（points() 时构建）
    mutable bool m_pointsViewValid = false;      // 物化视图是否与 m_numericCache 同步
};

#endif // QCHARTSERIES3D_H
