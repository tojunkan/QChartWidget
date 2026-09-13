// test_axis_edge.cpp —— 批次 B3：drawAtEdge 专项
// 边框轴 = plotArea 外 QPainter 直绘（不进 renderer/cull，有意设计）。
// 断言：轴线贴 plotArea 边缘、刻度/标签墨迹落 plotArea 外带；非边框 alignment 拒绝零绘制。
#include "test_axis_edge.h"

#include <QtTest>
#include <QImage>
#include <QColor>
#include <QPainter>

#include "QChartAxis.h"
#include "QValueAxis.h"
#include "QChartProjection.h"
#include "QCartesianProjection.h"

namespace {
bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}
/// 4h-a：外带内“标签行带”中心（每行只要有墨即计入，连续行合并为一带）
QVector<int> labelRowBands(const QImage& img, const QRectF& plot, int bandWidth = 55)
{
    QVector<int> bands;
    const int x0 = qMax(0, int(plot.left()) - bandWidth);
    const int x1 = qMin(img.width() - 1, int(plot.left()) - 1);
    int runStart = -1;
    // 仅扫 plotArea 纵向范围（排除上/下边框轴标签带）
    const int y0 = qMax(0, int(plot.top())), y1 = qMin(img.height() - 1, int(plot.bottom()));
    for (int y = y0; y <= y1; ++y) {
        bool ink = false;
        for (int x = x0; x <= x1 && !ink; ++x)
            if (isInk(img.pixelColor(x, y))) ink = true;
        if (ink && runStart < 0) runStart = y;
        if (!ink && runStart >= 0) { bands.append((runStart + y - 1) / 2); runStart = -1; }
    }
    if (runStart >= 0) bands.append((runStart + y1) / 2);
    return bands;
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

struct EdgeFixture {
    QImage img{320, 220, QImage::Format_ARGB32_Premultiplied};
    QRectF plot{50, 40, 220, 140};
    QCartesianProjection proj;
    DrawContext ctx;

    EdgeFixture() {
        img.fill(Qt::white);
        ctx.plotArea = plot;
        // 4h-a：改为**生产朝向**（形态等同 QChartLayer::numericBoundsFromAxes()）：
        // legacy 取向 —— dim0 = left(min)..right(max)、dim1 = bottom(min)..top(max)，故 height() < 0。
        // （旧夹具喂 height>0 的数学式 rect，与生产朝向相反，因此测不出 drawAtEdge 的反向读取。）
        ctx.dataBounds = QRectF(0, 10, 10, -10);
        ctx.viewRect = QRectF(0, 10, 10, -10);
        ctx.projection = &proj;
    }
};

/// 画单边框轴并断言外带墨迹与边缘线（side: 0=bottom 1=top 2=left 3=right）
void drawAndVerifyEdge(EdgeFixture& fx, Qt::Alignment align, const QRectF& lineRect,
                       const QRect& outerBand)
{
    QValueAxis axis(nullptr, align);
    axis.setColor(Qt::black);
    QPainter p(&fx.img);
    axis.drawAtEdge(&p, fx.ctx, true, true, true);
    p.end();
    QVERIFY2(inkIn(fx.img, lineRect.toRect().adjusted(-1, -1, 1, 1)) > 0,
             "轴线应贴 plotArea 边缘");
    QVERIFY2(inkIn(fx.img, outerBand) > 0, "刻度/标签墨迹应落 plotArea 外带");
}
} // namespace

