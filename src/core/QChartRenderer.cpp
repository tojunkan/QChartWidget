// QChartRenderer.cpp
#include "QChartRenderer.h"
#include "QChartCamera.h"
#include "QChartAbstractProjection.h"
#include <QRectF>
#include <QPainter>
#include <QVector>
#include <algorithm>
#include <cmath>

// ============================================================================
// t75：标签自动避让（"最空一边"启发式）常量——契约见 include/core/QChartRenderer.h
// ============================================================================
namespace {
constexpr qreal kAvoidTieEpsPx      = 1.0;    // ① 打平判定：**绝对容差**（像素）
constexpr qreal kAvoidHysteresisPx  = 8.0;    // ③ 迟滞：另一侧余量需超出这么多像素才切换
constexpr int   kAvoidMaxEntries    = 256;    // 状态表 LRU 上限（防无界增长）
} // namespace

QString QChartRenderer::labelAvoidKey(const QString& text, qreal fontSize)
{
    // 稳定身份：标签每帧重建，故以"内容 + 字号"作键（同一场景内同一刻度文字恒定）。
    // ★ 图层已按契约把组号盖进 label.sourceId（二维 addSpine / 三维 addLine），待标签绘制调用点
    //   （QPainterChartRenderer::drawLabels2D/3D、QOpenGLChartRenderer::drawLabels）进入可改范围时，
    //   把组号透传进来替换本代理键即可（一处改动，语义不变）。
    return QString::number(fontSize, 'g', 6) + QLatin1Char('|') + text;
}

int QChartRenderer::labelAvoidSide(const QString& key) const
{
    const auto it = m_labelAvoid.constFind(key);
    return (it == m_labelAvoid.constEnd()) ? -1 : it->side;
}

int QChartRenderer::labelAvoidStateCount() const
{
    return m_labelAvoid.size();
}

void QChartRenderer::pruneAvoidState()
{
    // LRU 裁剪：超过上限时丢弃最久未更新的条目（标签消失/场景重建不会无界增长）
    if (m_labelAvoid.size() <= kAvoidMaxEntries) return;

    QVector<QPair<quint64, QString>> ages;
    ages.reserve(m_labelAvoid.size());
    for (auto it = m_labelAvoid.constBegin(); it != m_labelAvoid.constEnd(); ++it)
        ages.append(qMakePair(it->seq, it.key()));
    std::sort(ages.begin(), ages.end(),
              [](const QPair<quint64, QString>& a, const QPair<quint64, QString>& b) {
                  return a.first < b.first;
              });
    const int drop = m_labelAvoid.size() - kAvoidMaxEntries;
    for (int i = 0; i < drop; ++i) m_labelAvoid.remove(ages[i].second);
}

void QChartRenderer::render(QChartScene& scene, QPaintDevice* device)
{
    if (!device || !scene.camera || !scene.projection) {
        return;
    }

    onRenderBegin(device);

    // ---- 步骤 2：变换与裁剪（4g：仅在场景快照重建后执行——widget 在层重收集时置 viewDirty；
    //      视图变化必须重收集背景（网格/刻度/标签随可见 numeric 范围更新），
    //      当前为粗粒度全量更新（重收集 + 变换 + 裁剪），背景/前景分级留待后续批次） ----
    if (m_viewDirty) {
        transformNumericToCartesian(scene);
        cullAndResolveLabels(scene);
        m_viewDirty = false;
    }

    // ---- 步骤 3：图元绘制 ----
    drawPrimitives(scene, device, m_visibilityCache);

    // ---- 步骤 4：标签绘制 ----
    drawLabels(scene, device);

    onRenderEnd(device);
}

// 标签锚点像素可见性判定（批次2 收尾：两后端共用单一实现，见 QChartRenderer.h）
bool QChartRenderer::anchorVisibleInPlotArea(const QChartScene& scene, const QVector3D& cart) const
{
    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return false;

    const QRectF& plotArea = scene.plotArea;
    if (plotArea.isEmpty()) return false;   // 零面积画布：显式锚点一律不可见

    const QChartProjectedPoint pp = camera->project(cart, plotArea);
    return std::isfinite(pp.screen.x()) && std::isfinite(pp.screen.y())
           && plotArea.contains(pp.screen);
}

