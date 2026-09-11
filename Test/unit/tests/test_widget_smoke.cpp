// test_widget_smoke.cpp —— 批次 A widget 容器化冒烟（offscreen CPU）
// 覆盖：QChartWidget 纯容器（无相机/buildScene/图例导出）→ addAxis/addLayer →
//       plotArea 扣除边框轴外边距 + plotAreaChanged/projectionChanged 广播 →
//       CPU paintEvent 全量 QPainter 出图（网格/边框轴真实像素）。
#include "test_widget_smoke.h"

#include <QtTest>
#include <QSignalSpy>
#include <QImage>
#include <QColor>
#include <cmath>

#include "QChartWidget.h"
#include "QChartLayer.h"
#include "QValueAxis.h"
#include "QPainterChartRenderer.h"
#include "QCartesianProjection.h"

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
int inkAll(const QImage& img)
{
    return inkIn(img, QRect(0, 0, img.width(), img.height()));
}

// ===== 批次2 A：网格脊单标签夹具（layer + 双轴 + 投影/相机/plotArea 场景上下文）=====
struct GridFixture {
    QChartLayer layer;
    QValueAxis ax, ay;
    QCartesianProjection proj;
    QChartScene scene;      // collect 后拷贝（camera 指向 layer 值成员，layer 存活期有效）

    GridFixture()
        : ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft)
    {
        ax.setRange(-10, 10); ax.setTickCount(5); ax.setColor(Qt::black);
        ay.setRange(-10, 10); ay.setTickCount(5); ay.setColor(Qt::black);
        layer.setAxisX(&ax);
        layer.setAxisY(&ay);
        layer.setGridVisible(true);
        layer.setGridColor(QColor(120, 120, 120));
        // legacy 取向矩形：left/right=dim0 极值、bottom/top=dim1 极值（bottom<=top 才同步语法糖范围）
        layer.setNumericBounds(QRectF(-10, 10, 20, -20));
        layer.camera()->setViewRect(QRectF(-10, -10, 20, 20));
        layer.setScenePlotArea(QRectF(0, 0, 400, 400));
        layer.setSceneProjection(&proj);
    }

    void collect()
    {
        layer.collectPrimitives();
        scene = layer.scene();
    }

    QImage render()
    {
        QImage img(400, 400, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        QPainterChartRenderer r;
        r.render(scene, &img);
        return img;
    }
};

/// 策略探针：全部脊不出标签 / 保留逐刻度标签（验证"图层决定哪些脊使用单标签模式"）
class NoneLabelLayer : public QChartLayer {
public:
    using QChartLayer::QChartLayer;
protected:
    QChartAxis::LabelMode gridSpineLabelMode() const override { return QChartAxis::LabelMode::None; }
};
class TickwiseLabelLayer : public QChartLayer {
public:
    using QChartLayer::QChartLayer;
protected:
    QChartAxis::LabelMode gridSpineLabelMode() const override { return QChartAxis::LabelMode::Tickwise; }
};
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

