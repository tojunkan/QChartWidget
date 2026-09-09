// test_axes3d_smoke.cpp —— 批次 B1：3D 轴渲染冒烟（CPU offscreen）
// 组装：QChartLayer3D（QChartAxes3D 编排器 + 3 轴 + 自带 QChartCamera3D 值成员）
//   → setDataBounds + collectPrimitives（drawAtPosition 生成 Numeric 图元入 m_scene3D）
//   → 3D 投影 + Camera3D → QPainterChartRenderer::render(scene3D, QImage) 像素断言。
#include "test_axes3d_smoke.h"

#include <QtTest>
#include <QImage>
#include <QColor>
#include <memory>

#include "QChartLayer3D.h"
#include "QPainterChartRenderer.h"
#include "QValueAxis.h"
#include "QCartesianProjection3D.h"

namespace {
bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}

/// 3D 轴冒烟夹具：Layer3D + 三轴 + 数据盒 + 相机 + 场景上下文
struct Axis3DFixture {
    QChartLayer3D layer;
    QValueAxis ax, ay, az;
    QChartCamera3D* cam = nullptr;
    QCartesianProjection3D proj;
    QChartScene scene;          // collect 后从 layer.scene3D() 拷贝（camera 指针指向 layer 成员，layer 存活期有效）

    Axis3DFixture()
        : ax(nullptr, Qt::AlignBottom), ay(nullptr, Qt::AlignLeft), az(nullptr, Qt::AlignBottom)
    {
        ax.setTickCount(5); ay.setTickCount(5); az.setTickCount(5);
        ax.setColor(Qt::black); ay.setColor(Qt::black); az.setColor(Qt::black);
        // 网格色加深（默认 220 浅灰被 isInk>40 阈值排除 → 网格/Box-Lattice 差异无法断言）
        layer.setGridColor(QColor(120, 120, 120));
        layer.setAxisX(&ax);
        layer.setAxisY(&ay);
        layer.setAxisZ(&az);
        layer.setProjection3D(&proj);
        layer.setDataBounds(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));
        cam = layer.camera3D();
        cam->setViewCube(QCube(QVector3D(-4, -4, -4), QVector3D(4, 4, 4)));
        cam->setYaw(45.0);
        cam->setPitch(30.0);
        cam->setRoll(0.0);
        layer.setScene3DProjection(&proj);
        layer.setScene3DPlotArea(QRectF(0, 0, 400, 400));
        layer.setScene3DBackground(Qt::white);
    }

    QImage render(QChartLayer3D::GridMode mode)
    {
        layer.setGridMode(mode);
        layer.collectPrimitives();
        // B1 冒烟线框语义：GL 侧 depthTest 关闭=全边可见（以 prim.depth>1 标记 decor 批次，
        // 与 CPU painter 全绘制一致）；CPU 侧 depth 字段由相机每帧重算，不受影响。
        scene = layer.scene3D();
        for (QChartPrimitive& p : scene.primitives)
            p.depth = 2.0f;
        QImage img(400, 400, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        QPainterChartRenderer r;
        r.render(scene, &img);
        return img;
    }
};
} // namespace

void TestAxes3DSmoke::cpuBoxAndLatticeRender()
{
    // 两模式分别渲染并互相对比（Lattice 网格线远多于 Box → ink 显著更多），
    // 使模式差异与网格可见性成为真实判别（非仅各自过阈值）。
    Axis3DFixture fBox;
    const QImage imgBox = fBox.render(QChartLayer3D::GridMode::Box);
    Axis3DFixture fLat;
    const QImage imgLat = fLat.render(QChartLayer3D::GridMode::Lattice);

    auto measure = [](const QImage& img) {
        int total = 0, tiles = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (isInk(img.pixelColor(x, y))) ++total;
        for (int ty = 0; ty < 3; ++ty)
            for (int tx = 0; tx < 3; ++tx) {
                bool hit = false;
                for (int y = ty * 133; y < (ty + 1) * 133 && !hit; ++y)
                    for (int x = tx * 133; x < (tx + 1) * 133 && !hit; ++x)
                        if (isInk(img.pixelColor(x, y))) hit = true;
                if (hit) ++tiles;
            }
        return qMakePair(total, tiles);
    };
    const auto mBox = measure(imgBox);
    const auto mLat = measure(imgLat);
    qInfo().noquote() << QString("3D CPU ink: Box total=%1 tiles=%2/9  Lattice total=%3 tiles=%4/9")
        .arg(mBox.first).arg(mBox.second).arg(mLat.first).arg(mLat.second);

    for (int i = 0; i < 2; ++i) {
        Axis3DFixture f;
        f.layer.collectPrimitives();
        const QChartScene& sc = f.layer.scene3D();
        QVERIFY2(sc.primitives.size() > 40,
                 "collectPrimitives 应生成盒边/脊/刻度/网格图元");
        QVERIFY2(sc.labels.size() > 0, "脊刻度标签应已生成");
    }

    QVERIFY2(mBox.first > 200, qPrintable(QString("3D Box 应产生大量墨迹（total=%1）").arg(mBox.first)));
    QVERIFY2(mBox.second >= 4, qPrintable(QString("3D Box 应铺开多个画面区域（tiles=%1/9）").arg(mBox.second)));
    QVERIFY2(mLat.first > 200, qPrintable(QString("3D Lattice 应产生大量墨迹（total=%1）").arg(mLat.first)));
    QVERIFY2(mLat.second >= 4, qPrintable(QString("3D Lattice 应铺开多个画面区域（tiles=%1/9）").arg(mLat.second)));
    // 模式判别：Lattice（3 族全网格）墨迹应显著多于 Box（仅底面 2 族）
    QVERIFY2(mLat.first > mBox.first + 100,
             qPrintable(QString("Lattice 墨迹应显著多于 Box（Box=%1 Lattice=%2）")
                        .arg(mBox.first).arg(mLat.first)));
}
