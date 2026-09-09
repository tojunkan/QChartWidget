// test_widget_smoke.cpp —— 批次 A widget 容器化冒烟（offscreen CPU）
// 覆盖：QChartWidget 纯容器（无相机/buildScene/图例导出）→ addAxis/addLayer →
//       plotArea 扣除边框轴外边距 + plotAreaChanged/projectionChanged 广播 →
//       CPU paintEvent 全量 QPainter 出图（网格/边框轴真实像素）。
#include "test_widget_smoke.h"

#include <QtTest>
#include <QSignalSpy>
#include <QImage>
#include <QColor>

#include "QChartWidget.h"
#include "QChartLayer.h"
#include "QValueAxis.h"

namespace {
bool isInk(const QColor& c)
{
    // 网格默认色 (220,220,220)：离白 35 → 阈值 25 使浅灰网格也算墨
    return qAbs(c.red() - 255) > 25 || qAbs(c.green() - 255) > 25 || qAbs(c.blue() - 255) > 25;
}
int inkIn(const QImage& img, const QRect& r)
{
    int n = 0;
    const int x0 = qMax(0, r.left()), y0 = qMax(0, r.top());
    const int x1 = qMin(img.width() - 1, r.right()), y1 = qMin(img.height() - 1, r.bottom());
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}
} // namespace

void TestWidgetSmoke::cpuContainerRenders()
{
    QChartWidget w;
    QValueAxis ax(nullptr, Qt::AlignBottom);
    QValueAxis ay(nullptr, Qt::AlignLeft);
    ax.setRange(-10, 10); ax.setTickCount(5); ax.setColor(Qt::black);
    ay.setRange(-10, 10); ay.setTickCount(5); ay.setColor(Qt::black);

    QChartLayer layer;                      // 2D layer（自带相机值成员；批次 A 起可实例化）
    layer.setGridVisible(true);
    w.addAxis(&ax);
    w.addAxis(&ay);
    w.addLayer(&layer);

    // 广播观察（plotAreaChanged / projectionChanged 保留）
    QSignalSpy plotSpy(&w, &QChartWidget::plotAreaChanged);
    QSignalSpy projSpy(&w, &QChartWidget::projectionChanged);

    w.resize(420, 340);
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "offscreen 下窗口应暴露");
    QTest::qWait(30);

    QVERIFY2(plotSpy.count() >= 1, "首次布局应广播 plotAreaChanged");
    QCOMPARE(projSpy.count(), 0);           // 未换投影

    const QRectF pa = w.plotArea();
    QVERIFY2(pa.width() > 100 && pa.height() > 100, "plotArea 应有效");
    // 左边距：Y 边框轴（AlignLeft）sizeHint 宽度再叠加；下边距：X 边框轴高度叠加；
    // 上/右无边框轴 → 仅基础边距
    QVERIFY2(pa.left() > w.marginLeft() + 5, "左边距应含 Y 边框轴 sizeHint 占用");
    QVERIFY2(pa.bottom() < w.height() - w.marginBottom() - 1,
             "下边距应含 X 边框轴 sizeHint 占用");
    QVERIFY2(pa.top() >= w.marginTop() - 1 && pa.right() <= w.width() - w.marginRight() + 1,
             "plotArea 应扣除基础边距");

    // CPU 出图取证（paintEvent 全量 QPainter；widget.grab 触发渲染）
    // ★ HiDPI：QWidget::grab() 返回设备像素图（宽高 = 逻辑 × devicePixelRatio），
    //   尺寸断言与采样矩形必须按 s = img/逻辑 缩放（DPR=1 时 s=1，行为与历史一致）。
    const QImage img = w.grab().toImage();
    const qreal s = qreal(img.width()) / w.width();
    QVERIFY2(qAbs(s - w.devicePixelRatioF()) < 0.01,
             "grab 尺寸应等于 widget（设备像素 = 逻辑 × dpr）");
    auto dprRect = [s](const QRect& r) {
        return QRect(qRound(r.left() * s), qRound(r.top() * s),
                     qRound(r.width() * s), qRound(r.height() * s));
    };

    // 网格真实落屏：轴域中心（X/Y 网格脊在 numeric 0 处交叉）
    const QPoint center(qRound(pa.center().x()), qRound(pa.center().y()));
    QVERIFY2(inkIn(img, dprRect(QRect(center.x() - 2, center.y() - 2, 5, 5))) > 0,
             "plotArea 中心应有网格墨迹（轴脊交叠）");

    // 外部边框轴真实出图：左/下边距带内应有轴墨（drawAtEdge 标签/刻度/轴线）
    const QRect leftMargin(1, qRound(pa.top()) + 10, qMax(1, qRound(pa.left()) - 2), 40);
    const QRect bottomMargin(qRound(pa.left()) + 10, qRound(pa.bottom()) + 1,
                             qMax(1, qRound(pa.width()) - 20),
                             qMax(1, w.height() - qRound(pa.bottom()) - 2));
    QVERIFY2(inkIn(img, dprRect(leftMargin)) > 0, "左边距带应有 Y 边框轴墨迹");
    QVERIFY2(inkIn(img, dprRect(bottomMargin)) > 0, "下边距带应有 X 边框轴墨迹");

    // 批次 B3：外部边距墨迹带——仅绑定侧（下/左）有边框轴墨迹；未绑定侧（上/右）应为空
    // 未绑定侧（上/右）纯边距带：应无任何墨迹
    const QRect topBand(qRound(pa.left()), 0,
                        qMax(1, qRound(pa.width())), qMax(0, qRound(pa.top())));
    const QRect rightBand(qRound(pa.right()) + 1, 0,
                          qMax(0, img.width() - qRound(pa.right()) - 1), img.height());
    QVERIFY2(inkIn(img, dprRect(topBand)) == 0,
             "上侧边距（无边框轴绑定）应空白");
    QVERIFY2(inkIn(img, dprRect(rightBand)) == 0,
             "右侧边距（无边框轴绑定）应空白");

    // plotArea 内非空（网格+标签整体）
    QVERIFY2(inkIn(img, dprRect(pa.toRect().adjusted(4, 4, -4, -4))) > 200,
             "plotArea 内应产生大量墨迹（网格）");
}
