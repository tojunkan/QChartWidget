// test_axis_pipeline.cpp —— S0 轴渲染管线单元测试（见头文件注释）
#include "test_axis_pipeline.h"

#include <QtTest>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <QOpenGLContext>
#include <cmath>
#include <memory>

#include "QPainterChartRenderer.h"
#include "QOpenGLChartRenderer.h"
#include "QValueAxis.h"
#include "QChartScene.h"
#include "QChartCamera.h"
#include "QCartesianProjection.h"
#include "QPolarProjection.h"

namespace {

enum class ProjectionKind { Cartesian, Polar };

constexpr qreal kViewLo = -10.0;
constexpr qreal kViewHi =  10.0;   // 视图窗口 [-10,10]²
constexpr int    kSize  = 400;     // 方形画布/plotArea

/// 轴管线场景夹具：QValueAxis×2 + 投影 + 2D 相机 + 场景
struct AxisFixture {
    QChartCamera camera;
    QValueAxis dim0Axis;        // Cartesian: X（dim0）；Polar: 角度（环）
    QValueAxis dim1Axis;        // Cartesian: Y（dim1）；Polar: 半径（径向）
    std::unique_ptr<QChartProjection> projection;
    QChartScene scene;

    explicit AxisFixture(ProjectionKind kind)
        : dim0Axis(nullptr, Qt::AlignBottom)
        , dim1Axis(nullptr, Qt::AlignLeft)
    {
        dim0Axis.setTickCount(5);
        dim1Axis.setTickCount(5);
        dim0Axis.setColor(Qt::black);
        dim1Axis.setColor(Qt::black);

        camera.setViewRect(QRectF(kViewLo, kViewLo, kViewHi - kViewLo, kViewHi - kViewLo));

        if (kind == ProjectionKind::Cartesian)
            projection = std::make_unique<QCartesianProjection>();
        else
            projection = std::make_unique<QPolarProjection>();

        scene.camera = &camera;
        scene.projection = projection.get();
        scene.plotArea = QRectF(0, 0, kSize, kSize);
        scene.backgroundColor = Qt::white;
    }

    /// 组装场景：轴脊（+网格线）+ 刻度标签
    /// 语义（与 QChartLayer::drawGrid 的"tick 作 offset 反复 drawAtPosition"一致）：
    ///   grid=false → 只画两主轴脊（X@y=0 与 Y@x=0 / 外环 r=10 与 θ=0 径向脊）
    ///   grid=true  → 在另一轴每个 tick 的 offset 处再画主脊（笛卡尔：纵横网格线；
    ///                极坐标：θ tick 的径向辐条 + r tick 的同心环）；网格脊不带标签
    void build(bool grid, bool labels)
    {
        // 批次1：夹具 bool 语义映射 true→Tickwise / false→None（Single 由标签契约专项测试覆盖）
        const QChartAxis::LabelMode labelMode = labels ? QChartAxis::LabelMode::Tickwise
                                                       : QChartAxis::LabelMode::None;
        scene.primitives.clear();
        scene.labels.clear();

        if (projection->type() == QChartAbstractProjection::CoordinateSystem::Cartesian) {
            // 轴脊：X 沿 dim0 扫 y=0；Y 沿 dim1 扫 x=0
            dim0Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 0, scene, 72, labelMode);
            dim1Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 1, scene, 72, labelMode);