// ===== 批次2 A：网格脊单标签（数量=脊数、自由标签契约、值轴文字来源、组尾定位）=====
void TestWidgetSmoke::gridSingleLabelPerSpine()
{
    GridFixture f;
    f.collect();

    const QVector<qreal> ticksY = f.ay.tickValues(-10.0, 10.0);   // 水平脊（固定 y 值轴）
    const QVector<qreal> ticksX = f.ax.tickValues(-10.0, 10.0);   // 垂直脊（固定 x 值轴）
    const QStringList labelsY = f.ay.tickLabels(ticksY);
    const QStringList labelsX = f.ax.tickLabels(ticksX);
    QVERIFY2(ticksY.size() >= 2 && ticksX.size() >= 2, "双轴应有刻度");

    // ① 标签数量恰等于网格脊数量（每脊 1 个，而非每刻度 1 个）
    const int spineCount = ticksY.size() + ticksX.size();
    QCOMPARE(f.scene.labels.size(), spineCount);

    // ② 自由标签契约：坐标全 NaN、不指向图元、sourceId = 本脊组号（组号按脊序 1..N）
    for (int i = 0; i < f.scene.labels.size(); ++i) {
        const QChartTextLabel& l = f.scene.labels[i];
        QVERIFY2(std::isnan(l.numericAnchor.x()) && std::isnan(l.numericAnchor.y())
                     && std::isnan(l.numericAnchor.z()),
                 "网格脊标签必须是自由标签（numericAnchor 全 NaN）");
        QCOMPARE(l.refPrimitiveId, -1);
        QVERIFY2(l.sourceId >= 1, "网格脊标签应以本脊组号定位");
        QCOMPARE(l.sourceId, i + 1);
    }

    // ③ 文字来源：前 ticksY.size() 个（水平脊）= y 轴文字；其后（垂直脊）= x 轴文字
    for (int i = 0; i < ticksY.size(); ++i)
        QCOMPARE(f.scene.labels[i].text, labelsY.value(i));
    for (int j = 0; j < ticksX.size(); ++j)
        QCOMPARE(f.scene.labels[ticksY.size() + j].text, labelsX.value(j));

    // ④ 来源强证：改 y 轴 labelFormat → 水平脊文字随之变化、垂直脊不受影响
    {
        GridFixture f2;
        f2.ay.setLabelFormat("y=%g");
        f2.collect();
        QCOMPARE(f2.scene.labels.size(), spineCount);
        for (int i = 0; i < ticksY.size(); ++i)
            QVERIFY2(f2.scene.labels[i].text.startsWith("y="),
                     "水平网格脊文字应来自 y 轴（固定坐标所属轴）");
        for (int j = 0; j < ticksX.size(); ++j)
            QVERIFY2(!f2.scene.labels[ticksY.size() + j].text.startsWith("y="),
                     "垂直网格脊文字应来自 x 轴");
    }

    // ⑤ 组尾定位：renderer 步骤 2 把自由标签锚到"同 sourceId 组尾最后可见图元"
    const QImage img = f.render();
    for (const QChartTextLabel& l : f.scene.labels) {
        int tail = -1;
        for (int i = 0; i < f.scene.primitives.size(); ++i)
            if (f.scene.primitives[i].sourceId == l.sourceId) tail = i;
        QVERIFY2(tail >= 0, "每条网格脊应有图元组");
        QVERIFY2(l.visible, "网格脊标签应可见（组内存在可见图元）");
        QVERIFY2(l.cartesianAnchor.x() == f.scene.primitives[tail].cartA.x()
                     && l.cartesianAnchor.y() == f.scene.primitives[tail].cartA.y(),
                 "自由标签锚点应等于本组组尾图元 cartA");
    }

    // ⑥ 标签真实出墨：同一场景去掉标签后墨迹更少
    QChartScene noLabel = f.scene;
    noLabel.labels.clear();
    QImage imgNo(400, 400, QImage::Format_ARGB32_Premultiplied);
    imgNo.fill(Qt::white);
    QPainterChartRenderer r2;
    r2.render(noLabel, &imgNo);
    QVERIFY2(inkAll(img) > inkAll(imgNo) + 20,
             qPrintable(QString("网格脊标签应真实出墨（labeled=%1 unlabeled=%2）")
                        .arg(inkAll(img)).arg(inkAll(imgNo))));
}

// ===== 批次2 A：图层决定哪些脊使用单标签模式（策略钩子）=====
void TestWidgetSmoke::gridSpineLabelModePolicy()
{
    const auto countLabels = [](QChartLayer& layer, QValueAxis& ax, QValueAxis& ay,
                                QCartesianProjection& proj) {
        layer.setAxisX(&ax);
        layer.setAxisY(&ay);
        layer.setGridVisible(true);
        layer.setNumericBounds(QRectF(-10, 10, 20, -20));
        layer.setScenePlotArea(QRectF(0, 0, 400, 400));
        layer.setSceneProjection(&proj);
        layer.collectPrimitives();
        return layer.scene().labels.size();
    };

    QValueAxis ax1(nullptr, Qt::AlignBottom), ay1(nullptr, Qt::AlignLeft);
    QValueAxis ax2(nullptr, Qt::AlignBottom), ay2(nullptr, Qt::AlignLeft);
    QValueAxis ax3(nullptr, Qt::AlignBottom), ay3(nullptr, Qt::AlignLeft);
    for (QValueAxis* a : {&ax1, &ax2, &ax3}) { a->setRange(-10, 10); a->setTickCount(5); }
    for (QValueAxis* a : {&ay1, &ay2, &ay3}) { a->setRange(-10, 10); a->setTickCount(5); }

    const int ticksPerSpine = ay1.tickValues(-10.0, 10.0).size();
    const int spineCount = ay1.tickValues(-10.0, 10.0).size() + ax1.tickValues(-10.0, 10.0).size();

    QChartLayer singleLayer;                 // 默认策略 = Single（每脊 1 个）
    QCartesianProjection p1;
    QCOMPARE(countLabels(singleLayer, ax1, ay1, p1), spineCount);

    NoneLabelLayer noneLayer;                // 覆写为 None（图层决定不出标签）
    QCartesianProjection p2;
    QCOMPARE(countLabels(noneLayer, ax2, ay2, p2), 0);

    TickwiseLabelLayer tickwiseLayer;        // 覆写为 Tickwise（保留旧"每脊每刻度"语义）
    QCartesianProjection p3;
    QCOMPARE(countLabels(tickwiseLayer, ax3, ay3, p3), spineCount * ticksPerSpine);
}
