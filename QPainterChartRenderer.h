// QPainterChartRenderer.h —— QPainter 后端渲染器
// 持有 bg/fg 两张 QPixmap 缓存 + 脏标记，收编原 QChartWidget 的
// drawBackground/drawForeground 绘制编排（轴、网格、系列、调试黄框）。
// 缓存启用：背景脏→重建背景缓存、前景脏→重建前景缓存，再 blit 到目标 device；
// 缓存禁用：直接绘制。
#ifndef QPAINTERCHARTRENDERER_H
#define QPAINTERCHARTRENDERER_H

#include "QChartRenderer.h"
#include <QPixmap>

class QPainter;

class QPainterChartRenderer : public QChartRenderer {
public:
    QPainterChartRenderer() = default;
    ~QPainterChartRenderer() override = default;

    void render(const QChartScene& scene, QPaintDevice* device) override;
    void renderUncached(const QChartScene& scene, QPaintDevice* device) override;
    void invalidateBackground() override;
    void invalidateForeground() override;
    void setCachingEnabled(bool enabled) override;
    bool isCachingEnabled() const override;

private:
    void drawBackground(QPainter* p, const QChartScene& scene);
    void drawForeground(QPainter* p, const QChartScene& scene);
    /// 无缓存直接绘制（drawBackground + drawForeground）
    void drawDirect(QPainter* p, const QChartScene& scene);

    QPixmap m_bgCache, m_fgCache;
    bool m_bgDirty = true;
    bool m_fgDirty = true;
    bool m_cachingEnabled = true;
};

#endif // QPAINTERCHARTRENDERER_H