void TestAxisEdge::fourDirectionsRenderOutside()
{
    const qreal L = 50, T = 40, R = 270, B = 180;   // plot 边缘
    // Bottom：线在 y=B（横贯 x∈L..R）；外带 y∈[B+1, B+60]
    {
        EdgeFixture fx;
        drawAndVerifyEdge(fx, Qt::AlignBottom,
                          QRectF(L, B, R - L, 1), QRect(qRound(L), qRound(B) + 1, qRound(R - L), 55));
    }
    // Top：线在 y=T；外带 y∈[T-55, T-1]
    {
        EdgeFixture fx;
        drawAndVerifyEdge(fx, Qt::AlignTop,
                          QRectF(L, T, R - L, 1), QRect(qRound(L), qRound(T) - 55, qRound(R - L), 55));
    }
    // Left：线在 x=L；外带 x∈[L-55, L-1]
    {
        EdgeFixture fx;
        drawAndVerifyEdge(fx, Qt::AlignLeft,
                          QRectF(L, T, 1, B - T), QRect(qRound(L) - 55, qRound(T), 55, qRound(B - T)));
    }
    // Right：线在 x=R；外带 x∈[R+1, R+55]
    {
        EdgeFixture fx;
        drawAndVerifyEdge(fx, Qt::AlignRight,
                          QRectF(R, T, 1, B - T), QRect(qRound(R) + 1, qRound(T), 55, qRound(B - T)));
    }

    // ===== 4h-a：区间规范化（同一数值范围的两种朝向必须逐项一致）=====
    // legacy 朝向（生产：top=max、height<0） vs 数学式朝向（对向：top=min、height>0）
    auto renderWithBounds = [](const QRectF& bounds, Qt::Alignment align, int tickCount) {
        EdgeFixture fx;
        fx.ctx.dataBounds = bounds;
        fx.ctx.viewRect = bounds;
        QValueAxis axis(nullptr, align);
        axis.setTickCount(tickCount);
        axis.setColor(Qt::black);
        QPainter p(&fx.img);
        axis.drawAtEdge(&p, fx.ctx, true, true, true);
        p.end();
        return fx.img;
    };
    for (Qt::Alignment align : { Qt::AlignLeft, Qt::AlignRight, Qt::AlignTop, Qt::AlignBottom }) {
        const QImage legacy = renderWithBounds(QRectF(0, 10, 10, -10), align, 5);   // 生产朝向
        const QImage mirror = renderWithBounds(QRectF(0, 0, 10, 10), align, 5);     // 对向朝向（同范围）
        QVERIFY2(legacy == mirror,
                 qPrintable(QString("4h-a：同一范围两种朝向渲染必须逐位一致（align=%1）")
                                .arg(int(align))));
    }

    // ===== 4h-a：非对称范围回归（字面量期望；-5..45、plot{50,40,220,140}、tickCount=5）=====
    // 一次实测取定的字面量（禁止用生产 helper 反推）：
    //   刻度值集合 = {0, 10, 20, 30, 40}（nice step=10，5 个；反向读取时会退化为 9 等分兜底）
    //   标签行带中心 = {54, 82, 110, 138, 166}（像素行；容差 ±2）
    {
        const QRectF asyPlot(50, 40, 220, 140);
        for (int orientation = 0; orientation < 2; ++orientation) {
            // orientation 0 = 生产朝向（legacy, height<0）；1 = 对向朝向（数学式）
            const QRectF bounds = (orientation == 0) ? QRectF(0, 45, 10, -50) : QRectF(0, -5, 10, 50);
            const QImage img = renderWithBounds(bounds, Qt::AlignLeft, 5);
            const QVector<int> rows = labelRowBands(img, asyPlot);
            QCOMPARE(rows.size(), 5);                       // 5 个 nice 刻度（非 9 等分兜底）
            const int expectRows[5] = {54, 82, 110, 138, 166};
            for (int i = 0; i < 5; ++i)
                QVERIFY2(qAbs(rows[i] - expectRows[i]) <= 2,
                         qPrintable(QString("4h-a 标签行带 %1：期望 %2（±2）实为 %3（朝向 %4）")
                                        .arg(i).arg(expectRows[i]).arg(rows[i]).arg(orientation)));
            // 刻度值集合（字面量）：用独立投影算式换算 expected 行——见上（值集合由 tickValues 语义保证：
            // 若反向读取则 step 退化、行带数变 9，上面的 size 断言即变红）
        }
    }
}

void TestAxisEdge::nonBorderAlignmentRejected()
{
    for (Qt::Alignment align : {Qt::AlignHCenter, Qt::AlignVCenter}) {
        EdgeFixture fx;
        QValueAxis axis(nullptr, align);
        QPainter p(&fx.img);
        // 非法 alignment：qWarning 拒绝分支 + 零绘制
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("QChartAxis::drawAtEdge.*not a border alignment.*"));
        axis.drawAtEdge(&p, fx.ctx, true, true, true);
        p.end();
        QCOMPARE(inkIn(fx.img, QRect(0, 0, fx.img.width(), fx.img.height())), 0);
    }
}

void TestAxisEdge::styleParamsEffective()
{
    // 颜色生效：轴线应为 axis color（品红系）
    {
        EdgeFixture fx;
        QValueAxis axis(nullptr, Qt::AlignBottom);
        axis.setColor(Qt::magenta);
        QPainter p(&fx.img);
        axis.drawAtEdge(&p, fx.ctx, true, false, true);   // 无标签，纯线
        p.end();
        const QColor c = fx.img.pixelColor(qRound((fx.plot.left() + fx.plot.right()) / 2),
                                           qRound(fx.plot.bottom()));
        QVERIFY2(qAbs(c.red() - 255) < 40 && qAbs(c.green() - 0) < 60 && qAbs(c.blue() - 255) < 60,
                 qPrintable(QString("轴线颜色应≈magenta 实际 %1").arg(c.name())));
    }
    // tickCount 生效：标签行数随刻度数变化（tickCount 3 vs 9 → 外带墨迹显著不同）
    {
        EdgeFixture a, b;
        auto paintAxis = [](EdgeFixture& fx, int ticks) {
            QValueAxis axis(nullptr, Qt::AlignBottom);
            axis.setTickCount(ticks);
            axis.setColor(Qt::black);
            QPainter p(&fx.img);
            axis.drawAtEdge(&p, fx.ctx, false, true, true);
            p.end();
        };
        paintAxis(a, 3);
        paintAxis(b, 9);
        const QRect band(qRound(a.plot.left()), qRound(a.plot.bottom()) + 1,
                         qRound(a.plot.width()), 50);
        const int na = inkIn(a.img, band), nb = inkIn(b.img, band);
        QVERIFY2(nb > na, "tickCount 增大应产生更多刻度/标签墨迹");
    }
}