            if (grid) {
                const QVector<qreal> ticksY = dim1Axis.tickValues(kViewLo, kViewHi);
                for (qreal yt : ticksY) {
                    if (qAbs(yt) < 1e-9) continue;      // y=0 即 X 轴脊，跳过
                    dim0Axis.drawAtPosition(kViewLo, kViewHi, yt, 0.0, 0, scene, 8, QChartAxis::LabelMode::None);
                }
                const QVector<qreal> ticksX = dim0Axis.tickValues(kViewLo, kViewHi);
                for (qreal xt : ticksX) {
                    if (qAbs(xt) < 1e-9) continue;      // x=0 即 Y 轴脊，跳过
                    dim1Axis.drawAtPosition(kViewLo, kViewHi, xt, 0.0, 1, scene, 8, QChartAxis::LabelMode::None);
                }
            }
        } else {
            // 轴脊：外环 r=10（dim0=θ 0..360）；径向脊 θ=0（dim1=r 0..10）
            dim0Axis.drawAtPosition(0.0, 360.0, 10.0, 0.0, 0, scene, 90, labelMode);
            dim1Axis.drawAtPosition(0.0, 10.0, 0.0, 0.0, 1, scene, 8, labelMode);

            if (grid) {
                const QVector<qreal> ticksR = dim1Axis.tickValues(0.0, 10.0);
                for (qreal r : ticksR) {
                    if (r < 1e-9 || qAbs(r - 10.0) < 1e-9) continue;  // r=10 即外环，跳过
                    dim0Axis.drawAtPosition(0.0, 360.0, r, 0.0, 0, scene, 90, QChartAxis::LabelMode::None);
                }
                const QVector<qreal> ticksTheta = dim0Axis.tickValues(0.0, 360.0);
                for (qreal th : ticksTheta) {
                    if (qAbs(th) < 1e-9) continue;      // θ=0 即径向脊，跳过
                    dim1Axis.drawAtPosition(0.0, 10.0, th, 0.0, 1, scene, 8, QChartAxis::LabelMode::None);
                }
            }
        }
    }
};

/// 图元 Numeric → 像素（CPU 约定：row 顶→底；仅用于选取采样点）
QPoint pixOf(qreal x, qreal y)
{
    const qreal scale = kSize / (kViewHi - kViewLo);   // 20px/单位
    return QPoint(qRound((x - kViewLo) * scale), qRound((kViewHi - y) * scale));
}

QImage renderCpu(AxisFixture& f)
{
    QImage img(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainterChartRenderer renderer;
    renderer.render(f.scene, &img);
    return img;
}

bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}

int inkCount(const QImage& img, int cx, int cy, int radius)
{
    int n = 0;
    for (int y = qMax(0, cy - radius); y <= qMin(img.height() - 1, cy + radius); ++y) {
        for (int x = qMax(0, cx - radius); x <= qMin(img.width() - 1, cx + radius); ++x) {
            if (isInk(img.pixelColor(x, y))) ++n;
        }
    }
    return n;
}

int inkCountAll(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}

} // namespace

// ===== Cartesian：轴脊与刻度点真实落屏 =====
void TestAxisPipeline::cartesianSpineTicksRender()
{
    AxisFixture f(ProjectionKind::Cartesian);
    f.build(/*grid=*/false, /*labels=*/false);

    QVERIFY2(f.scene.primitives.size() > 10, "drawAtPosition 应提交轴脊与刻度图元");
    QCOMPARE(f.scene.labels.size(), 0);

    const QImage img = renderCpu(f);
    QVERIFY2(inkCountAll(img) > 300, "轴脊（X+Y 贯穿线）应产生大量非白像素（非空图冒烟）");

    // X 轴脊：cart(-2.5, 0) → 像素 (150, 200)
    const QPoint px = pixOf(-2.5, 0.0);
    QVERIFY2(inkCount(img, px.x(), px.y(), 3) > 0,
             qPrintable(QString("X 轴脊未落屏 @%1,%2").arg(px.x()).arg(px.y())));
    // Y 轴脊：cart(0, 5) → 像素 (200, 100)
    const QPoint py = pixOf(0.0, 5.0);
    QVERIFY2(inkCount(img, py.x(), py.y(), 3) > 0,
             qPrintable(QString("Y 轴脊未落屏 @%1,%2").arg(py.x()).arg(py.y())));
    // 主刻度中心点：tick x=4（QValueAxis -10..10 → -8,-4,0,4,8）→ 像素 (280,200)
    const QPoint pt = pixOf(4.0, 0.0);
    QVERIFY2(inkCount(img, pt.x(), pt.y(), 3) > 0,
             qPrintable(QString("X 轴主刻度点未落屏 @%1,%2").arg(pt.x()).arg(pt.y())));
    // 刻度点必须“超出”轴脊单线才算真验到 7 点刻度图元（轴脊墨迹会覆盖刻度中心，
    // 仅查中心点无法区分）→ 抽查垂直臂 ±6px（0.3 数据单位 = TICK_LENGTH×轴长 20）
    const QPoint ptArmA(pt.x(), pt.y() - 6);
    const QPoint ptArmB(pt.x(), pt.y() + 6);
    QVERIFY2(inkCount(img, ptArmA.x(), ptArmA.y(), 2) > 0,
             qPrintable(QString("X 轴刻度垂直臂未落屏（疑似刻度点图元缺失）@%1,%2")
                        .arg(ptArmA.x()).arg(ptArmA.y())));
    QVERIFY2(inkCount(img, ptArmB.x(), ptArmB.y(), 2) > 0,
             qPrintable(QString("X 轴刻度垂直臂未落屏（疑似刻度点图元缺失）@%1,%2")
                        .arg(ptArmB.x()).arg(ptArmB.y())));
}

