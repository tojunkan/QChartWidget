// test_widget_smoke.cpp —— 批次 A widget 容器化冒烟（offscreen CPU）
// 覆盖：QChartWidget 纯容器（无相机/buildScene/图例导出）→ addAxis/addLayer →
//       plotArea 扣除边框轴外边距 + plotAreaChanged/projectionChanged 广播 →
//       CPU paintEvent 全量 QPainter 出图（网格/边框轴真实像素）。
#include "test_widget_smoke.h"

#include <QtTest>
#include <QSignalSpy>
#include <QImage>
#include <QColor>
#include <QMatrix4x4>     // t72：viewMatrix 朝向约定断言
#include <QVector4D>
#include <cmath>

#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>

#include "QChartWidget.h"
#include "QChartLayer.h"
#include "QChartCamera.h"    // t72：viewMatrix()/project() 双路径一致性
#include "QChartPlotAreaLayout.h"   // t82：直通布局单元断言
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

// 4g-fix（t58）：绘图区内像素级差异判定（轴/内容状态已变 ⇒ 图内必须变化）
bool imagesDifferInPlot(const QImage& a, const QImage& b, const QRectF& plotArea)
{
    if (a.size() != b.size()) return true;
    const QRect r = plotArea.toRect().intersected(a.rect());
    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x)
            if (a.pixel(x, y) != b.pixel(x, y)) return true;
    return false;
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
        // t73（越界最小修正，已在提交中向队长声明）：组尾锚点改为**按图元类型分类**——直线取尾端
        // cartB（旧实现一律取 cartA(首端点)，恰落视图边界时会被绘制期二次包含判定按浮点噪声丢掉
        // 整组，见 t70 §3a 与 t73 修复）。本夹具组尾为网格脊（4i 后 = 两顶点 Line）⇒ 期望 cartB。
        const QChartPrimitive& tp = f.scene.primitives[tail];
        QVector3D want = tp.cartA;
        if (tp.type == QChartPrimitive::Type::Line) {
            want = tp.cartB;
        } else if (tp.type == QChartPrimitive::Type::Path || tp.type == QChartPrimitive::Type::Polygon
                   || tp.type == QChartPrimitive::Type::TriangleMesh
                   || tp.type == QChartPrimitive::Type::TriangleFan
                   || tp.type == QChartPrimitive::Type::TriangleStrip) {
            if (!tp.cartVerts.isEmpty()) want = tp.cartVerts.last();
        }
        QVERIFY2(l.cartesianAnchor.x() == want.x() && l.cartesianAnchor.y() == want.y(),
                 "自由标签锚点应等于本组组尾图元的**组尾锚点**（t73 分类：Line→cartB，其余→cartA/末顶点）");
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

    // ===== t72：二维相机朝向约定（viewMatrix 与 project() 必须是**同一个**映射）=====
    // 背景（t71 诊断）：GL 曾相对 CPU 整幅垂直镜像——viewMatrix 的 Y 缩放取负号，把 NDC 的
    // “+Y = 视口上方”误当成数值大端；而 CPU project() 的 py = bottom − ny·h 表示“数值小端在屏幕下方”。
    // 本段把两条路径的一致性钉死在**已登记进构建**的本文件里（legacy 的 test_qchartcamera.cpp 不参与构建）。
    {
        QChartWidget wo;
        QValueAxis ox(nullptr, Qt::AlignBottom), oy(nullptr, Qt::AlignLeft);
        QChartLayer lo;
        ox.setRange(-13.0, 7.0);          // ★ 非对称范围（中心 −3）：nice 刻度 = {−12,−8,−4,0,4}
        oy.setRange(-13.0, 7.0);
        ox.setTickCount(5); oy.setTickCount(5);
        wo.addAxis(&ox); wo.addAxis(&oy); wo.addLayer(&lo);
        wo.resize(420, 340);
        wo.show();
        QVERIFY2(QTest::qWaitForWindowExposed(&wo), "offscreen 下窗口应暴露");
        wo.grab();
        const QRectF pa = wo.plotArea();
        const QChartCamera* cam = lo.camera();
        QVERIFY2(cam && pa.width() > 100.0 && pa.height() > 100.0, "plotArea 与相机应有效");
        const QRectF vr = cam->viewRect();
        const QMatrix4x4 vm = cam->viewMatrix();

        auto ndcOf = [&vm](qreal x, qreal y) {
            const QVector4D v = vm * QVector4D(float(x), float(y), 0.0f, 1.0f);
            return QVector3D(v.x() / v.w(), v.y() / v.w(), 0.0f);
        };
        // NDC → 像素（GL 视口约定：NDC +Y 朝屏幕上方；Qt 像素行号向下递增）
        auto ndcToPixel = [&pa](const QVector3D& ndc) {
            return QPointF(pa.left() + (ndc.x() + 1.0) * 0.5 * pa.width(),
                           pa.top()  + (1.0 - ndc.y()) * 0.5 * pa.height());
        };

        // ① viewRect 四角：NDC 符号 + 与 project() 像素映射一致（同一 Cartesian 点两路径差 < 1e-6）
        struct Corner { qreal x, y, ndcY; const char* tag; };
        const Corner corners[4] = {
            { vr.left(),  vr.top(),    -1.0, "left-top（数值小端 Y）" },
            { vr.right(), vr.top(),    -1.0, "right-top（数值小端 Y）" },
            { vr.left(),  vr.bottom(), +1.0, "left-bottom（数值大端 Y）" },
            { vr.right(), vr.bottom(), +1.0, "right-bottom（数值大端 Y）" },
        };
        for (const Corner& c : corners) {
            const QVector3D ndc = ndcOf(c.x, c.y);
            QVERIFY2(qAbs(ndc.y() - c.ndcY) < 1e-6,
                     qPrintable(QString("角 %1 的 NDC y 应为 %2，实为 %3")
                                .arg(c.tag).arg(c.ndcY).arg(ndc.y())));
            QVERIFY2(qAbs(qAbs(ndc.x()) - 1.0) < 1e-6,
                     qPrintable(QString("角 %1 的 NDC x 应为 ±1，实为 %2").arg(c.tag).arg(ndc.x())));
            const QPointF viaNdc = ndcToPixel(ndc);
            const QPointF viaProj = cam->project(QVector3D(float(c.x), float(c.y), 0.0f), pa).screen;
            QVERIFY2(qAbs(viaNdc.x() - viaProj.x()) < 1e-6 && qAbs(viaNdc.y() - viaProj.y()) < 1e-6,
                     qPrintable(QString("角 %1：viewMatrix→NDC→像素 %2,%3 与 project() %4,%5 应一致")
                                .arg(c.tag).arg(viaNdc.x()).arg(viaNdc.y())
                                .arg(viaProj.x()).arg(viaProj.y())));
        }
        // ② 内部偏心点同样一致（横/纵各 5 等分，含视图中心）
        int interiorChecked = 0;
        for (int i = 1; i <= 4; ++i) {
            const qreal t = i / 4.0;
            const qreal xv = vr.left() + t * vr.width();
            const qreal yv = vr.top()  + t * vr.height();
            const qreal xs[2] = { xv, vr.center().x() };
            const qreal ys[2] = { vr.center().y(), yv };
            for (int k = 0; k < 2; ++k) {
                const QVector3D ndc = ndcOf(xs[k], ys[k]);
                const QPointF viaNdc = ndcToPixel(ndc);
                const QPointF viaProj = cam->project(QVector3D(float(xs[k]), float(ys[k]), 0.0f), pa).screen;
                QVERIFY2(qAbs(viaNdc.x() - viaProj.x()) < 1e-6 && qAbs(viaNdc.y() - viaProj.y()) < 1e-6,
                         qPrintable(QString("内部点 (%1,%2)：NDC→像素 %3,%4 与 project() %5,%6 应一致")
                                    .arg(xs[k]).arg(ys[k]).arg(viaNdc.x()).arg(viaNdc.y())
                                    .arg(viaProj.x()).arg(viaProj.y())));
                ++interiorChecked;
            }
        }
        // ③ 屏幕方向（显式）：数值大端的像素行号必须**更小**（在屏幕上方）
        const QPointF pLo = cam->project(QVector3D(float(vr.center().x()), float(vr.top()), 0.0f), pa).screen;
        const QPointF pHi = cam->project(QVector3D(float(vr.center().x()), float(vr.bottom()), 0.0f), pa).screen;
        QVERIFY2(pHi.y() < pLo.y() - 1.0,
                 qPrintable(QString("数值大端应在上方：y=top→行%1，y=bottom→行%2").arg(pLo.y()).arg(pHi.y())));
        // ④ 夹具自检（防“镜像≡平移”自欺）：刻度行集关于中心行镜像后**不得**整体重合
        const QVector<qreal> oyTicks = oy.tickValues(qMin(vr.top(), vr.bottom()),
                                                     qMax(vr.top(), vr.bottom()));
        QVector<qreal> tickRows;
        for (qreal v : oyTicks)
            tickRows.append(cam->project(QVector3D(0.0f, float(v), 0.0f), pa).screen.y());
        QVERIFY2(tickRows.size() >= 4, "非对称夹具至少 4 条刻度行");
        int mirrorHits = 0;
        for (qreal r : tickRows) {
            for (qreal r2 : tickRows) {
                if (qAbs((2.0 * pa.center().y() - r) - r2) <= 1.0) { ++mirrorHits; break; }
            }
        }
        QVERIFY2(mirrorHits * 2 <= tickRows.size(),
                 qPrintable(QString("刻度行集必须不关于视图中心对称（镜像命中 %1/%2）；否则镜像不可观测")
                            .arg(mirrorHits).arg(tickRows.size())));
        qInfo().noquote() << QString("t72 朝向约定: 四角+%1 内部点 NDC↔project 一致（<1e-6px）；"
                                     "数值大端行 %2 < 小端行 %3；刻度行 %4 条、镜像命中 %5")
                                 .arg(interiorChecked).arg(pHi.y()).arg(pLo.y())
                                 .arg(tickRows.size()).arg(mirrorHits);
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

// ===== 4g：脏模型契约（二维）——视图变化重收集 / 无变化跳过 / 数据变化仍重收集 =====
void TestWidgetSmoke::dirtyModelContract()
{
    QChartWidget w;
    QValueAxis ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft);
    QChartLayer layer;
    ax.setRange(-10, 10); ay.setRange(-10, 10);
    w.addAxis(&ax); w.addAxis(&ay); w.addLayer(&layer);
    w.resize(420, 340);
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "offscreen 下窗口应暴露");
    w.grab();                                     // 首帧（必然重收集）
    layer.resetCollectCount();

    auto maxNumX = [&layer]() {                   // 背景图元的 numeric 极值（变换只写 cart*，num* 保真）
        qreal m = -1e30;
        for (const QChartPrimitive& p : layer.scene().primitives) {
            m = qMax(m, qreal(p.numA.x()));
            if (p.type == QChartPrimitive::Type::Line) m = qMax(m, qreal(p.numB.x()));
        }
        return m;
    };

    // ① 无变化 → 跳过重收集（4g：不再每帧重建背景）
    w.grab();
    QCOMPARE(layer.collectCount(), 0);
    w.grab();
    QCOMPARE(layer.collectCount(), 0);

    // ② 视图变化（相机侧平移）→ 重收集 +1，且 renderer 重算变换（相机侧链：fit 0）
    const int fit0 = w.fitCount();
    w.panViewCartesian(1.0, 1.0);
    w.grab();
    QCOMPARE(layer.collectCount(), 1);
    QCOMPARE(w.fitCount(), fit0);

    // ③ 数值侧驱动（轴范围 → 相机窗口）→ 重收集 +1，且背景网格随新范围重建
    ax.setRange(0.0, 100.0);
    w.grab();
    QCOMPARE(layer.collectCount(), 2);
    const qreal e100a = maxNumX();
    QVERIFY2(e100a > 90.0, qPrintable(QString("背景应重收集到新轴范围（numeric 极值应≈100，实为 %1）").arg(e100a)));

    ax.setRange(-10.0, 10.0);                     // 回旧范围 → 图元随范围回退（对照）
    w.grab();
    QCOMPARE(layer.collectCount(), 3);
    const qreal e10 = maxNumX();
    QVERIFY2(e10 > 8.0 && e10 < 13.0,
             qPrintable(QString("回旧范围后 numeric 极值应≈10，实为 %1").arg(e10)));
    QVERIFY2(e100a > e10 + 50.0, "两个范围的背景几何必须明显不同（证明按范围重建，而非陈旧缓存）");

    ax.setRange(0.0, 100.0);                      // 再回新范围 → 几何应与首次逐位一致（可复现重建）
    w.grab();
    QCOMPARE(layer.collectCount(), 4);
    QVERIFY2(qAbs(maxNumX() - e100a) < 1e-9,
             qPrintable(QString("同范围重建应逐位一致：首次 %1 vs 再次 %2").arg(e100a).arg(maxNumX())));

    // ④ 数据变化（invalidateData）→ 仍会重收集
    layer.invalidateData();
    w.grab();
    QCOMPARE(layer.collectCount(), 5);

    // ⑤ 交互（滚轮缩放）→ 视图变化 → 重收集 +1
    sendWheel(&w, w.plotArea().center(), 120);
    w.grab();
    QCOMPARE(layer.collectCount(), 6);

    // ⑥ t58 F1：轴样式（刻度数）变化 → 置脏重收集 + 绘图区内图像变化
    const QImage imgBeforeTick = w.grab().toImage();
    ax.setTickCount(17);
    w.grab();
    QCOMPARE(layer.collectCount(), 7);
    QVERIFY2(imagesDifferInPlot(imgBeforeTick, w.grab().toImage(), w.plotArea()),
             "刻度数变化后绘图区内图像应变化（t58 F1 回归）");

    // ⑦ t58 F2：退化轴范围（min==max，不 fit、相机不变）→ 仍须置脏重收集 + 图内图像随轴变化
    const QImage imgBeforeDeg = w.grab().toImage();
    ax.setRange(5.0, 5.0);
    w.grab();
    QCOMPARE(layer.collectCount(), 8);
    QVERIFY2(imagesDifferInPlot(imgBeforeDeg, w.grab().toImage(), w.plotArea()),
             "退化轴范围后绘图区内图像应变化（t58 F2 回归：不得内外不一致）");
    ax.setRange(-10.0, 10.0);                     // 恢复（重新 fit → 再收集一次）
    w.grab();
    QCOMPARE(layer.collectCount(), 9);

    // ⑧ t58 F4：内容指纹覆盖网格样式（不依赖逐信号接线）
    layer.setGridVisible(false);
    w.grab();
    QCOMPARE(layer.collectCount(), 10);
    layer.setGridVisible(true);
    w.grab();
    QCOMPARE(layer.collectCount(), 11);

    qInfo().noquote() << QString("4g 2D 脏模型: 稳态跳过 ×2；平移/轴范围/回退/同范围重建/数据/滚轮/刻度数/退化范围/网格样式 → 重收集累计 %1 次"
                                 "（背景 numeric 极值：范围 0..100 → %2，范围 -10..10 → %3，同范围重建逐位一致）")
                             .arg(layer.collectCount()).arg(e100a).arg(e10);

    // ===== t82：QChartPlotAreaLayout 直通布局（offscreen，无需 GL）=====
    // 目的：GL 宿主几何改由 Qt **布局阶段**应用（脱离绘制回调，消除 Windows resize 崩溃的重入路径）。
    // 本段验证布局类自身：① 给定 plotArea 后经事件循环几何被正确应用；② 几何未变时重复触发布局
    // 不再调用 setGeometry（applyCount 不增、skipCount 增）；③ setGeometry 忽略 Qt 传入 rect（直通）；
    // ④ 摘除宿主（切回 CPU 后端的等价动作）后不再摆放。
    {
        QWidget host;
        host.resize(400, 300);
        auto* child = new QWidget(&host);
        auto* lay = new QChartPlotAreaLayout(child, &host);
        host.setLayout(lay);
        host.show();
        QVERIFY2(QTest::qWaitForWindowExposed(&host), "offscreen 下宿主窗口应暴露");
        auto pump = []() { for (int i = 0; i < 5; ++i) { QCoreApplication::processEvents(); QTest::qWait(5); } };

        lay->resetCounters();
        lay->setPlotArea(QRect(20, 30, 200, 150));
        QCOMPARE(lay->applyCount(), 0);           // 调用点（等价 relayout()）不直接改几何
        pump();
        QCOMPARE(child->geometry(), QRect(20, 30, 200, 150));
        QVERIFY2(lay->applyCount() >= 1, "几何应由布局阶段应用");
        QCOMPARE(lay->host(), child);

        // ② 同一几何重复触发布局 ⇒ 不再调用 setGeometry
        const int applied = lay->applyCount();
        const int skipped = lay->skipCount();
        for (int i = 0; i < 3; ++i) { lay->setPlotArea(QRect(20, 30, 200, 150)); lay->invalidate(); }
        pump();
        QCOMPARE(lay->applyCount(), applied);
        QVERIFY2(lay->skipCount() > skipped, "重复布局应走跳过分支（几何未变不重设）");

        // ③ 直通语义：Qt 传入 rect 被忽略，仍按保存的 plotArea 摆放
        lay->setGeometry(host.rect());
        lay->invalidate();
        pump();
        QCOMPARE(child->geometry(), QRect(20, 30, 200, 150));

        // ④ 摘除宿主后不再摆放（切回 CPU 后端时先摘除再销毁）
        lay->setHost(nullptr);
        lay->setPlotArea(QRect(0, 0, 10, 10));
        lay->invalidate();
        pump();
        QCOMPARE(child->geometry(), QRect(20, 30, 200, 150));

        // ⑤ 宿主由隐藏变可见时补布局（不可见期间被跳过的几何需自兜底）
        {
            QWidget host2;
            host2.resize(300, 200);
            auto* child2 = new QWidget(&host2);    // 父窗口未 show ⇒ 子控件 isVisible()==false
            auto* lay2 = new QChartPlotAreaLayout(child2, &host2);
            host2.setLayout(lay2);
            lay2->setPlotArea(QRect(5, 6, 120, 80));
            pump();                                     // 宿主窗口未显示：几何不该被应用
            QVERIFY2(!child2->isVisible(), "父窗口未 show 时子控件不可见");
            QVERIFY2(child2->geometry() != QRect(5, 6, 120, 80), "不可见期间不得摆放");
            host2.show();
            QVERIFY2(QTest::qWaitForWindowExposed(&host2), "第二个宿主窗口应暴露");
            pump();
            QCOMPARE(child2->geometry(), QRect(5, 6, 120, 80));
        }

        qInfo().noquote() << QString("t82 布局直通（offscreen）：apply=%1 skip=%2；几何=%3；摘除后不再摆放")
                                 .arg(lay->applyCount()).arg(lay->skipCount())
                                 .arg(QString("(%1,%2 %3x%4)").arg(child->geometry().x())
                                          .arg(child->geometry().y())
                                          .arg(child->geometry().width())
                                          .arg(child->geometry().height()));
    }
}
