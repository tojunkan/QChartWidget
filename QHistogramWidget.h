#ifndef QHISTOGRAMWIDGET_H
#define QHISTOGRAMWIDGET_H
#include "QBarWidget.h"
#include <QVector>

class QHistogramWidget : public QBarWidget {
    Q_OBJECT
public:
    explicit QHistogramWidget(QWidget* p=nullptr);
    void setRawData(const QVector<qreal>& data);
    void setHorizontal(bool on);
    bool isHorizontal() const { return m_horizontal; }
    // curve overlay (Widget 层)
    void setDensityCurveVisible(bool v);
    void setNormalCurveVisible(bool v);
    void setBinCountHint(int n) { m_binCountHint=n; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    void drawCurves(QPainter* p);
    QVector<qreal> m_rawData;
    bool m_horizontal=false;
    bool m_densityVisible=false, m_normalVisible=false;
    int m_binCountHint=0;
    QColor m_densityColor=Qt::red, m_normalColor=Qt::blue;
};
#endif
