#ifndef QCHARTWIDGET_H
#define QCHARTWIDGET_H
#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include "QChartGeometry.h"

class QChartWidget : public QWidget {
    Q_OBJECT
        Q_PROPERTY(bool panEnabled READ isPanEnabled WRITE setPanEnabled)
        Q_PROPERTY(bool zoomEnabled READ isZoomEnabled WRITE setZoomEnabled)
        Q_PROPERTY(bool cachingEnabled READ isCachingEnabled WRITE setCachingEnabled)
public:
    explicit QChartWidget(QWidget* p=nullptr);
    ~QChartWidget() override;

    void addGeometry(QChartGeometry* g);
    void removeGeometry(QChartGeometry* g);
    QList<QChartGeometry*> geometries() const { return m_geometries; }

    //由于每个Widget只有一个Area，所以Axis的添加、绘制等显示层的任务只需要在Widget层进行即可；
    //Geometry必须绑定若干Axis才能成立，Axis的数据会传给Geometry，并由Geometry重算坐标以后映射给Series。
    void addAxis(QChartAxis* a);
    void removeAxis(QChartAxis* a);
    QList<QChartAxis*> axes() const { return m_axes; }

    //每个Widget只有一个Area，但可以有很多个Geometry做不同的图层
    QRectF plotArea() const { return m_plotArea; }

    bool isCachingEnabled() const { return m_cachingEnabled; }
    void setCachingEnabled(bool v) { m_cachingEnabled=v; update(); }
    //设置拖拽
    bool isPanEnabled() const { return m_panEnabled; }
    void setPanEnabled(bool v) { m_panEnabled=v; }
    //设置滚轮
    bool isZoomEnabled() const { return m_zoomEnabled; }
    void setZoomEnabled(bool v) { m_zoomEnabled=v; }

    void invalidateBackground();
    void invalidateForeground();

signals:
    void seriesHovered(QChartSeries*,int,bool);
    void seriesClicked(QChartSeries*,int);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;

    virtual void layoutAxes();
    virtual void drawBackground(QPainter* p);
    virtual void drawForeground(QPainter* p);

    QList<QChartGeometry*> m_geometries;
    QList<QChartAxis*> m_axes;
    QRectF m_plotArea;
    QPixmap m_bgCache, m_fgCache;
    bool m_bgDirty = true, m_fgDirty = true, m_cachingEnabled = true;
    bool m_panEnabled = true, m_zoomEnabled = true;
    QPointF m_panStart; 
    bool m_panning = false;
    QChartSeries* m_hoverSeries = nullptr;
    int m_hoverIndex = -1;
};

#endif