// ===== Cartesian：网格 × 标签 组合 =====
void TestAxisPipeline::cartesianGridLabelCombos()
{
    for (bool grid : {false, true}) {
        for (bool labels : {false, true}) {
            AxisFixture f(ProjectionKind::Cartesian);
            f.build(grid, labels);

            QCOMPARE(f.scene.labels.size(), labels ? 10 : 0);
            const QImage img = renderCpu(f);
            QVERIFY2(inkCountAll(img) > 300, "任一组合下轴脊应可见");

            // 轴脊始终可见：中心交点 cart(0,0) → (200,200)
            const QPoint pc = pixOf(0.0, 0.0);
            QVERIFY2(inkCount(img, pc.x(), pc.y(), 3) > 0, "轴脊中心交点应落屏");

            // 网格采样点：垂直网格线 x=4 与水平网格线 y=6 上的点 cart(4,6) → (280,80)
            // （离轴脊与标签排版区都足够远，网格关→白，网格开→有墨）
            const QPoint pg = pixOf(4.0, 6.0);
            const int n = inkCount(img, pg.x(), pg.y(), 2);
            if (grid)
                QVERIFY2(n > 0, qPrintable(QString("网格开：%1,%2 应有网格线墨迹").arg(pg.x()).arg(pg.y())));
            else
                QVERIFY2(n == 0, qPrintable(QString("网格关：%1,%2 应为空白").arg(pg.x()).arg(pg.y())));

            if (labels) {
                AxisFixture off(ProjectionKind::Cartesian);
                off.build(grid, false);
                const QImage imgOff = renderCpu(off);
                QVERIFY2(inkCountAll(img) > inkCountAll(imgOff) + 20,
                         "标签开时应比标签关多出文字像素");
            }
        }
    }
}

// ===== Polar：轴脊（外环 + 径向）与刻度点真实落屏 =====
void TestAxisPipeline::polarSpineTicksRender()
{
    AxisFixture f(ProjectionKind::Polar);
    f.build(/*grid=*/false, /*labels=*/false);

    QVERIFY2(f.scene.primitives.size() > 40, "drawAtPosition 应提交外环/径向脊图元");
    QCOMPARE(f.scene.labels.size(), 0);

    const QImage img = renderCpu(f);
    QVERIFY2(inkCountAll(img) > 400, "极坐标轴脊（环+径向线）应产生大量非白像素");

    // 径向脊：cart(2, 0) → (240, 200)（θ=0 半轴，r=2 刻度点亦在此）
    const QPoint pr = pixOf(2.0, 0.0);
    QVERIFY2(inkCount(img, pr.x(), pr.y(), 3) > 0,
             qPrintable(QString("径向脊/刻度未落屏 @%1,%2").arg(pr.x()).arg(pr.y())));
    // 外环：r=10、θ=45° → cart(7.071, 7.071) → (341,59)
    const QPoint po = pixOf(7.0710678, 7.0710678);
    QVERIFY2(inkCount(img, po.x(), po.y(), 4) > 0,
             qPrintable(QString("外环未落屏 @%1,%2").arg(po.x()).arg(po.y())));
    // 环轴 θ 刻度点图元（θ=80 主刻度，7 点形态的径向内臂落在 r=10−5.4=4.6、
    // θ=80° → cart(0.799, 4.530) → (216,109)）：远离外环/径向脊，能区分刻度点与脊线
    const QPoint ptArm = pixOf(0.7989385, 4.5295340);
    QVERIFY2(inkCount(img, ptArm.x(), ptArm.y(), 2) > 0,
             qPrintable(QString("极坐标环轴刻度点未落屏（疑似刻度点图元缺失）@%1,%2")
                        .arg(ptArm.x()).arg(ptArm.y())));
}

