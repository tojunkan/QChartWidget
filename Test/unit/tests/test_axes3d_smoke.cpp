// test_axes3d_smoke.cpp —— 批次 B1/B2：3D 轴渲染冒烟（CPU offscreen）
// 组装：QChartLayer3D（QChartAxes3D 编排器 + 3 轴 + 自带 QChartCamera3D 值成员）
//   → setDataBounds + collectPrimitives（drawAtPosition 生成 Numeric 图元入 m_scene）
//   → 3D 投影 + Camera3D → QPainterChartRenderer::render(scene3D, QImage) 像素断言。
// 批次2 B 覆盖：三网格模式（Box/FaceLine/Lattice）语义 + 面线退化安全 + Box 非直角投影回退。
#include "test_axes3d_smoke.h"

#include <QtTest>
#include <QImage>
#include <QColor>
#include <QRegularExpression>
#include <memory>

#include "QChartLayer3D.h"
#include "QPainterChartRenderer.h"
#include "QValueAxis.h"
#include "QCartesianProjection3D.h"
#include "QSphericalProjection3D.h"

namespace {
bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}

int inkAll(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}

/// 3x3 分块铺开计数（模式判别辅助：内容是否铺满多区）
int inkTiles(const QImage& img)
{
    int tiles = 0;
    for (int ty = 0; ty < 3; ++ty)
        for (int tx = 0; tx < 3; ++tx) {
            bool hit = false;
            for (int y = ty * 133; y < (ty + 1) * 133 && !hit; ++y)
                for (int x = tx * 133; x < (tx + 1) * 133 && !hit; ++x)
                    if (isInk(img.pixelColor(x, y))) hit = true;
            if (hit) ++tiles;
        }
    return tiles;
}

/// 3D 冒烟夹具：Layer3D + 三轴 + 数据盒 + 相机 + 场景上下文（投影非持有，生命周期由调用方保证）
struct Axis3DFixture {
    QChartLayer3D layer;
    QValueAxis ax, ay, az;
    QChartCamera3D* cam = nullptr;
    QChartScene scene;          // collect 后从 layer.scene3D() 拷贝（camera 指针指向 layer 成员）

    Axis3DFixture(const QChartProjection3D* proj, const QVector3D& mn, const QVector3D& mx)
        : ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft), az(nullptr, Qt::AlignBottom)
    {
        ax.setTickCount(5); ay.setTickCount(5); az.setTickCount(5);
        ax.setColor(Qt::black); ay.setColor(Qt::black); az.setColor(Qt::black);
        // 网格色加深（默认 220 浅灰被 isInk>40 阈值排除 → 网格/Box-Lattice 差异无法断言）
        layer.setGridColor(QColor(120, 120, 120));
        layer.setAxisX(&ax);
        layer.setAxisY(&ay);
        layer.setAxisZ(&az);
        layer.setProjection3D(proj);
        layer.setDataBounds(mn, mx);
        cam = layer.camera3D();
        const QVector3D pad = (mx - mn) * 0.25f;
        cam->setViewCube(QCube(mn - pad, mx + pad));
        cam->setYaw(45.0);
        cam->setPitch(30.0);
        cam->setRoll(0.0);
        layer.setScene3DProjection(proj);
        layer.setScene3DPlotArea(QRectF(0, 0, 400, 400));
        layer.setScene3DBackground(Qt::white);
    }

    /// 非恒等投影（球/柱）：viewCube 必须取世界包围盒而非数值盒
    void fitCameraToWorld(const QChartProjection3D& proj, const QVector3D& mn, const QVector3D& mx)
    {
        QCube wc = proj.computeViewCube(mn, mx);
        const QVector3D pad = (wc.max - wc.min) * 0.35f;
        wc = QCube(wc.min - pad, wc.max + pad);
        cam->setViewCube(wc);
        const qreal hd = (wc.max - wc.min).length() * 0.5;
        cam->setDistance(hd / qMax(qreal(0.05), qSin(qDegreesToRadians(cam->fov()) * 0.5)));
        cam->resetNearFar();
    }

    void collect(QChartLayer3D::GridMode mode)
    {
        layer.setGridMode(mode);
        layer.collectPrimitives();
        // B1 冒烟线框语义：GL 侧 depthTest 关闭=全边可见（以 prim.depth>1 标记 decor 批次，
        // 与 CPU painter 全绘制一致）；CPU 侧 depth 字段由相机每帧重算，不受影响。
        scene = layer.scene3D();
        for (QChartPrimitive& p : scene.primitives)
            p.depth = 2.0f;
    }

    QImage render(QChartLayer3D::GridMode mode)
    {
        collect(mode);
        QImage img(400, 400, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        QPainterChartRenderer r;
        r.render(scene, &img);
        return img;
    }
};
} // namespace

