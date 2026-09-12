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

#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>

#include "QChartWidget.h"
#include "QChartLayer.h"
#include "QValueAxis.h"
#include "QPainterChartRenderer.h"
#include "QCartesianProjection.h"
#include "QPolarProjection.h"   // 4b：非线性投影近似用例

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
// ===== 4f：合成鼠标/滚轮事件注入（直接投递到 widget，走基类 final 分发 → 交互开关 → 钩子）=====
void sendMouse(QWidget* w, QEvent::Type type, const QPointF& pos, Qt::MouseButton button)
{
    const Qt::MouseButtons buttons = (type == QEvent::MouseMove) ? Qt::NoButton : button;
    QMouseEvent ev(type, pos, w->mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &ev);
}
void sendWheel(QWidget* w, const QPointF& pos, int deltaY)
{
    QWheelEvent ev(pos, w->mapToGlobal(pos), QPoint(0, 0), QPoint(0, deltaY),
                   Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(w, &ev);
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
        // 4b：范围由轴持有（上方 ax/ay 已 setRange(-10,10)），无需再经图层写范围
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
        // 4b：范围由轴持有（ax/ay 由调用方 setRange），无需再经图层写范围
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

// ===== 4b：轴=范围唯一持有者 + dataBounds 按需组装（数值断言，与 4a 基线一致）=====
void TestWidgetSmoke::axisRangeAndDataBoundsContract()
{
    // ① setRange = 真实状态（不再是语法糖；min/max 直读）
    {
        QValueAxis axis(nullptr, Qt::AlignBottom);
        axis.setRange(3.0, 7.0);
        QCOMPARE(axis.min(), 3.0);
        QCOMPARE(axis.max(), 7.0);
        axis.setRange(-2.0, 5.0);
        QCOMPARE(axis.min(), -2.0);
        QCOMPARE(axis.max(), 5.0);
    }

    // ② 线性（Cartesian）：相机窗口变化 → 轴范围更新为可见范围；dataBounds() 组装值 == 旧字段值 4a 基线
    {
        QChartWidget w;
        QValueAxis ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft);
        ax.setRange(-10, 10); ay.setRange(-10, 10); ax.setTickCount(5); ay.setTickCount(5);
        QChartLayer layer;
        w.addAxis(&ax); w.addAxis(&ay); w.addLayer(&layer);

        w.setViewRect(QRectF(-10, -10, 20, 20));
        QCOMPARE(ax.min(), -10.0); QCOMPARE(ax.max(), 10.0);
        QCOMPARE(ay.min(), -10.0); QCOMPARE(ay.max(), 10.0);
        // 4a 基线硬编码值：legacy 取向 (left=-10, top=10, w=20, h=-20)
        QCOMPARE(w.dataBounds(), QRectF(-10, 10, 20, -20));

        w.setViewRect(QRectF(-5, -5, 10, 10));    // 缩放：轴范围更新为新的可见范围
        QCOMPARE(ax.min(), -5.0); QCOMPARE(ax.max(), 5.0);
        QCOMPARE(ay.min(), -5.0); QCOMPARE(ay.max(), 5.0);
        QCOMPARE(w.dataBounds(), QRectF(-5, 5, 10, -10));

        w.panViewCartesian(2.0, 3.0);             // 平移：轴范围/组装值同步跟随
        const QRectF legacy = w.dataBounds();
        QCOMPARE(ax.min(), legacy.left());  QCOMPARE(ax.max(), legacy.right());
        QCOMPARE(ay.min(), legacy.bottom()); QCOMPARE(ay.max(), legacy.top());
        qInfo().noquote() << QString("4b 2D: viewRect=%1 → 轴=(%2..%3, %4..%5) dataBounds=%6")
                                 .arg(QString("(-3,-2 10x10)")).arg(ax.min()).arg(ax.max())
                                 .arg(ay.min()).arg(ay.max())
                                 .arg(QString("(%1,%2 %3x%4)").arg(legacy.left()).arg(legacy.top())
                                          .arg(legacy.width()).arg(legacy.height()));
    }

    // ③ 无轴（无层轴绑定）：dataBounds() 按需组装 → 空（无长期缓存可回退，证明"无人持有"）
    {
        QChartWidget w;
        QChartLayer bare;
        w.addLayer(&bare);
        QVERIFY2(w.dataBounds().isEmpty(), "无轴时 dataBounds() 应为空（按需组装语义）");
    }

    // ④ 非线性（Polar）投影近似：轴范围/组装值 == 旧链同算式（computeDataBounds → legacy 取向）
    {
        QChartWidget w;
        QValueAxis ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft);
        QChartLayer layer;
        w.addAxis(&ax); w.addAxis(&ay); w.addLayer(&layer);
        w.setProjection(std::make_unique<QPolarProjection>());

        const QRectF vr(-10, -10, 20, 20);
        w.setViewRect(vr);
        QPolarProjection refProj;                 // 与 widget 内同一投影类型 → 同算式期望值
        const QRectF math = refProj.computeDataBounds(vr);
        QRectF exp;
        exp.setLeft(math.left());
        exp.setRight(math.left() + math.width());
        exp.setBottom(math.top());
        exp.setTop(math.top() + math.height());
        QCOMPARE(ax.min(), exp.left());  QCOMPARE(ax.max(), exp.right());
        QCOMPARE(ay.min(), exp.bottom()); QCOMPARE(ay.max(), exp.top());
        QCOMPARE(w.dataBounds(), exp);
        qInfo().noquote() << QString("4b Polar(非线性近似): 轴=(%1..%2, %3..%4) dataBounds=(%5,%6 %7x%8)")
                                 .arg(ax.min()).arg(ax.max()).arg(ay.min()).arg(ay.max())
                                 .arg(exp.left()).arg(exp.top()).arg(exp.width()).arg(exp.height());
    }
}