// ===== Polar：网格 × 标签 组合 =====
void TestAxisPipeline::polarGridLabelCombos()
{
    for (bool grid : {false, true}) {
        for (bool labels : {false, true}) {
            AxisFixture f(ProjectionKind::Polar);
            f.build(grid, labels);

            // 环轴 5 个 θ 刻度 + 径向轴 6 个 r 刻度（含原点/外端）→ 11 个标签
            QCOMPARE(f.scene.labels.size(), labels ? 11 : 0);
            const QImage img = renderCpu(f);
            QVERIFY2(inkCountAll(img) > 400, "任一组合下极轴脊应可见");

            // 径向脊 + 原点：cart(2,0) → (240,200)
            const QPoint pr = pixOf(2.0, 0.0);
            QVERIFY2(inkCount(img, pr.x(), pr.y(), 3) > 0, "径向脊应始终落屏");

            // 网格采样点 A：同心环 r=4、θ=45° → cart(2.828,2.828) → (257,143)
            const QPoint pgA = pixOf(2.8284271, 2.8284271);
            const int na = inkCount(img, pgA.x(), pgA.y(), 2);
            if (grid)
                QVERIFY2(na > 0, qPrintable(QString("网格开：环采样 %1,%2 应有墨迹").arg(pgA.x()).arg(pgA.y())));
            else
                QVERIFY2(na == 0, qPrintable(QString("网格关：环采样 %1,%2 应为空白").arg(pgA.x()).arg(pgA.y())));

            // 网格采样点 B：辐条 θ=80°、r=4 → cart(0.695,3.939) → (214,121)
            const QPoint pgB = pixOf(0.6945927, 3.9392310);
            const int nb = inkCount(img, pgB.x(), pgB.y(), 2);
            if (grid)
                QVERIFY2(nb > 0, qPrintable(QString("网格开：辐条采样 %1,%2 应有墨迹").arg(pgB.x()).arg(pgB.y())));
            else
                QVERIFY2(nb == 0, qPrintable(QString("网格关：辐条采样 %1,%2 应为空白").arg(pgB.x()).arg(pgB.y())));

            if (labels) {
                AxisFixture off(ProjectionKind::Polar);
                off.build(grid, false);
                const QImage imgOff = renderCpu(off);
                QVERIFY2(inkCountAll(img) > inkCountAll(imgOff) + 20,
                         "标签开时应比标签关多出文字像素");
            }
        }
    }
}

// ============================================================
// 批次1：标签契约内核（LabelMode 三态 + 锚点三级优先级 + NaN 哨兵）
// ============================================================

namespace {

/// 标签契约夹具：手工装配点图元 + 标签（不经轴），直接验证 renderer 步骤 2 的锚点解析
struct LabelFixture {
    QChartCamera camera;
    std::unique_ptr<QChartProjection> projection;
    QChartScene scene;

    LabelFixture()
        : projection(std::make_unique<QCartesianProjection>())
    {
        camera.setViewRect(QRectF(kViewLo, kViewLo, kViewHi - kViewLo, kViewHi - kViewLo));
        scene.camera = &camera;
        scene.projection = projection.get();
        scene.plotArea = QRectF(0, 0, kSize, kSize);
        scene.backgroundColor = Qt::white;
    }

    /// 追加一个点图元（sourceId 供自由标签分组），返回其下标
    int addPoint(const QVector3D& num, int sourceId)
    {
        QChartPrimitive p;
        p.type = QChartPrimitive::Type::Point;
        p.numA = num;
        p.color = Qt::black;
        p.sourceId = sourceId;
        scene.primitives.append(p);
        scene.maxSourceId = qMax(scene.maxSourceId, sourceId);
        return scene.primitives.size() - 1;
    }

    int addLabel(const QChartTextLabel& label)
    {
        scene.labels.append(label);
        return scene.labels.size() - 1;
    }