// ===== 批次2 B：三网格模式语义 + 出图 =====
void TestAxes3DSmoke::cpuThreeGridModes()
{
    QCartesianProjection3D proj;
    const QVector3D mn(-3, -3, -3), mx(3, 3, 3);

    Axis3DFixture fBox(&proj, mn, mx);
    const QImage imgBox = fBox.render(QChartLayer3D::GridMode::Box);
    const int boxPrims = fBox.scene.primitives.size();
    const int boxLabels = fBox.scene.labels.size();

    Axis3DFixture fFace(&proj, mn, mx);
    const QImage imgFace = fFace.render(QChartLayer3D::GridMode::FaceLine);
    const int facePrims = fFace.scene.primitives.size();
    const int faceLabels = fFace.scene.labels.size();

    Axis3DFixture fLat(&proj, mn, mx);
    const QImage imgLat = fLat.render(QChartLayer3D::GridMode::Lattice);
    const int latPrims = fLat.scene.primitives.size();
    const int latLabels = fLat.scene.labels.size();

    qInfo().noquote() << QString("3D smoke: Box prims=%1 labels=%2 ink=%3 tiles=%4 | "
                                 "FaceLine prims=%5 labels=%6 ink=%7 | Lattice prims=%8 labels=%9 ink=%10 tiles=%11")
        .arg(boxPrims).arg(boxLabels).arg(inkAll(imgBox)).arg(inkTiles(imgBox))
        .arg(facePrims).arg(faceLabels).arg(inkAll(imgFace))
        .arg(latPrims).arg(latLabels).arg(inkAll(imgLat)).arg(inkTiles(imgLat));

    // ① 盒模式：现有盒几何（12 边 + 底面网格 + 三主轴脊）+ 三主轴 Tickwise 标签
    QVERIFY2(boxPrims > 40, "盒模式应生成盒边/脊/刻度/网格图元");
    QVERIFY2(boxLabels > 0, "盒模式：三条主轴应按刻度逐个生成标签");
    QVERIFY2(inkAll(imgBox) > 200, "盒模式应产生大量墨迹");
    QVERIFY2(inkTiles(imgBox) >= 4, "盒模式应铺开多个画面区域");

    // ② 面线模式：只画那条退化安全的轴线（+刻度点/刻度标签）；无盒边、无网格
    QVERIFY2(facePrims > 0, "面线模式应画出安全轴线");
    QVERIFY2(facePrims < boxPrims, "面线模式只画一条轴线（图元应远少于盒模式）");
    QVERIFY2(faceLabels > 0, "面线模式：轴应按刻度逐个标注（Tickwise）");
    QVERIFY2(inkAll(imgFace) > 30, "面线模式应出墨（轴线 + 刻度标签）");

    // ③ 晶格模式：只画线，LabelMode::None —— 无任何文字
    QVERIFY2(latLabels == 0, "晶格模式不得生成任何标签（LabelMode::None）");
    QVERIFY2(latPrims > 40, "晶格模式应生成三主轴 + 三族网格 + 盒边");
    QVERIFY2(inkAll(imgLat) > inkAll(imgBox) + 100,
             "晶格（3 族网格）墨迹应显著多于盒（底面 2 族）");

    // ④ 面线为枚举默认模式（批次2 B）
    QChartLayer3D fresh;
    QCOMPARE(static_cast<int>(fresh.gridMode()),
             static_cast<int>(QChartLayer3D::GridMode::FaceLine));
}