// ============================================================================
// 【未来混合后端预研 · 当前不启用】自由标签 CPU 前置解析旁路（hybrid 预研）
// ----------------------------------------------------------------------------
// 见 include/core/QChartRenderer.h 中 hybridResolveFreeLabelAnchor 的长注释：
// 既定契约 = GL 不渲染自由标签；本组代码是 t29 验证过的"CPU 前置 projection+裁剪"实现
// 的收拢存放处，默认不被任何正常渲染路径调用（属旁路 / 死代码，grep "hybrid" 可定位）。
// ============================================================================
// ============================================================================
// 组尾锚点助手（t73）：自由标签（同 sourceId 组）与 tier2 绑定标签的锚点取"组的末段代表点"，
// 按图元类型分类——与 t29 hybrid 旁路既有分类一致，并由二维 CPU 后端（tier2/tier3）与 GL 后端
// （tier2）共用同一实现，避免两后端语义分叉：
//   Point → 自身；Line → **尾端**（numB/cartB）；Path/Polygon/三角族 → 末顶点；Rect/Ellipse → 中心
// 背景：4i 直线化后二维网格脊只剩一个两顶点 Line，旧实现一律取 cartA（**首端点**）恰落视图边界，
// 绘制期的二次包含判定（见下方 drawLabels）会因 1e-5 px 级浮点噪声丢掉整组标签（t70 §3a）。
// 说明：头文件不在本批 inScope，故两处调用方以相同签名的前置声明共用（定义唯一）。
// ============================================================================
QVector3D qchartGroupTailAnchorNumeric(const QChartPrimitive& p)
{
    switch (p.type) {
    case QChartPrimitive::Type::Point:
        return p.numA;
    case QChartPrimitive::Type::Line:
        return p.numB;
    case QChartPrimitive::Type::Polygon:
    case QChartPrimitive::Type::Path:
    case QChartPrimitive::Type::TriangleMesh:
    case QChartPrimitive::Type::TriangleFan:
    case QChartPrimitive::Type::TriangleStrip:
        return p.numVerts.isEmpty() ? p.numA : p.numVerts.last();
    case QChartPrimitive::Type::Rect:
    case QChartPrimitive::Type::Ellipse:
        return QVector3D(static_cast<float>(p.numRect.center().x()),
                         static_cast<float>(p.numRect.center().y()), 0.0f);
    }
    return p.numA;
}

/// Cartesian 空间同义分类（CPU 后端在 transformNumericToCartesian 之后解析标签）
QVector3D qchartGroupTailAnchorCartesian(const QChartPrimitive& p)
{
    switch (p.type) {
    case QChartPrimitive::Type::Point:
        return p.cartA;
    case QChartPrimitive::Type::Line:
        return p.cartB;
    case QChartPrimitive::Type::Polygon:
    case QChartPrimitive::Type::Path:
    case QChartPrimitive::Type::TriangleMesh:
    case QChartPrimitive::Type::TriangleFan:
    case QChartPrimitive::Type::TriangleStrip:
        return p.cartVerts.isEmpty() ? p.cartA : p.cartVerts.last();
    case QChartPrimitive::Type::Rect:
    case QChartPrimitive::Type::Ellipse:
        return QVector3D(static_cast<float>(p.cartRect.center().x()),
                         static_cast<float>(p.cartRect.center().y()), 0.0f);
    }
    return p.cartA;
}

bool QChartRenderer::hybridResolveFreeLabelAnchor(const QChartScene& scene,
                                                 QChartTextLabel& label) const
{
    // ★ 旁路函数：默认不被正常渲染路径调用（见头文件注释）。启用属"混合后端"阶段内容。
    if (label.hasExplicitAnchor() || label.refPrimitiveId != -1) return false;
    if (label.sourceId < 0) return false;

    const QChartAbstractProjection* proj = scene.projection;
    if (!proj) return false;

    // 组尾图元：同 sourceId 的最后一个（GL 粗裁=全可见 → "最后一个"即"最后可见"）
    int tail = -1;
    for (int i = 0; i < scene.primitives.size(); ++i)
        if (scene.primitives[i].sourceId == label.sourceId) tail = i;
    if (tail < 0) return false;

    label.cartesianAnchor = proj->toCartesian(qchartGroupTailAnchorNumeric(scene.primitives[tail]));
    label.visible = anchorVisibleInPlotArea(scene, label.cartesianAnchor);
    return true;
}