    QChartTextLabel& label(int i) { return scene.labels[i]; }
};

/// t31 旁路探针：暴露基类 protected 的 hybridResolveFreeLabelAnchor（仅测试用；
/// 产品正常渲染路径不得调用该旁路——见 QChartRenderer.h 的"未来混合后端预研 · 当前不启用"）
class HybridBypassProbe : public QPainterChartRenderer {
public:
    bool probeHybridFreeLabel(const QChartScene& scene, QChartTextLabel& label)
    {
        return hybridResolveFreeLabelAnchor(scene, label);
    }
};

bool vecNear(const QVector3D& a, const QVector3D& b, float eps = 1e-4f)
{
    return qAbs(a.x() - b.x()) <= eps && qAbs(a.y() - b.y()) <= eps
           && qAbs(a.z() - b.z()) <= eps;
}

/// CPU 后端步骤 2（+绘制）：每次新建 renderer（m_viewDirty=true）保证重新解析标签
void resolveCpu(LabelFixture& f)
{
    QImage img(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainterChartRenderer renderer;
    renderer.render(f.scene, &img);
}

/// GL 后端步骤 2（+标签覆盖层）：无 GL context 时图元绘制跳过、cull 标签解析仍执行
void resolveGl(LabelFixture& f)
{
    if (QOpenGLContext* ctx = QOpenGLContext::currentContext())
        ctx->doneCurrent();   // 确保走"无 context"分支（对应 qWarning 由调用方 ignoreMessage）
    QImage img(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QOpenGLChartRenderer renderer;
    QTest::ignoreMessage(QtWarningMsg, "No current OpenGL context!");
    renderer.render(f.scene, &img);
}

} // namespace

// ===== #5 LabelMode 三态：标签数量 + Single=中间刻度 + 默认参=Tickwise =====
void TestAxisPipeline::labelModeThreeStates()
{
    const auto labelCount = [](QChartAxis::LabelMode mode, bool useDefaultArg) {
        AxisFixture f(ProjectionKind::Cartesian);
        if (useDefaultArg)
            f.dim0Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 0, f.scene, 72);
        else
            f.dim0Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 0, f.scene, 72, mode);
        return f.scene.labels.size();
    };

    QCOMPARE(labelCount(QChartAxis::LabelMode::None, false), 0);      // None：零标签
    QCOMPARE(labelCount(QChartAxis::LabelMode::Single, false), 1);    // Single：1 个代表标签
    QCOMPARE(labelCount(QChartAxis::LabelMode::Tickwise, false), 5);  // Tickwise：每刻度一标签
    QCOMPARE(labelCount(QChartAxis::LabelMode::Tickwise, true), 5);   // 默认参保持现状语义（=Tickwise）

    // Single 代表标签 = 中间刻度（tickCount=5 → 刻度 -8,-4,0,4,8 → 中间 = 0）
    AxisFixture f(ProjectionKind::Cartesian);
    f.dim0Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 0, f.scene, 72,
                              QChartAxis::LabelMode::Single);
    QCOMPARE(f.scene.labels.size(), 1);
    QCOMPARE(f.scene.labels[0].text, QStringLiteral("0"));
    QVERIFY2(vecNear(f.scene.labels[0].numericAnchor, QVector3D(0, 0, 0)),
             "Single 标签锚点应为中间刻度 (0,0,0)");

    // None 只关标签，不改图元（1 条轴脊 Path + 每刻度 7 点 = 36）
    AxisFixture fNone(ProjectionKind::Cartesian);
    fNone.dim0Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 0, fNone.scene, 72,
                                  QChartAxis::LabelMode::None);
    QCOMPARE(fNone.scene.labels.size(), 0);
    QCOMPARE(fNone.scene.primitives.size(), 1 + 7 * 5);

    // ★ "单标签"取值口径冻结：刻度列表下标 size/2（元素个数为偶数时取偏上的中位）；
    //   该位置文字为空则不生成标签。跨 tickCount 验证（含偶数刻度数场景）。
    for (int tickCount : {4, 5, 6, 7}) {
        AxisFixture fm(ProjectionKind::Cartesian);
        fm.dim0Axis.setTickCount(tickCount);
        fm.dim0Axis.setColor(Qt::black);

        const QVector<qreal> ticks = fm.dim0Axis.tickValues(kViewLo, kViewHi);
        const QStringList labels = fm.dim0Axis.tickLabels(ticks);
        QVERIFY(ticks.size() >= 2);
        const int midIdx = ticks.size() / 2;   // 偶数取上中位（整数除法）

        fm.dim0Axis.drawAtPosition(kViewLo, kViewHi, 0.0, 0.0, 0, fm.scene, 72,
                                   QChartAxis::LabelMode::Single);
        QVERIFY2(fm.scene.labels.size() <= 1, "Single 每脊至多 1 个标签");
        if (fm.scene.labels.isEmpty()) {
            QVERIFY2(labels.value(midIdx).isEmpty(),
                     "仅当该位置文字为空时可不生成标签");
            continue;
        }
        QCOMPARE(fm.scene.labels[0].text, labels.value(midIdx));
        QVERIFY2(vecNear(fm.scene.labels[0].numericAnchor,
                         QVector3D(ticks.value(midIdx), 0, 0)),
                 "Single 标签锚点应为刻度列表下标 size/2 处的刻度");
    }
}