// ===== 4e：三向驱动链契约（方向状态取代一次性闩锁 / 计数防回环 / 幂等 / 极端输入 / 像素侧重 fit）=====
void TestWidgetSmoke::driveChainContract()
{
    // ===== ① 数值侧驱动：首帧后改轴范围**仍会**重新 fit（修掉一次性闩锁语义）=====
    QChartWidget w;
    QValueAxis ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft);
    QChartLayer layer;
    ax.setRange(-10, 10); ay.setRange(-10, 10);
    QVERIFY2(w.isNumericDirty(), "初始应为数值侧待 fit（绑定变化即置脏）");
    w.addAxis(&ax); w.addAxis(&ay); w.addLayer(&layer);
    w.resize(420, 340);
    QVERIFY2(w.isNumericDirty(), "首次渲染前仍待 fit（布局不消费脏标记）");
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "offscreen 下窗口应暴露");
    w.grab();                                        // 真实渲染路径（paintEvent → onBeforePaint 收口）
    QVERIFY2(!w.isNumericDirty() && !w.isCameraDirty(), "首帧后两方向均已消费");
    QVERIFY2(w.fitCount() >= 1, "首帧应发生数值侧 fit");
    w.resetDriveCounters();                          // 从稳态开始计数

    w.grab();                                        // 重复同状态 → 不产生额外重算（幂等）
    QCOMPARE(w.fitCount(), 0);
    QCOMPARE(w.backCalcCount(), 0);

    ax.setRange(0.0, 100.0);                         // ★核心修复：首帧后改轴范围必须重新 fit
    QVERIFY2(w.isNumericDirty(), "轴 setRange 应置数值侧脏");
    w.grab();
    QCOMPARE(w.fitCount(), 1);                       // 数值侧变化 → fit 恰 1 次
    QCOMPARE(w.backCalcCount(), 1);                  // 并以实际可见范围回写轴
    QCOMPARE(w.viewRect(), QRectF(0.0, -10.0, 100.0, 20.0));   // 相机窗口跟随新轴范围（Cartesian 恒等映射）
    QCOMPARE(ax.min(), 0.0); QCOMPARE(ax.max(), 100.0);

    // ===== ③ 相机侧驱动：只反算，绝不 fit（防回环）=====
    const int fit0 = w.fitCount(), back0 = w.backCalcCount();
    w.setViewRect(QRectF(-5, -5, 10, 10));
    QCOMPARE(w.backCalcCount(), back0 + 1);
    QCOMPARE(w.fitCount(), fit0);                    // 相机变化 → fit 0 次
    QCOMPARE(ax.min(), -5.0); QCOMPARE(ax.max(), 5.0);
    QVERIFY2(!w.isNumericDirty() && !w.isCameraDirty(), "反算即消费相机侧脏并覆盖数值侧请求");
    w.grab();
    QCOMPARE(w.fitCount(), fit0);                    // 渲染不产生额外 fit（无回环）
    QCOMPARE(w.backCalcCount(), back0 + 1);

    w.panViewCartesian(1.0, 1.0);                    // 平移（相机侧）→ 反算 +1、fit 不变
    QCOMPARE(w.fitCount(), fit0);
    QCOMPARE(w.backCalcCount(), back0 + 2);
    w.zoomViewCartesian(0.0, 0.0, 0.5, 0.5);         // 缩放（相机侧）→ 同上
    QCOMPARE(w.fitCount(), fit0);
    QCOMPARE(w.backCalcCount(), back0 + 3);

    ax.setRange(-50.0, 50.0);                        // 数值侧请求…
    QVERIFY(w.isNumericDirty());
    w.setViewRect(QRectF(-2, -2, 4, 4));             // …被相机侧操作覆盖（相机=真值来源）
    QVERIFY2(!w.isNumericDirty(), "相机侧操作应覆盖待 fit 请求");
    w.grab();
    QCOMPARE(w.fitCount(), fit0);                    // 不再补做那次 fit

    // ===== ⑤ 极端输入：退化范围不 fit、保持相机窗口 =====
    ax.setRange(7.0, 7.0);                           // 退化范围（min == max）
    const QRectF vrKeep = w.viewRect();
    const int fitKeep = w.fitCount();
    w.grab();
    QCOMPARE(w.fitCount(), fitKeep);
    QCOMPARE(w.viewRect(), vrKeep);
    ax.setRange(-10.0, 10.0);
    w.grab();                                        // 恢复（并重新 fit 一次）
    QCOMPARE(w.fitCount(), fitKeep + 1);

    // 零尺寸 plotArea：二维数值侧 fit 不依赖 plotArea（plotArea 只参与投影矩阵/像素映射）→ 仍 fit
    {
        QChartWidget w2;
        QValueAxis bx(nullptr, Qt::AlignBottom), by(nullptr, Qt::AlignLeft);
        QChartLayer l2;
        bx.setRange(-3, 3); by.setRange(-2, 2);
        w2.addAxis(&bx); w2.addAxis(&by); w2.addLayer(&l2);
        w2.resetDriveCounters();
        w2.grab();
        QCOMPARE(w2.fitCount(), 1);
        QCOMPARE(w2.viewRect(), QRectF(-3.0, -2.0, 6.0, 4.0));
    }

    // ===== ④ 像素侧驱动：resize → plotArea 变化 → 相机按 fit 模式适配（Stretch 零变化 / Expand 适配+反算）=====
    {
        QChartWidget w3;
        QValueAxis cx(nullptr, Qt::AlignBottom), cy(nullptr, Qt::AlignLeft);
        QChartLayer l3;
        cx.setRange(-10, 10); cy.setRange(-10, 10);
        w3.addAxis(&cx); w3.addAxis(&cy); w3.addLayer(&l3);
        w3.resize(400, 300);
        w3.show();
        QVERIFY2(QTest::qWaitForWindowExposed(&w3), "offscreen 下窗口应暴露");
        w3.grab();
        const QRectF vrBefore = w3.viewRect();
        QVERIFY2(vrBefore.width() > 0.0, "稳态窗口应有效");

        // (a) Stretch（拉伸铺满；Cartesian 常态）→ 窗口不变、零额外重算
        l3.camera()->setFitMode(ViewRectFitMode::Stretch);
        w3.resetDriveCounters();
        w3.resize(700, 300);
        w3.relayout();
        QCOMPARE(w3.fitCount(), 0);
        QCOMPARE(w3.backCalcCount(), 0);
        QCOMPARE(w3.viewRect(), vrBefore);
        w3.grab();
        QCOMPARE(w3.fitCount(), 0);
        QCOMPARE(w3.backCalcCount(), 0);

        // (b) Expand → 窗口宽比适配新 plotArea，随即相机侧反算写轴（不触发数值侧 fit）
        l3.camera()->setFitMode(ViewRectFitMode::Expand);
        w3.resetDriveCounters();
        w3.resize(300, 700);
        w3.relayout();
        const QRectF pa = w3.plotArea();
        QVERIFY2(pa.width() > 0 && pa.height() > 0, "plotArea 应有效");
        QVERIFY2(w3.isCameraDirty() || w3.backCalcCount() >= 1, "Expand：窗口适配应转相机侧反算");
        QVERIFY2(qAbs(w3.viewRect().width() / w3.viewRect().height()
                      - pa.width() / pa.height()) < 1e-9,
                 qPrintable(QString("Expand 后窗口宽比应等于 plotArea 宽比：view=%1 plot=%2")
                                .arg(w3.viewRect().width() / w3.viewRect().height())
                                .arg(pa.width() / pa.height())));
        QCOMPARE(w3.fitCount(), 0);                  // 像素侧绝不触发数值侧 fit
        w3.grab();                                    // 收口（反算写轴）
        QCOMPARE(cx.min(), w3.viewRect().left());      // 轴由反算写回（Cartesian 恒等：X=left..right）
        QCOMPARE(cx.max(), w3.viewRect().right());
        QCOMPARE(cy.min(), w3.viewRect().top());       // legacy 取向：Y=top..bottom（数值增序）
        QCOMPARE(cy.max(), w3.viewRect().bottom());
        QVERIFY2(!w3.isNumericDirty() && !w3.isCameraDirty(), "收口后两方向都不脏");

        // 幂等：同尺寸重复 relayout → 不再有任何重算
        w3.resetDriveCounters();
        w3.relayout();
        QCOMPARE(w3.fitCount(), 0);
        QCOMPARE(w3.backCalcCount(), 0);
    }

    // ===== 4e（t51 补测）：非线性投影（Polar）驱动链——写轴回环必须被 m_axisWriteDepth 抑制 =====
    // 线性（Cartesian）下 fit 回写轴的值与入参逐位相同 → 不产生 rangeChanged，回环自然不显；
    // 非线性（Polar）下 fit 回写的是**实际可见范围**（与请求值不同）→ 若无抑制，写轴会再置数值侧脏，
    // 下一次渲染再 fit（计数递增：无 guard 时实测 fit=2/3）。本段用计数把“无回环”锁死。
    {
        QChartWidget wp;
        QValueAxis px(nullptr, Qt::AlignBottom), py(nullptr, Qt::AlignLeft);
        QChartLayer lp;
        px.setRange(0, 360); py.setRange(0, 10);
        wp.addAxis(&px); wp.addAxis(&py); wp.addLayer(&lp);
        wp.setProjection(std::make_unique<QPolarProjection>());
        wp.resize(420, 340);
        wp.show();
        QVERIFY2(QTest::qWaitForWindowExposed(&wp), "offscreen 下窗口应暴露");
        wp.grab();                                    // 首帧（数值侧 fit）
        wp.resetDriveCounters();

        px.setRange(10, 300);
        QVERIFY2(wp.isNumericDirty(), "Polar 下轴 setRange 应置数值侧脏");
        wp.grab();
        QCOMPARE(wp.fitCount(), 1);                   // 数值侧变化 → fit 恰 1 次
        QVERIFY2(qAbs(px.max() - 360.0) < 1e-6 && px.min() < 1.0,
                 qPrintable(QString("Polar：fit 后轴应为实际可见范围（非线性 → 与请求 10..300 不同），实为 %1..%2")
                                .arg(px.min()).arg(px.max())));
        wp.grab();                                    // 紧接重复渲染：若回环存在，这里 fit 会变成 2
        QCOMPARE(wp.fitCount(), 1);                   // ★ 防回环：不再增长
        QCOMPARE(wp.backCalcCount(), 1);

        // 第二组：同法再验（无 guard 时 fit 会继续累加到 3）
        px.setRange(20, 200);
        wp.resetDriveCounters();
        wp.grab(); wp.grab(); wp.grab();
        QCOMPARE(wp.fitCount(), 1);
        QCOMPARE(wp.backCalcCount(), 1);
        QVERIFY2(!wp.isNumericDirty() && !wp.isCameraDirty(), "Polar 收口后两方向都不脏");

        qInfo().noquote() << QString("4e Polar 防回环: setRange → fit=%1 back=%2；三次渲染后仍 fit=%1"
                                     "（轴被写为实际可见范围 %3..%4）")
                                 .arg(wp.fitCount()).arg(wp.backCalcCount())
                                 .arg(px.min(), 0, 'g', 6).arg(px.max(), 0, 'g', 6);
    }

    qInfo().noquote() << QString("4e 2D: 数值侧 fit=%1 相机侧反算=%2；闩锁已由方向状态取代")
                             .arg(w.fitCount()).arg(w.backCalcCount());
}

