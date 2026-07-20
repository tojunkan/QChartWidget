#ifndef QPIEWIDGET_H
#define QPIEWIDGET_H

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QString>
#include <QPropertyAnimation>
#include <QPointer>

class QPieWidget : public QWidget
{
    Q_OBJECT
        //Q_PROPERTY(qreal explodeOffset READ explodeOffset WRITE setExplodeOffset)

public:
    explicit QPieWidget(QWidget* parent = nullptr);
    ~QPieWidget();

    // -------- 数据管理 --------
    void appendSlice(const QString& label, qreal value, const QColor& color = QColor());
    void insertSlice(int index, const QString& label, qreal value, const QColor& color = QColor());
    void removeSlice(int index);
    void clearSlices();
    int count() const { return m_slices.size(); }
    QPair<QString, qreal> sliceData(int index) const;
    void setSliceValue(int index, qreal value);
    void setSliceLabel(int index, const QString& label);

    // -------- 样式 --------
    void setHoleSize(qreal size);               // 0 ~ 0.99
    qreal holeSize() const { return m_holeSize; }
    void setStartAngle(qreal angle);            // 单位：度，0 为 12 点钟方向
    qreal startAngle() const { return m_startAngle; }
    void setPieSize(qreal relativeSize);        // 0~1，相对 widget 短边的比例
    qreal pieSize() const { return m_pieSize; }
    void setPiePosition(qreal x, qreal y);      // 0~1，圆心相对 widget 的位置
    void setAllLabelsVisible(bool visible);
    void setAllLabelsPosition(int position);    // 0: InsideHorizontal, 1: Outside
    void setAllBorderWidth(int width);
    void setAllBorderColor(const QColor& color);
    void setPalette(const QList<QColor>& colors);
    void setSliceExploded(int index, bool exploded);
    void setSliceExplodeDistanceFactor(int index, qreal factor); // 0~1，爆炸距离占饼图半径的比例
    void setAutoColorEnabled(bool enabled);     // 是否自动配色，默认 true
    void setTooltipEnabled(bool enabled);       // 是否显示悬停提示，默认 true
    QList<QColor> palette() const { return m_palette; }

signals:
    void countChanged(int newCount);
    void sliceAdded(int index);
    void sliceRemoved(int index);
    void sliceValueChanged(int index, qreal newValue);
    void sliceLabelChanged(int index, const QString& newLabel);
    void sliceClicked(int index);
    void slicePressed(int index);
    void sliceReleased(int index);
    void sliceHovered(int index, bool hovered);  // 悬停进入/离开

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onExplodeAnimationFinished();

private:
    struct Slice {
        QString label;
        qreal value;
        QColor color;
        bool exploded = false;
        qreal explodeTarget = 0.0;          // 目标爆炸因子 (0~1)
        qreal explodeOffset = 0.0;          // 当前插值偏移因子
        qreal explodeFactor = 0.15;         // 每切片爆炸距离比例
        QPointer<QVariantAnimation> anim;  // 动画指针，自动销毁
    };

    QVector<Slice> m_slices;
    qreal m_holeSize = 0.0;
    qreal m_startAngle = 90.0;              // 默认从顶部开始（12 点方向）
    qreal m_pieSize = 0.8;
    qreal m_pieX = 0.5, m_pieY = 0.5;       // 圆心位置比例
    bool m_labelsVisible = true;
    int m_labelPosition = 0;                // 0: inside, 1: outside
    int m_borderWidth = 0;
    QColor m_borderColor = Qt::black;
    QList<QColor> m_palette;                // 调色板（空则用默认）
    bool m_autoColor = true;                // 是否自动配色
    bool m_tooltipEnabled = true;           // 是否显示悬停提示
    int m_hoverIndex = -1;                  // 当前悬停切片索引，-1 表示无
    int m_pressedIndex = -1;

    // 辅助方法
    QRectF pieRect() const;                 // 饼图外接矩形（在 widget 坐标系）
    QPointF pieCenter() const;
    qreal pieRadius() const;
    qreal innerRadius() const;
    // 核心绘制函数：绘制单个切片
    void drawSlice(QPainter* painter, const Slice& slice,
        qreal startAngle, qreal spanAngle,
        const QPointF& center, qreal radius, qreal innerRadius,
        bool drawLabel, int labelPos, int borderWidth, const QColor& borderColor,
        qreal totalValue);
    int sliceAtPos(const QPointF& pos) const; // 返回鼠标位置所在的切片索引，-1 表示无
    void updateExplodeAnimation(int index, bool on);
    void updateAllExplodeOffsets();         // 初始化所有偏移为0
};

#endif // QPIEWIDGET_H