// ===== #6 锚点三级优先级：tier1 显式锚点 > tier2 refId 绑定 > tier3 自由标签 =====
void TestAxisPipeline::anchorTierPriority()
{
    // 哨兵默认值：numericAnchor 全 NaN（不是 0,0,0）
    QChartTextLabel fresh;
    QVERIFY2(std::isnan(fresh.numericAnchor.x()) && std::isnan(fresh.numericAnchor.y())
                 && std::isnan(fresh.numericAnchor.z()),
             "numericAnchor 默认应为全 NaN 哨兵（防 0,0,0 被误判为显式锚点）");
    QVERIFY2(!fresh.hasExplicitAnchor(), "默认标签不应被判定为显式锚点");

    LabelFixture f;
    const int i0 = f.addPoint(QVector3D(2, 4, 0), 7);     // 源7 组内前一个图元
    const int i1 = f.addPoint(QVector3D(-5, -5, 0), 7);   // 源7 组尾（自由标签取它）
    const int i2 = f.addPoint(QVector3D(2, 4, 0), 8);     // 源8：tier2 绑定目标
    Q_UNUSED(i0);

    // tier1：显式锚点 + refId 指向其他图元 → 锚点取显式值（不被 refId 覆盖）
    QChartTextLabel l1; l1.text = "t1"; l1.numericAnchor = QVector3D(3, 3, 0); l1.refPrimitiveId = i1;
    // tier1：显式锚点 + 自由标签 refId=-1 → 仍取显式锚点（不走组尾算法）
    QChartTextLabel l2; l2.text = "t1f"; l2.numericAnchor = QVector3D(-3, 5, 0);
    l2.refPrimitiveId = -1; l2.sourceId = 7;
    // tier2：锚点全 NaN（默认哨兵）+ refId≥0 → 继承该图元坐标与可见性
    QChartTextLabel l3; l3.text = "t2"; l3.refPrimitiveId = i2;
    // tier3：锚点全 NaN + refId=-1 → 同 sourceId 组尾最后可见图元
    QChartTextLabel l4; l4.text = "t3"; l4.refPrimitiveId = -1; l4.sourceId = 7;

    const int a = f.addLabel(l1), b = f.addLabel(l2), c = f.addLabel(l3), d = f.addLabel(l4);
    resolveCpu(f);

    // tier1：显式锚点生效且可见（落在 plotArea 内）
    QVERIFY2(vecNear(f.label(a).cartesianAnchor, QVector3D(3, 3, 0)),
             "tier1 应使用显式锚点 (3,3,0)，而非 refId 图元坐标 (-5,-5,0)");
    QVERIFY(f.label(a).visible);
    QVERIFY2(vecNear(f.label(b).cartesianAnchor, QVector3D(-3, 5, 0)),
             "tier1 自由标签也应使用显式锚点");
    QVERIFY(f.label(b).visible);
    // tier2：继承绑定图元坐标
    QVERIFY2(vecNear(f.label(c).cartesianAnchor, QVector3D(2, 4, 0)),
             "tier2 应继承绑定图元坐标");
    QVERIFY(f.label(c).visible);
    // tier3：组尾最后可见图元 = 源7 的 i1(-5,-5)
    QVERIFY2(vecNear(f.label(d).cartesianAnchor, QVector3D(-5, -5, 0)),
             "tier3 应取同 sourceId 组尾最后可见图元");
    QVERIFY(f.label(d).visible);
}

