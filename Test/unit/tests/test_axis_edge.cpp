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
        ctx.dataBounds = QRectF(0, 0, 10, 10);
        ctx.viewRect = QRectF(0, 0, 10, 10);
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
