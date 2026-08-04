// QChartAxis.h —— 轴基类
// 职责：数值化（Data↔Numeric）+ 刻度生成 + 标签格式化 + 绘制
// 不负责：坐标映射（Projection 的事）、视窗变换（Widget 的事）
// 五空间链中：Axis 掌管 Data ↔ Numeric 这一环
#ifndef QCHARTAXIS_H
#define QCHARTAXIS_H

#include "QChartProjection.h"
#include <QObject>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QPainter>
#include <Qt>
#include <QFont>
#include <QVariant>

// ===== 前置声明 =====
struct DrawContext;

// ===== DrawContext：每次 draw 调用时传递的上下文 =====
struct DrawContext {
    QRectF plotArea;                   // 绘图区像素矩形
    QRectF dataBounds;                 // 当前可见 Numeric 范围: (dim0Min, dim1Min, dim0Span, dim1Span)
    QRectF viewRect;                   // 当前 View Cartesian 窗口
    const QChartProjection* projection = nullptr;
};

class QChartAxis : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(int tickCount READ tickCount WRITE setTickCount NOTIFY tickCountChanged)
    Q_PROPERTY(int subTickCount READ subTickCount WRITE setSubTickCount NOTIFY subTickCountChanged)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)

public:
    explicit QChartAxis(QObject* parent = nullptr,
                        Qt::Alignment alignment = Qt::AlignBottom);
    virtual ~QChartAxis() = default;

    // ===== 数值化（纯虚）—— 跨类型统一的数字中间层 =====
    /// Data → Numeric。非法 data 返回 NaN 并 qWarning
    virtual qreal toNumeric(QVariant data) const = 0;
    /// Numeric → Data。NaN/Inf 按子类策略处理
    virtual QVariant fromNumeric(qreal num) const = 0;

    // ===== 刻度生成（纯虚）—— 在给定 Numeric 区间内生成合适刻度 =====
    /// 在 [numericMin, numericMax] 区间内生成主刻度位置（Numeric 空间的值）
    virtual QVector<qreal> tickValues(qreal numericMin, qreal numericMax) const = 0;
    /// 给定刻度数值（Numeric 空间），返回格式化标签字符串
    virtual QStringList tickLabels(const QVector<qreal>& ticks) const = 0;
    /// 次刻度位置，默认返回空。子类可覆盖
    virtual QVector<qreal> subTickValues(qreal numericMin, qreal numericMax) const;

    // ===== 绘制 =====

	static constexpr qreal textPadding() { return TEXT_PADDING; }

    /// 边框轴模式：画在 plotArea 对应边缘（仅 Cartesian 有效）
    /// 不依赖 Projection，直接用 plotArea 边缘坐标线性插值
    void drawAtEdge(QPainter* painter, const DrawContext& ctx,
                    bool drawAxisLine, bool drawLabels, bool drawTicks) const;

    /// 数据主脊模式：画在 offset 指定的 Numeric 位置（所有坐标系有效）
    /// offset 是另一维度的 Numeric 值（由调用者 Geometry 提供）
    void drawAtPosition(QPainter* painter, const DrawContext& ctx, qreal offset,
                        bool drawAxisLine, bool drawLabels, bool drawTicks) const;

    /// 边框轴占用空间估算；数据主脊返回 {0, 0}
    virtual QSizeF sizeHint(const QFont& font) const;

    // ===== 语法糖（仅 Cartesian，内部转发给 Widget）=====
    /// setRange / min / max 仅在 Cartesian 下有意义
    /// 内部存储语法糖字段，通过 rangeChanged 信号由 Widget 连接后
    /// 映射到 Widget::setDataRangeDim0/Dim1
    void setRange(qreal min, qreal max);
    qreal min() const { return m_sugarMin; }
    qreal max() const { return m_sugarMax; }

    // ===== 样式 / 查询 =====
    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment a) { m_alignment = a; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool v);

    QString title() const { return m_title; }
    void setTitle(const QString& t) { m_title = t; }

    QColor color() const { return m_color; }
    void setColor(const QColor& c) { m_color = c; }

    int tickCount() const { return m_tickCount; }
    void setTickCount(int n);

    int subTickCount() const { return m_subTickCount; }
    void setSubTickCount(int n);

    /// 是否为水平方向（Top / Bottom / HCenter）
    bool isHorizontal() const {
        return (m_alignment == Qt::AlignBottom || m_alignment == Qt::AlignTop
                || m_alignment == Qt::AlignHCenter);
    }

signals:
    /// setRange 语法糖触发，Widget 连接后映射到 setDataRangeDim0/Dim1
    void rangeChanged(qreal min, qreal max);
    void visibleChanged();
    void styleChanged();
    void tickCountChanged();
    void subTickCountChanged();

protected:
    // ── 语法糖字段（仅 Cartesian 有效，非映射基准）──
    qreal m_sugarMin = 0.0;
    qreal m_sugarMax = 0.0;

    // ── 刻度参数 ──
    int m_tickCount = 5;         // 目标主刻度数（供 niceStep 参考）
    int m_subTickCount = 0;      // 每个主刻度间的次刻度数

    // ── 样式 ──
    bool m_visible = true;
    QString m_title;
    QColor m_color = Qt::black;
    Qt::Alignment m_alignment = Qt::AlignBottom;

    // ── 常量 ──
    static constexpr qreal AXIS_MARGIN = 8.0;   // 轴线到文字外侧的总边距
    static constexpr qreal TICK_LENGTH = 4.0;   // 主刻度线长度
    static constexpr qreal SUB_TICK_LENGTH = 2.0; // 次刻度线长度
    static constexpr qreal TEXT_PADDING = 3.0;   // 文字周围的微小呼吸空间
};

#endif // QCHARTAXIS_H
