#ifndef QHISTOGRAMWIDGET_H
#define QHISTOGRAMWIDGET_H

#include "QBarWidget.h"

class QHistogramWidget : public QBarWidget
{
    Q_OBJECT
public:
    enum BinMethod { Sturges, FreedmanDiaconis, Scott, Fixed };

    explicit QHistogramWidget(QWidget* parent = nullptr);

    // 输入原始数据，自动分箱
    void setRawData(const QVector<qreal>& data);
    QVector<qreal> rawData() const { return m_rawData; }

    // 分箱控制
    void setBinCount(int n);   // 0 = auto
    int binCount() const { return m_binCount; }
    void setBinWidth(qreal w); // 0 = auto
    qreal binWidth() const;
    void setBinMethod(BinMethod m);
    void computeBins();

    // 曲线叠加
    void setDensityCurveVisible(bool v) { m_densityVisible = v; update(); }
    bool isDensityCurveVisible() const { return m_densityVisible; }
    void setNormalCurveVisible(bool v) { m_normalVisible = v; update(); }
    bool isNormalCurveVisible() const { return m_normalVisible; }
    void setDensityCurveColor(const QColor& c) { m_densityColor = c; update(); }
    void setNormalCurveColor(const QColor& c) { m_normalColor = c; update(); }

protected:
    void paintOverlay(QPainter* p, const QRectF& plotArea) override;

private:
    QVector<qreal> computeBinEdges() const;
    QVector<qreal> computeBinCenters() const;
    QVector<qreal> computeDensity() const;
    void drawCurve(QPainter* p, const QRectF& area,
                   const QVector<QPointF>& points, const QColor& color);

    QVector<qreal> m_rawData;
    int m_binCount = 0;
    qreal m_binWidth = 0;
    BinMethod m_binMethod = FreedmanDiaconis;

    bool m_densityVisible = false;
    bool m_normalVisible = false;
    QColor m_densityColor = Qt::red;
    QColor m_normalColor = Qt::blue;
};

#endif // QHISTOGRAMWIDGET_H