// ===== #7 NaN 分量判定 + 显式锚点可见性 + 负例 =====
void TestAxisPipeline::anchorVisibilityRules()
{
    LabelFixture f;
    const int iVis = f.addPoint(QVector3D(2, 4, 0), 7);          // 视窗内
    f.addPoint(QVector3D(100, 100, 0), 8);                       // 视窗外（源8 唯一图元）

    // ① NaN 分量规则：仅一个分量非 NaN 即显式锚点（tier1），不被 refId 覆盖
    QChartTextLabel partial;
    partial.text = "partial-nan";
    partial.numericAnchor = QVector3D(static_cast<float>(qQNaN()), 4.0f, 0.0f);
    partial.refPrimitiveId = iVis;   // 若误判 tier2 会得到 (2,4,0)
    QVERIFY2(partial.hasExplicitAnchor(), "部分分量 NaN 仍应判定为显式锚点");

    // ② 显式锚点但投影落在 plotArea 外 → 不可见
    QChartTextLabel outside; outside.text = "outside"; outside.numericAnchor = QVector3D(50, 0, 0);
    // ③ 自由标签：同 sourceId 组内无可见图元 → 不可见
    QChartTextLabel orphan; orphan.text = "orphan"; orphan.refPrimitiveId = -1; orphan.sourceId = 8;
    // ④ 绑定越界（refId ≥ primitives.size()）→ 不可见
    QChartTextLabel oob; oob.text = "oob";
    oob.refPrimitiveId = static_cast<int>(f.scene.primitives.size()) + 3;
    // ⑤ 非法负绑定（< -1）→ 不可见
    QChartTextLabel bogus; bogus.text = "bogus"; bogus.refPrimitiveId = -7;

    const int ip = f.addLabel(partial), io = f.addLabel(outside), ir = f.addLabel(orphan),
              ib = f.addLabel(oob), ig = f.addLabel(bogus);
    resolveCpu(f);

    // ① tier1 命中：锚点走 toCartesian(NaN,4,0)（x=NaN），未继承图元 (2,4,0)；非有限投影 → 不可见
    QVERIFY2(std::isnan(f.label(ip).cartesianAnchor.x()),
             "部分 NaN 锚点应走 tier1（x=NaN），而非继承绑定图元 (2,4,0)");
    QVERIFY2(!f.label(ip).visible, "非有限投影的显式锚点应不可见");
    // ②
    QVERIFY(vecNear(f.label(io).cartesianAnchor, QVector3D(50, 0, 0)));
    QVERIFY2(!f.label(io).visible, "plotArea 外的显式锚点应不可见");
    // ③
    QVERIFY2(!f.label(ir).visible, "同组无可见图元 → 自由标签不可见（负例）");
    // ④⑤
    QVERIFY2(!f.label(ib).visible, "越界绑定应不可见");
    QVERIFY2(!f.label(ig).visible, "非法负绑定应不可见");

    // 正证（③）：把源8 图元移入视窗 → 新一轮解析后同组自由标签变可见
    f.scene.primitives[1].numA = QVector3D(1, 1, 0);
    resolveCpu(f);
    QVERIFY2(f.label(ir).visible, "源8 图元移入视窗后自由标签应可见（证明③判的是可见性而非 sourceId）");
    QVERIFY(vecNear(f.label(ir).cartesianAnchor, QVector3D(1, 1, 0)));
}