// ===== 批次2 B：面线模式的退化安全（球坐标 θ=φ=0 一类安全位置；θ/φ 跨界）=====
void TestAxes3DSmoke::cpuFaceLineSafeOnSpherical()
{
    QSphericalProjection3D proj;   // dim0=r, dim1=θ(°), dim2=φ(°)

    // 安全固定值规则：0 在范围内取 0；否则取中点（θ/φ 跨界区间）
    QCOMPARE(QChartAxes3D::safeFixedValue(0.0, 90.0), 0.0);
    QCOMPARE(QChartAxes3D::safeFixedValue(-30.0, 30.0), 0.0);
    QCOMPARE(QChartAxes3D::safeFixedValue(10.0, 80.0), 45.0);
    QCOMPARE(QChartAxes3D::safeFixedValue(80.0, 10.0), 45.0);   // 反序输入同样稳健

    struct Case { QVector3D mn, mx; qreal expectTheta, expectPhi; };
    const Case cases[2] = {
        { QVector3D(1, 0, -30),  QVector3D(3, 90, 30),  0.0,  0.0 },   // θ/φ 均含 0
        { QVector3D(1, 10, -30), QVector3D(3, 80, 30), 45.0,  0.0 },   // θ 跨 0 但不含 0 → 中点
    };

    for (const Case& c : cases) {
        // 安全线段几何契约
        const QPair<QVector3D, QVector3D> seg = QChartAxes3D::faceLineSegment(c.mn, c.mx);
        QCOMPARE(seg.first.y(), c.expectTheta);
        QCOMPARE(seg.first.z(), c.expectPhi);
        QCOMPARE(seg.second.y(), c.expectTheta);
        QCOMPARE(seg.second.z(), c.expectPhi);
        QCOMPARE(seg.first.x(), c.mn.x());
        QCOMPARE(seg.second.x(), c.mx.x());

        Axis3DFixture f(&proj, c.mn, c.mx);
        f.fitCameraToWorld(proj, c.mn, c.mx);
        f.collect(QChartLayer3D::GridMode::FaceLine);

        // ① FACE-DEFERRED 记录项：面线模式只出线（Path/Point），不得有任何面片图元
        for (const QChartPrimitive& p : f.scene.primitives)
            QVERIFY2(p.type == QChartPrimitive::Type::Path || p.type == QChartPrimitive::Type::Point,
                     "FACE-DEFERRED：面线模式本阶段不画面（仅 Path/Point）");

        // ① 轴线 Path 全部顶点落在安全线上（dim1/dim2 固定）
        int pathCount = 0, tickCenters = 0;
        QVector<QVector3D> centerWorld;
        for (int i = 0; i < f.scene.primitives.size(); ++i) {
            const QChartPrimitive& p = f.scene.primitives[i];
            if (p.type == QChartPrimitive::Type::Path) {
                ++pathCount;
                for (const QVector3D& v : p.numVerts) {
                    QVERIFY2(qFuzzyCompare(v.y() + 1.0, c.expectTheta + 1.0),
                             "安全线顶点 θ（dim1）应固定为安全值");
                    QVERIFY2(qFuzzyCompare(v.z() + 1.0, c.expectPhi + 1.0),
                             "安全线顶点 φ（dim2）应固定为安全值");
                }
            } else if (p.type == QChartPrimitive::Type::Point) {
                // 每刻度 7 点：中心点（每组第 1 个）在安全线上
                if ((i - 1) % 7 == 0) {
                    ++tickCenters;
                    QVERIFY2(qFuzzyCompare(p.numA.y() + 1.0, c.expectTheta + 1.0)
                                 && qFuzzyCompare(p.numA.z() + 1.0, c.expectPhi + 1.0),
                             "刻度中心点应落在安全线上");
                    centerWorld.append(proj.toCartesian(p.numA.x(), p.numA.y(), p.numA.z()));
                }
            }
        }
        QCOMPARE(pathCount, 1);            // 面线模式只画一条轴线（无盒边/网格）
        QVERIFY2(tickCenters >= 2, "安全线应有刻度点");

        // ② 世界映射非退化：相邻刻度中心的世界点互不重合（极点/奇点会坍缩）
        for (int i = 1; i < centerWorld.size(); ++i)
            QVERIFY2((centerWorld[i] - centerWorld[i - 1]).length() > 1e-3,
                     "安全线世界点不得退化坍缩（θ/φ 需避开极点）");

        // ③ 真实出图（非恒等投影 + 世界盒相机 fit）
        const QImage img = f.render(QChartLayer3D::GridMode::FaceLine);
        QVERIFY2(inkAll(img) > 20, "球坐标面线模式应出图");
        QVERIFY2(!f.scene.labels.isEmpty(), "面线模式应带刻度标签");
    }
}

// ===== 批次2 B：Box 仅三维直角坐标——非直角投影警告并回退 FaceLine =====
void TestAxes3DSmoke::boxModeFallbackOnNonCartesian()
{
    QSphericalProjection3D proj;
    const QVector3D mn(1, 0, -30), mx(3, 90, 30);

    Axis3DFixture fFace(&proj, mn, mx);
    fFace.collect(QChartLayer3D::GridMode::FaceLine);
    const int facePrims = fFace.scene.primitives.size();
    const int faceLabels = fFace.scene.labels.size();
    QVERIFY2(facePrims > 0 && faceLabels > 0, "面线基准应有图元与标签");

    Axis3DFixture fBox(&proj, mn, mx);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("GridMode::Box"));
    fBox.collect(QChartLayer3D::GridMode::Box);   // 球坐标 + Box → 警告 + 回退
    QCOMPARE(fBox.scene.primitives.size(), facePrims);
    QCOMPARE(fBox.scene.labels.size(), faceLabels);

    // 对照：三维直角投影下 Box 不回退（图元多于面线，且不产生警告）
    QCartesianProjection3D cart;
    Axis3DFixture fCart(&cart, QVector3D(-3, -3, -3), QVector3D(3, 3, 3));
    fCart.collect(QChartLayer3D::GridMode::Box);
    QVERIFY2(fCart.scene.primitives.size() > facePrims,
             "三维直角坐标下 Box 应正常绘制（图元多于面线模式）");

    // 模式自身不被改写（回退只作用于本次收集的局部生效模式）
    QCOMPARE(static_cast<int>(fBox.layer.gridMode()),
             static_cast<int>(QChartLayer3D::GridMode::Box));
}
