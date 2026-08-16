// QPainterChartRenderer.cpp —— QPainter 后端渲染器实现
#include "QPainterChartRenderer.h"
#include "QChartAxis.h"    // DrawContext + 轴绘制
#include "QChartLayer.h"   // drawGrid / drawAllSeries
#include "QChartLegend.h"  // 图例绘制
#include "QChartDebug.h"   // logRender / logWidget
#include <QPainter>
#include <QFont>
#include <QDebug>
#include <QLoggingCategory>
#include <QtMath>

// 渲染每帧细节（createPath 采样等）——默认静默（QtWarningMsg），
// 可用规则 `*.verbose=true` / `chart.render.verbose=true` 精确打开。
Q_LOGGING_CATEGORY(logRenderVerbose, "chart.render.verbose", QtWarningMsg)

void QPainterChartRenderer::render(const QChartScene& scene, QPaintDevice* device) {
    if (!device) return;

    QPainter p(device);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!m_cachingEnabled) {
        drawDirect(&p, scene);
        return;
    }

    // 目标设备像素尺寸：QPaintDevice::width()/height() 对 QWidget 是逻辑尺寸、
    // 对 QImage 是像素尺寸；统一乘 devicePixelRatioF() 得到设备像素尺寸。
    qreal dpr = device->devicePixelRatioF();
    QSize devicePixels(qRound(device->width() * dpr),
                       qRound(device->height() * dpr));

    if (m_bgDirty || m_bgCache.isNull() || m_bgCache.size() != devicePixels
        || m_bgCache.devicePixelRatio() != dpr) {
        m_bgCache = QPixmap(devicePixels);
        m_bgCache.setDevicePixelRatio(dpr);
        m_bgCache.fill(Qt::transparent);
        QPainter bg(&m_bgCache);
        bg.setRenderHint(QPainter::Antialiasing, true);
        drawBackground(&bg, scene);
        m_bgDirty = false;
    }
    p.drawPixmap(0, 0, m_bgCache);

    if (m_fgDirty || m_fgCache.isNull() || m_fgCache.size() != devicePixels
        || m_fgCache.devicePixelRatio() != dpr) {
        m_fgCache = QPixmap(devicePixels);
        m_fgCache.setDevicePixelRatio(dpr);
        m_fgCache.fill(Qt::transparent);
        QPainter fg(&m_fgCache);
        fg.setRenderHint(QPainter::Antialiasing, true);
        drawForeground(&fg, scene);
        m_fgDirty = false;
    }
    p.drawPixmap(0, 0, m_fgCache);
}

void QPainterChartRenderer::renderUncached(const QChartScene& scene, QPaintDevice* device) {
    if (!device) return;
    QPainter p(device);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawDirect(&p, scene);
}

void QPainterChartRenderer::invalidateBackground() { m_bgDirty = true; }
void QPainterChartRenderer::invalidateForeground() { m_fgDirty = true; }
void QPainterChartRenderer::setCachingEnabled(bool enabled) { m_cachingEnabled = enabled; }
bool QPainterChartRenderer::isCachingEnabled() const { return m_cachingEnabled; }

void QPainterChartRenderer::drawDirect(QPainter* p, const QChartScene& scene) {
    drawBackground(p, scene);
    drawForeground(p, scene);
}

// ===== 背景层（背景填充 + 轴 + 网格 + 调试黄框）=====
void QPainterChartRenderer::drawBackground(QPainter* p, const QChartScene& scene) {
    // 画布底色：整 device 矩形填充（plotArea 跟随，无独立填充）；invalid 不填
    if (scene.backgroundColor.isValid())
        p->fillRect(QRectF(p->window()), scene.backgroundColor);

    QFont f = p->font();
    f.setPointSize(f.pointSize() - 1);
    p->setFont(f);

    // 构建 DrawContext —— 所有 draw 调用共用
    DrawContext ctx;
    ctx.plotArea   = scene.plotArea;
    ctx.dataBounds = scene.dataBounds;
    ctx.viewRect   = scene.viewRect;
    ctx.projection = scene.projection;

    qCDebug(logRender) << "drawBackground: plotArea=" << scene.plotArea
        << "viewRect=" << scene.viewRect
        << "dataBounds=" << scene.dataBounds
        << "projection type=" << (scene.projection ? (int)scene.projection->type() : -1);

    // ── 绘制所有轴 ──
    for (auto* a : scene.axes) {
        if (!a || !a->isVisible()) continue;

        qCDebug(logRender) << "drawBackground: drawing axis alignment=" << a->alignment()
            << "color=" << a->color()
            << "isInterior=" << (a->alignment() == Qt::AlignHCenter || a->alignment() == Qt::AlignVCenter);

        bool isInterior = (a->alignment() == Qt::AlignHCenter
                        || a->alignment() == Qt::AlignVCenter);
        if (isInterior) {
            // 数据主脊：画在 offset = 0 的位置（通过 dataBounds 确定默认位置）
            // offset=0 意味着画在 Numeric dim0=0 或 dim1=0 的等值线上
            qreal defaultOffset = 0.0;
            if (a->alignment() == Qt::AlignHCenter)
                defaultOffset = ctx.dataBounds.top();  // Y 维度在默认位置
            else
                defaultOffset = ctx.dataBounds.left(); // X 维度在默认位置

            QString nullLabel = "";
            p->save();
            p->setClipRect(scene.plotArea);
            a->drawAtPosition(p, ctx, defaultOffset,
                              /*axisLine=*/true, /*labels=*/false, /*ticks=*/true,
                              /*label=*/nullLabel, /*pen=*/nullptr);
            p->restore();
        } else {
            // 边框轴：画在 plotArea 边缘
            a->drawAtEdge(p, ctx,
                          /*axisLine=*/true, /*labels=*/true, /*ticks=*/true);
        }
    }

    // ── 绘制网格（取最后一个几何体）──
    if (!scene.layers.isEmpty()) {
        auto* geo = scene.layers.last();
        p->save();
        p->setClipRect(scene.plotArea);
        geo->drawGrid(p, ctx);
        p->restore();
    }

    // ── 调试：黄色 plotArea 边框（仅屏显；导出模式跳过，避免泄漏进导出产物）──
    if (!scene.exportMode && logWidget().isDebugEnabled()) {
        p->save();
        p->setPen(Qt::yellow);
        p->drawRect(scene.plotArea);
        p->restore();
    }
}

// ===== 前景层（系列）=====
void QPainterChartRenderer::drawForeground(QPainter* p, const QChartScene& scene) {
    DrawContext ctx;
    ctx.plotArea   = scene.plotArea;
    ctx.dataBounds = scene.dataBounds;
    ctx.viewRect   = scene.viewRect;
    ctx.projection = scene.projection;

    for (auto* g : scene.layers) {
        p->save();
        p->setClipRect(scene.plotArea);
        g->drawAllSeries(p, ctx);
        p->restore();
    }

    // 图例（B1 overlay，clip 到 plotArea）
    if (scene.legend && scene.legend->isVisible()) {
        p->save();
        p->setClipRect(scene.plotArea);
        scene.legend->draw(p, scene.plotArea, scene.legendItems);
        p->restore();
    }
}