// ===== 4f：二维鼠标交互契约（平移比例 / 缩放中心不变 / 开关 / 计数）=====
void TestWidgetSmoke::mouseInteractionContract()
{
    QChartWidget w;
    QValueAxis ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft);
    QChartLayer layer;
    ax.setRange(-10, 10); ay.setRange(-10, 10);
    w.addAxis(&ax); w.addAxis(&ay); w.addLayer(&layer);
    w.resize(420, 340);
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "offscreen 下窗口应暴露");
    w.grab();
    const QRectF pa = w.plotArea();
    QVERIFY2(pa.width() > 0.0 && pa.height() > 0.0, "plotArea 应有效");
    QVERIFY2(w.isInteractionEnabled(), "交互开关默认开");

    // ---- ① 左键拖动 = 平移：像素位移 → 视图位移（比例一致）+ 计数（fit 0 / back +1）----
    w.resetDriveCounters();
    const QRectF vr0 = w.viewRect();
    const QPointF p0 = pa.center();
    const QPointF drag(37.0, -21.0);
    const QPointF expect(-drag.x() * vr0.width() / pa.width(),
                          drag.y() * vr0.height() / pa.height());
    sendMouse(&w, QEvent::MouseButtonPress, p0, Qt::LeftButton);
    sendMouse(&w, QEvent::MouseMove, p0 + drag, Qt::NoButton);
    sendMouse(&w, QEvent::MouseButtonRelease, p0 + drag, Qt::LeftButton);
    QCOMPARE(w.fitCount(), 0);                    // 二维交互属相机侧：绝不 fit
    QCOMPARE(w.backCalcCount(), 1);               // 反算恰 +1
    const int panFit = w.fitCount(), panBack = w.backCalcCount();
    const QRectF vr1 = w.viewRect();
    QVERIFY2(qAbs((vr1.left() - vr0.left()) - expect.x()) < 1e-9,
             qPrintable(QString("平移 Δx 应与像素比例一致：实 %1 期 %2").arg(vr1.left() - vr0.left()).arg(expect.x())));
    QVERIFY2(qAbs((vr1.top() - vr0.top()) - expect.y()) < 1e-9,
             qPrintable(QString("平移 Δy 应与像素比例一致：实 %1 期 %2").arg(vr1.top() - vr0.top()).arg(expect.y())));
    QVERIFY2(qAbs(vr1.width() - vr0.width()) < 1e-12 && qAbs(vr1.height() - vr0.height()) < 1e-12,
             "平移不改变窗口尺寸");

    // ---- ② 滚轮 = 以光标为中心缩放：中心处数据坐标不变 + 计数 ----
    w.resetDriveCounters();
    const QPointF pz(pa.left() + pa.width() * 0.3, pa.top() + pa.height() * 0.7);
    const QPointF c0 = w.pixelToCartesian(pz);
    const QRectF vrZ = w.viewRect();
    sendWheel(&w, pz, 120);
    QCOMPARE(w.fitCount(), 0);
    QCOMPARE(w.backCalcCount(), 1);
    const QPointF c1 = w.pixelToCartesian(pz);
    QVERIFY2(qAbs(c1.x() - c0.x()) < 1e-9 && qAbs(c1.y() - c0.y()) < 1e-9,
             qPrintable(QString("缩放中心处数据坐标应不变：前 (%1,%2) 后 (%3,%4)")
                            .arg(c0.x()).arg(c0.y()).arg(c1.x()).arg(c1.y())));
    const qreal f = 1.0 / 1.15;                          // 字面常量（t54 F2：不用生产 helper 自证）
    QVERIFY2(f < 1.0, "上滚 = 放大（视图窗口收缩）");
    QVERIFY2(qAbs(w.viewRect().width() - vrZ.width() * f) < 1e-9, "滚轮缩放因子生效（窗口宽 × 1.15⁻¹）");
    QVERIFY2(qAbs(QChartWidget::wheelZoomFactor(120) - (1.0 / 1.15)) < 1e-12,
             "接线断言：滚轮 120 的缩放因子恰为 1.15⁻¹（helper 对字面常量）");
    const int zoomFit = w.fitCount(), zoomBack = w.backCalcCount();

    // ---- ③ 开关关闭：鼠标事件零效果 ----
    w.setInteractionEnabled(false);
    const QRectF vrOff = w.viewRect();
    const QPointF cOff = w.pixelToCartesian(pz);
    w.resetDriveCounters();
    sendMouse(&w, QEvent::MouseButtonPress, p0, Qt::LeftButton);
    sendMouse(&w, QEvent::MouseMove, p0 + QPointF(80.0, 60.0), Qt::NoButton);
    sendMouse(&w, QEvent::MouseButtonRelease, p0 + QPointF(80.0, 60.0), Qt::LeftButton);
    sendWheel(&w, pz, -120);
    QCOMPARE(w.viewRect(), vrOff);
    QCOMPARE(w.pixelToCartesian(pz), cOff);
    QCOMPARE(w.fitCount(), 0);
    QCOMPARE(w.backCalcCount(), 0);
    w.setInteractionEnabled(true);

    qInfo().noquote() << QString("4f 2D: 拖动 Δpx=(%1,%2) → Δ视图=(%3,%4)［fit=%5 back=%6］；"
                                 "滚轮 f(120)=%7［fit=%8 back=%9］；开关关闭后 fit=%10 back=%11")
                             .arg(drag.x()).arg(drag.y()).arg(expect.x()).arg(expect.y())
                             .arg(panFit).arg(panBack).arg(f, 0, 'g', 6)
                             .arg(zoomFit).arg(zoomBack).arg(w.fitCount()).arg(w.backCalcCount());
}