void QChartRenderer::drawLabel(QPainter& painter,
                               const QRectF& plotArea,
                               const QPointF& pixelAnchor,
                               const QString& text,
                               const QColor& color,
                               qreal fontSize,
                               Qt::Alignment alignment)
{
    if (text.isEmpty()) return;

    QFont font = painter.font();
    font.setPointSizeF(fontSize);
    painter.setFont(font);
    painter.setPen(color);

    QFontMetrics fm(font);
    qreal textW = fm.horizontalAdvance(text);
    qreal textH = fm.height();
    const qreal pad = 3.0;
    QSizeF textSize(textW + pad * 2, textH + pad * 2);

    QPointF textPos;
    bool autoAvoid = (alignment == Qt::AlignCenter);

    if (autoAvoid) {
        const qreal distLeft   = pixelAnchor.x() - plotArea.left();
        const qreal distRight  = plotArea.right() - pixelAnchor.x();
        const qreal distTop    = pixelAnchor.y() - plotArea.top();
        const qreal distBottom = plotArea.bottom() - pixelAnchor.y();

        // t75 契约（见 include/core/QChartRenderer.h）：索引 0=Right 1=Bottom 2=Left 3=Top
        //   ① 绝对容差 ε=1px 判平（旧实现用严格 `>`，打平时结果由比较顺序隐式决定）
        //   ② 打平走写死优先序：右 → 下 → 左 → 上
        //   ③ 迟滞：上一帧的边若未"明显更差"（差值 ≤ 8px）则保持，避免边缘处来回摆
        const qreal margins[4] = { distRight, distBottom, distLeft, distTop };   // 索引即优先序
        qreal maxMargin = margins[0];
        for (int i = 1; i < 4; ++i) maxMargin = qMax(maxMargin, margins[i]);
        int best = 0;
        for (int i = 0; i < 4; ++i) {
            if (margins[i] >= maxMargin - kAvoidTieEpsPx) { best = i; break; }   // 优先序内首个"打平"者
        }

        const QString avoidKey = labelAvoidKey(text, fontSize);
        int chosen = best;
        const auto prevIt = m_labelAvoid.constFind(avoidKey);
        if (prevIt != m_labelAvoid.constEnd() && prevIt->side >= 0
            && margins[prevIt->side] >= margins[best] - kAvoidHysteresisPx) {
            chosen = prevIt->side;   // 迟滞生效：另一侧未"明显更大"（≤8px）→ 保持上一帧的边
        }

        LabelAvoidState& st = m_labelAvoid[avoidKey];
        st.side = chosen;
        st.seq = ++m_labelAvoidSeq;
        pruneAvoidState();           // ③ 状态有界：LRU 上限（标签消失/场景重建不会无界增长）

        switch (chosen) {
        case 0: textPos = pixelAnchor + QPointF(pad, -textSize.height()/2); break;              // Right
        case 1: textPos = pixelAnchor + QPointF(-textSize.width()/2, pad); break;                // Bottom
        case 2: textPos = pixelAnchor + QPointF(-textSize.width() - pad, -textSize.height()/2); break; // Left
        default: textPos = pixelAnchor + QPointF(-textSize.width()/2, -textSize.height() - pad); break; // Top
        }
    } else {
        if (alignment & Qt::AlignLeft) {
            textPos = pixelAnchor + QPointF(-textSize.width() - pad, -textSize.height()/2);
        } else if (alignment & Qt::AlignRight) {
            textPos = pixelAnchor + QPointF(pad, -textSize.height()/2);
        } else if (alignment & Qt::AlignTop) {
            textPos = pixelAnchor + QPointF(-textSize.width()/2, -textSize.height() - pad);
        } else if (alignment & Qt::AlignBottom) {
            textPos = pixelAnchor + QPointF(-textSize.width()/2, pad);
        } else {
            textPos = pixelAnchor + QPointF(-textSize.width()/2, -textSize.height()/2);
        }
    }

    QRectF textRect(textPos, textSize);
    if (!plotArea.contains(textRect)) {
        if (textRect.left() < plotArea.left())
            textRect.moveLeft(plotArea.left() + pad);
        if (textRect.right() > plotArea.right())
            textRect.moveRight(plotArea.right() - pad);
        if (textRect.top() < plotArea.top())
            textRect.moveTop(plotArea.top() + pad);
        if (textRect.bottom() > plotArea.bottom())
            textRect.moveBottom(plotArea.bottom() - pad);
        if (textRect.width() < textSize.width() * 0.5 ||
            textRect.height() < textSize.height() * 0.5) {
            textRect = QRectF(plotArea.center() - QPointF(textSize.width()/2, textSize.height()/2),
                              textSize);
        }
    }

    painter.drawText(textRect, Qt::AlignCenter, text);
}

void QChartRenderer::drawLabels(QChartScene& scene, QPaintDevice* device)
{
    if (!device) return;

    const QChartAbstractCamera* camera = scene.camera;
    if (!camera) return;

    const QRectF& plotArea = scene.plotArea;

    QPainter painter(device);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipRect(plotArea);

    for (const QChartTextLabel& label : scene.labels) {
        if (!label.visible) continue;

        QChartProjectedPoint pp = camera->project(label.cartesianAnchor, plotArea);//Cartesian -> Pixel

        // t73②：二次包含判定**只对 tier1（显式锚点）保留** —— drawLabel 会把文字框钳制进绘图区，
        // 若不判定，落在视图外的显式标签会被"挤"到边缘说谎。tier2/tier3 的可见性已由**同帧**裁剪
        // 结果导出（visibility[] / 同组组尾可见性），此处再判一次会因锚点恰落边界（1e-5 px 级浮点噪声）
        // 把整组标签丢掉（t70 §3a）。按用户裁定不加容差（fit 已保证视图内元素落在绘图区内）。
        // ★ 生效路径：二维 CPU = QPainterChartRenderer::drawLabels2D；GL = QOpenGLChartRenderer::drawLabels
        //   （两者均已同步同一策略；本基类实现为"未覆写 drawLabels 的子类"兜底，语义保持一致）。
        if (label.hasExplicitAnchor() && !plotArea.contains(pp.screen)) continue;

        drawLabel(painter, plotArea, pp.screen, label.text, label.color,
                  label.fontSize, label.alignment);
    }
}