// ===== #8 GL 后端步骤 2：锚点三级优先级同契约（离屏 ctest 常驻） =====
void TestAxisPipeline::glAnchorTierResolution()
{
    LabelFixture f;
    const int iVis = f.addPoint(QVector3D(2, 4, 0), 7);   // 视窗内
    f.addPoint(QVector3D(100, 100, 0), 8);                // 视窗外：GL 无 CPU 图元裁剪

    QChartTextLabel t1; t1.text = "t1"; t1.numericAnchor = QVector3D(3, 3, 0); t1.refPrimitiveId = iVis;
    QChartTextLabel out; out.text = "out"; out.numericAnchor = QVector3D(50, 0, 0);
    QChartTextLabel bound; bound.text = "bound"; bound.refPrimitiveId = 1;   // 源8 视窗外图元
    // t31 恢复既定契约：GL 不渲染自由标签（tier3 一律不可见）
    QChartTextLabel freeLbl; freeLbl.text = "free"; freeLbl.refPrimitiveId = -1; freeLbl.sourceId = 7;
    QChartTextLabel oob; oob.text = "oob"; oob.refPrimitiveId = 99;

    const int a = f.addLabel(t1), b = f.addLabel(out), c = f.addLabel(bound),
              d = f.addLabel(freeLbl), e = f.addLabel(oob);
    resolveGl(f);

    // tier1（显式坐标）与 tier2（指向图元）行为与批次1 过审状态一致
    QVERIFY2(vecNear(f.label(a).cartesianAnchor, QVector3D(3, 3, 0)), "GL tier1：显式锚点生效");
    QVERIFY(f.label(a).visible);
    QVERIFY2(!f.label(b).visible, "GL tier1：plotArea 外显式锚点不可见");
    QVERIFY2(vecNear(f.label(c).cartesianAnchor, QVector3D(100, 100, 0)),
             "GL tier2：绑定图元 numA → Cartesian");
    QVERIFY2(f.label(c).visible, "GL tier2：图元可见性归 GPU 裁剪（本后端粗裁=可见）");
    QVERIFY2(!f.label(e).visible, "GL：越界绑定 → 不可见");

    // ★ 契约锁定（t31）：GL 纯 GPU 后端不渲染自由标签 —— 无论同组是否有可见图元
    QVERIFY2(f.label(d).refPrimitiveId == -1 && std::isnan(f.label(d).numericAnchor.x()),
             "契约锁定对象确为自由标签（全 NaN 锚点 + 不指向图元）");
    QVERIFY2(!f.label(d).visible,
             "GL 不渲染自由标签（tier3 一律不可见；既定已接受差异，t31 恢复）");

    // 负例：同 sourceId 组不存在 → 同样不可见
    QChartTextLabel orphan; orphan.text = "orphan"; orphan.refPrimitiveId = -1; orphan.sourceId = 42;
    const int g = f.addLabel(orphan);
    resolveGl(f);
    QVERIFY2(!f.label(g).visible, "GL 自由标签：同组无图元 → 不可见");

    // ---- 旁路隔离验证：预研实现保留且仍可用，但正常渲染路径不调用它 ----
    QChartScene probeScene = f.scene;          // 复用同一场景拷贝
    HybridBypassProbe probe;
    QChartTextLabel& freeInProbe = probeScene.labels[d];
    QVERIFY2(!freeInProbe.visible, "进入旁路前：自由标签在 GL 解析结果中不可见");
    QVERIFY2(probe.probeHybridFreeLabel(probeScene, freeInProbe),
             "hybrid 旁路保留可用（t31：实现不删除、仅隔离）");
    QVERIFY2(vecNear(freeInProbe.cartesianAnchor, QVector3D(2, 4, 0)),
             "hybrid 旁路：锚点=同 sourceId 组尾图元（源7 组尾 (2,4,0)）");
    QVERIFY2(freeInProbe.visible, "hybrid 旁路：锚点像素落在 plotArea 内 → 可见（预研语义）");
}

// ===== 批次2 C③：NaN 规则锁定（未设置必须写 NaN；{0,0,0} = 显式坐标（原点））=====
void TestAxisPipeline::nanAnchorRuleLocked()
{
    // 规则①：默认（未设置）= 全 NaN 哨兵
    QChartTextLabel fresh;
    QVERIFY2(std::isnan(fresh.numericAnchor.x()) && std::isnan(fresh.numericAnchor.y())
                 && std::isnan(fresh.numericAnchor.z()),
             "未设置坐标必须写 NaN（默认应为全 NaN 哨兵）");
    QVERIFY2(!fresh.hasExplicitAnchor(), "未设置坐标不得被判为显式锚点");

    // 规则②：{0,0,0} 是显式坐标（原点），不是"未设置"
    QChartTextLabel origin;
    origin.text = "origin";
    origin.numericAnchor = QVector3D(0, 0, 0);
    QVERIFY2(origin.hasExplicitAnchor(),
             "numericAnchor = {0,0,0} 必须视为显式坐标（原点）");

    // 行为锁定：{0,0,0} 标签走 tier1（锚在原点），不被当作未设置而走 tier3 组尾算法
    LabelFixture f;
    f.addPoint(QVector3D(6, 6, 0), 7);          // 源7 组尾图元
    QChartTextLabel l = origin;
    l.refPrimitiveId = -1;
    l.sourceId = 7;                             // 若走 tier3 会锚到 (6,6,0)
    const int idx = f.addLabel(l);
    resolveCpu(f);

    QVERIFY2(vecNear(f.label(idx).cartesianAnchor, QVector3D(0, 0, 0)),
             "{0,0,0} 应走 tier1 锚到原点");
    QVERIFY2(!vecNear(f.label(idx).cartesianAnchor, QVector3D(6, 6, 0)),
             "{0,0,0} 不得被当作未设置而走组尾定位");
    QVERIFY(f.label(idx).visible);               // 原点在 plotArea 内
}
