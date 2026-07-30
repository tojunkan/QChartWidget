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

class QChartAxis : public QObject
{
    Q_OBJECT
        Q_PROPERTY(qreal min READ min WRITE setMin NOTIFY rangeChanged)
        Q_PROPERTY(qreal max READ max WRITE setMax NOTIFY rangeChanged)
        Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
        Q_PROPERTY(QString title READ title WRITE setTitle)
        Q_PROPERTY(QColor color READ color WRITE setColor)
        Q_PROPERTY(int tickCount READ tickCount WRITE setTickCount NOTIFY tickCountChanged)
        Q_PROPERTY(int subTickCount READ subTickCount WRITE setSubTickCount NOTIFY subTickCountChanged)
        Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
        Q_PROPERTY(bool dragEnabled READ isDragEnabled WRITE setDragEnabled)
        Q_PROPERTY(bool zoomEnabled READ isZoomEnabled WRITE setZoomEnabled)

public:
    // 构造函数中传入对齐方式（笛卡尔轴用），AngleAxis/RadialAxis可传入AlignCenter或不处理
    explicit QChartAxis(QObject* parent = nullptr, Qt::Alignment alignment = Qt::AlignBottom);
    virtual ~QChartAxis() = default;

    // ---------- 范围管理 ----------
    qreal min() const { return m_min; }
    qreal max() const { return m_max; }
    virtual void setMin(qreal v) {
        if (qFuzzyCompare(m_min, v)) return;
        m_min = v;
        emit rangeChanged(m_min, m_max);
    }
    virtual void setMax(qreal v) {
        if (qFuzzyCompare(m_max, v)) return;
        m_max = v;
        emit rangeChanged(m_min, m_max);
    }
    void setRange(qreal min, qreal max) {
        if (qFuzzyCompare(m_min, min) && qFuzzyCompare(m_max, max)) return;
        m_min = min;
        m_max = max;
        emit rangeChanged(m_min, m_max);
    }

    // ---------- 核心映射（纯数学）----------
    // 将数据原始值 -> 归一化 [0.0, 1.0]
    virtual qreal valueToNormalized(qreal value) const = 0;
    // 将归一化 [0.0, 1.0] -> 数据原始值
    virtual qreal normalizedToValue(qreal norm) const = 0;

    // ---------- 交互权限 ----------
    bool isDragEnabled() const { return m_panEnabled; }
    virtual void setDragEnabled(bool enabled) { m_panEnabled = enabled; }

    bool isZoomEnabled() const { return m_zoomEnabled; }
    virtual void setZoomEnabled(bool enabled) { m_zoomEnabled = enabled; }

    virtual void pan(qreal deltaNorm);
    virtual void zoom(qreal centerNorm, qreal factor);

    // ---------- 刻度 ----------
    int tickCount() const { return m_tickCount; }
    void setTickCount(int n) { 
        if (n < 2)n = 2;
        if (n == m_tickCount) return;
        m_tickCount = n; 
        emit tickCountChanged();
    }
    int subTickCount() const { return m_subTickCount; }
    void setSubTickCount(int n) {
        if (n == m_subTickCount) return;
        emit subTickCountChanged();
        m_subTickCount = n;
    }

    // 子类必须根据自身的 min/max 和刻度策略，返回原始数据值的刻度位置（数据坐标） 
    virtual QVector<qreal> tickValues() const = 0;
    // 子类根据 tickValues 返回格式化的字符串（日期/数字/对数符号等）
    virtual QStringList tickLabels() const = 0;

    virtual QVector<qreal> subTickValues() const = 0;
    virtual QStringList subTickLabels() const;

    // ---------- 布局 & 绘制（负责背景层的轴线和刻度标签）----------
    // 轴根据 alignment 和 plotArea 计算自己所需的空间（用于上层布局预留位置）
    virtual QSizeF sizeHint(const QFont& font) const;

    // 绘制背景层（轴、刻度线、刻度标签、标题）
    // plotArea 是绘图区矩形（像素坐标），轴根据自身的 alignment 决定画在哪条边上
    virtual void draw(QPainter* painter, 
        const QRectF& plotArea,
        const QChartProjection* projection,
        qreal offset = 0,
        bool drawAxisLine = true,
        bool drawLabels = true,
        bool drawTicks = true) const;

    // ---------- 样式 ----------
    bool isVisible() const { return m_visible; }
    void setVisible(bool v) {
        if (m_visible == v) return;
        m_visible = v;
        emit visibleChanged();
    }
    QString title() const { return m_title; }
    void setTitle(const QString& t) { m_title = t; }
    QColor color() const { return m_color; }
    void setColor(const QColor& c) { m_color = c; }

    // ---------- 坐标系标识 ---------
    virtual CoordinateSystem coordinateSystem() const = 0;

    // ---------- 边距/对齐（笛卡尔专用，极轴可忽略）----------
    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment alignment) { m_alignment = alignment; }

    virtual bool isAlignmentValid(Qt::Alignment alignment) const;

signals:
    void rangeChanged(qreal min, qreal max);
    void visibleChanged();
    void tickCountChanged();
    void subTickCountChanged();

protected:
    // 基础成员
    qreal m_min = 0.0;
    qreal m_max = 10.0;
    int m_tickCount = 5;
    int m_subTickCount = 0;
    bool m_visible = true;
    bool m_panEnabled = true;       // 默认允许拖动，子类可修改
    bool m_zoomEnabled = true;       // 默认允许缩放，子类可修改

    QString m_title;
    QColor m_color = Qt::black;

    // 对齐方式：用于笛卡尔轴(左/右/上/下)，Radial/Angle轴可忽略
    Qt::Alignment m_alignment = Qt::AlignBottom;

    static constexpr qreal AXIS_MARGIN = 8.0;  // 轴线到文字外侧的总边距
    static constexpr qreal TICK_LENGTH = 4.0;  // 主刻度线长度
    static constexpr qreal SUB_TICK_LENGTH = 2.0;
    static constexpr qreal TEXT_PADDING = 3.0; // 文字周围的微小呼吸空间
};

#endif // QCHARTAXIS_H