// test_widget3d_smoke.cpp —— 批次 B2：QChartWidget3D 轻量容器 CPU 冒烟（offscreen）
#include "test_widget3d_smoke.h"

#include <QtTest>
#include <QtMath>
#include <cmath>
#include <QImage>
#include <QColor>
#include <QRegularExpression>

#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>

#include "QChartWidget3D.h"
#include "QChartCamera3D.h"
#include "QCartesianProjection3D.h"
#include "QSphericalProjection3D.h"

namespace {
bool isInk(const QColor& c)
{
    return qAbs(c.red() - 255) > 40 || qAbs(c.green() - 255) > 40 || qAbs(c.blue() - 255) > 40;
}
// ===== 4f：合成鼠标/滚轮事件注入（直接投递到 widget）=====
void sendMouse3D(QWidget* w, QEvent::Type type, const QPointF& pos, Qt::MouseButton button)
{
    const Qt::MouseButtons buttons = (type == QEvent::MouseMove) ? Qt::NoButton : button;
    QMouseEvent ev(type, pos, w->mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &ev);
}
void sendWheel3D(QWidget* w, const QPointF& pos, int deltaY)
{
    QWheelEvent ev(pos, w->mapToGlobal(pos), QPoint(0, 0), QPoint(0, deltaY),
                   Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(w, &ev);
}

int inkTotal(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (isInk(img.pixelColor(x, y))) ++n;
    return n;
}
int inkTiles(const QImage& img)
{
    const int tw = 3, th = 3;
    int tiles = 0;
    for (int ty = 0; ty < th; ++ty)
        for (int tx = 0; tx < tw; ++tx) {
            bool hit = false;
            for (int y = ty * (img.height() / th); y < (ty + 1) * (img.height() / th) && !hit; ++y)
                for (int x = tx * (img.width() / tw); x < (tx + 1) * (img.width() / tw) && !hit; ++x)
                    if (isInk(img.pixelColor(x, y))) hit = true;
            if (hit) ++tiles;
        }
    return tiles;
}
} // namespace

void TestWidget3DSmoke::cpuWidget3DRenders()
{
    QChartWidget3D w;
    QCartesianProjection3D proj;          // 生命周期覆盖渲染（非持有约定）
    w.setProjection3D(&proj);
    w.setDomainBox(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));   // → layer3D.setDataBounds + fitWorld
    // 批次2 B：FaceLine 为默认模式——容器冒烟取证（盒/网格/刻度墨迹）显式使用几何最丰富的 Box
    w.setGridMode3D(QChartLayer3D::GridMode::Box);
    QCOMPARE(static_cast<int>(w.gridMode3D()),
             static_cast<int>(QChartLayer3D::GridMode::Box));
    QVERIFY2(w.hasDomainBox(), "setDomainBox 应记录域盒");
    QVERIFY2(w.layer3D() != nullptr, "构造应托管默认 layer3D");
    QVERIFY2(w.camera3D() == w.layer3D()->camera3D(), "相机归 layer3D（widget 无独立相机成员）");
    QVERIFY2(w.projection3D() == &proj, "投影应转发 layer3D");
    QVERIFY2(w.viewCube().isValid(), "fitWorld 后相机应有有效 viewCube");

    // worldToPixel 便捷（经层相机；plotArea 就绪前返回 NaN 亦可接受，此处仅调用不崩）
    (void)w.worldToPixel(QVector3D(0, 0, 0));

    w.resize(420, 360);
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "offscreen 下窗口应暴露");
    QTest::qWait(40);

    const QImage img = w.grab().toImage();
    // ★ HiDPI：grab 为设备像素图（逻辑 × dpr），断言按 dpr 缩放（DPR=1 时 s=1 与历史一致）
    const qreal s = qreal(img.width()) / w.width();
    QVERIFY2(qAbs(s - w.devicePixelRatioF()) < 0.01,
             "grab 尺寸应等于 widget（设备像素 = 逻辑 × dpr）");
    const int total = inkTotal(img);
    const int tiles = inkTiles(img);
    QVERIFY2(total > 150, qPrintable(QString("3D 盒/网格/刻度应出墨 total=%1").arg(total)));
    QVERIFY2(tiles >= 4, qPrintable(QString("3D 内容应铺开多区 tiles=%1/9").arg(tiles)));
}

// ===== 批次2 B：QChartWidget3D 三网格模式入口（转发图层）+ 默认值 + 标签契约 =====
void TestWidget3DSmoke::cpuWidget3DGridModes()
{
    QChartWidget3D w;
    QCartesianProjection3D proj;
    w.setProjection3D(&proj);
    w.setDomainBox(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));

    // 默认模式 = FaceLine（批次2 B）
    QCOMPARE(static_cast<int>(w.gridMode3D()),
             static_cast<int>(QChartLayer3D::GridMode::FaceLine));

    QChartLayer3D* layer = w.layer3D();
    QVERIFY(layer != nullptr);

    const auto collect = [layer](QChartLayer3D::GridMode m) {
        layer->setGridMode(m);
        layer->collectPrimitives();
        return qMakePair(layer->scene3D().primitives.size(), layer->scene3D().labels.size());
    };

    // FaceLine：只画一条安全轴线 + 刻度标签；无盒边/网格
    w.setGridMode3D(QChartLayer3D::GridMode::FaceLine);
    QCOMPARE(static_cast<int>(w.gridMode3D()),
             static_cast<int>(QChartLayer3D::GridMode::FaceLine));
    const auto face = collect(QChartLayer3D::GridMode::FaceLine);
    QVERIFY2(face.first > 0, "面线模式应有轴线图元");
    QVERIFY2(face.second > 0, "面线模式：轴按刻度逐个标注");

    // Box：盒 12 边 + 底面网格 + 三主轴标签（图元显著多于面线）
    w.setGridMode3D(QChartLayer3D::GridMode::Box);
    const auto box = collect(QChartLayer3D::GridMode::Box);
    QVERIFY2(box.first > face.first, "盒模式图元应多于面线模式");
    QVERIFY2(box.second > 0, "盒模式：三条主轴应按刻度生成标签");

    // Lattice：只画线，无任何标签
    w.setGridMode3D(QChartLayer3D::GridMode::Lattice);
    const auto lat = collect(QChartLayer3D::GridMode::Lattice);
    QCOMPARE(lat.second, 0);
    QVERIFY2(lat.first > 0, "晶格模式应有线图元");

    // Box + 非直角投影：图层 qWarning 并回退 FaceLine（模式下不变）
    QSphericalProjection3D sph;
    w.setProjection3D(&sph);
    w.setGridMode3D(QChartLayer3D::GridMode::Box);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("GridMode::Box"));
    layer->collectPrimitives();
    QCOMPARE(layer->scene3D().primitives.size(), face.first);
    QCOMPARE(static_cast<int>(w.gridMode3D()),
             static_cast<int>(QChartLayer3D::GridMode::Box));
}

// ===== 4b：三维数据盒不再独立持有——由三根轴范围组装（数值断言，与 4a 基线一致）=====
void TestWidget3DSmoke::dataBoundsFromAxesContract()
{
    QChartWidget3D w;
    QCartesianProjection3D proj;
    w.setProjection3D(&proj);
    QChartLayer3D* l3 = w.layer3D();
    QVERIFY(l3 != nullptr);

    // ① 默认（构造后）：三轴 0..10 → 组装盒 = (0,0,0)-(10,10,10)（= 迁移前默认盒）
    {
        QVERIFY2(!w.hasDomainBox(), "默认不应标记为已设域盒");
        const QCube box = l3->axes3D()->dataBounds();
        QCOMPARE(box.min.x(), 0.0);  QCOMPARE(box.min.y(), 0.0);  QCOMPARE(box.min.z(), 0.0);
        QCOMPARE(box.max.x(), 10.0); QCOMPARE(box.max.y(), 10.0); QCOMPARE(box.max.z(), 10.0);
        QVERIFY2(l3->hasValidDataBounds(), "默认盒有效");
    }

    // ② setDomainBox：数值落在三根轴上（本类不再持有 min/max）
    w.setDomainBox(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));
    QVERIFY(w.hasDomainBox());
    QCOMPARE(l3->axisX()->min(), -3.0); QCOMPARE(l3->axisX()->max(), 3.0);
    QCOMPARE(l3->axisY()->min(), -3.0); QCOMPARE(l3->axisY()->max(), 3.0);
    QCOMPARE(l3->axisZ()->min(), -3.0); QCOMPARE(l3->axisZ()->max(), 3.0);
    {
        const QCube box = l3->axes3D()->dataBounds();
        QCOMPARE(box.min.x(), -3.0); QCOMPARE(box.min.z(), -3.0);
        QCOMPARE(box.max.x(), 3.0);  QCOMPARE(box.max.z(), 3.0);
    }

    // ③ 直接改轴范围 → 组装盒随之变化（证明"轴是唯一持有者"）
    l3->axisX()->setRange(2.0, 8.0);
    {
        const QCube box = l3->axes3D()->dataBounds();
        QCOMPARE(box.min.x(), 2.0); QCOMPARE(box.max.x(), 8.0);
        QCOMPARE(box.min.y(), -3.0); QCOMPARE(box.max.y(), 3.0);   // 其余维不受影响
    }
    qInfo().noquote() << QString("4b 3D: 域盒(-3..3) → 轴=(%1..%2, %3..%4, %5..%6)；改 X 轴 2..8 后组装盒 X=(%7..%8)")
                             .arg(l3->axisX()->min()).arg(l3->axisX()->max())
                             .arg(l3->axisY()->min()).arg(l3->axisY()->max())
                             .arg(l3->axisZ()->min()).arg(l3->axisZ()->max())
                             .arg(l3->axes3D()->dataBounds().min.x()).arg(l3->axes3D()->dataBounds().max.x());

    // ④ clearDomainBox：回退默认盒 0..10（轴与组装盒一致）
    w.clearDomainBox();
    QVERIFY2(!w.hasDomainBox(), "clearDomainBox 后不再标记已设域盒");
    {
        const QCube box = l3->axes3D()->dataBounds();
        QCOMPARE(box.min.x(), 0.0); QCOMPARE(box.max.x(), 10.0);
        QCOMPARE(box.min.z(), 0.0); QCOMPARE(box.max.z(), 10.0);
    }
}

// ===== 4c：QCube 工具最小集（相等/包含/合并）+ 盒子 API QCube 化（零行为变化）=====
void TestWidget3DSmoke::cubeToolsAndBoxApiContract()
{
    // ① QCube 工具（4c 覆盖：相等比较 / 包含判定 / 两盒合并——均为已有 API，无新增类型）
    const QCube a(QVector3D(0, 0, 0), QVector3D(10, 10, 10));
    const QCube b(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));
    QVERIFY2(a == QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)), "相等比较");
    QVERIFY2(a != b, "不等比较");
    QVERIFY2(a.contains(QVector3D(0, 0, 0)) && a.contains(QVector3D(10, 10, 10)),
             "包含判定（含边界）");
    QVERIFY2(!a.contains(QVector3D(10.1, 0, 0)), "包含判定（外部点）");
    const QCube inner(QVector3D(1, 1, 1), QVector3D(2, 2, 2));
    QVERIFY2(a.contains(inner) && !inner.contains(a), "盒包含盒（真包含 / 反向为假）");
    QVERIFY2(!a.contains(b), "跨出边界的盒不被包含");
    const QCube u = a.united(b);
    QCOMPARE(u.min.x(), -3.0);
    QCOMPARE(u.max.x(), 10.0);
    QCOMPARE(u.size().y(), 13.0);
    QVERIFY2(u.contains(a) && u.contains(b), "并集包含两者");

    // ② QChartLayer3D::setDataBounds(QCube) 主签名（两点重载等价转发）
    {
        QChartLayer3D layer;
        QValueAxis ax, ay, az;
        layer.setAxisX(&ax); layer.setAxisY(&ay); layer.setAxisZ(&az);
        const QCube box(QVector3D(-3, -3, -3), QVector3D(3, 3, 3));
        layer.setDataBounds(box);
        QCOMPARE(layer.axisX()->min(), -3.0);  QCOMPARE(layer.axisX()->max(), 3.0);
        QCOMPARE(layer.axisY()->min(), -3.0);  QCOMPARE(layer.axisZ()->max(), 3.0);
        QVERIFY2(layer.hasValidDataBounds(), "QCube 主签名后盒有效");
        QVERIFY2(layer.axes3D()->dataBounds() == box, "组装盒 == 入参 QCube");

        layer.setDataBounds(QVector3D(-2, -2, -2), QVector3D(2, 2, 2));   // 便捷重载
        QVERIFY2(layer.axes3D()->dataBounds() == QCube(QVector3D(-2, -2, -2), QVector3D(2, 2, 2)),
                 "两点重载等价于 QCube 主签名");
    }

    // ③ QChartWidget3D 域盒 API QCube 语义（数值仍落三轴；domainBox() 按需组装）
    {
        QChartWidget3D w;
        QCartesianProjection3D proj;
        w.setProjection3D(&proj);
        const QCube box(QVector3D(-4, -5, -6), QVector3D(4, 5, 6));
        w.setDomainBox(box);
        QVERIFY2(w.hasDomainBox(), "QCube setDomainBox 应记录");
        QVERIFY2(w.domainBox() == box, "domainBox() 组装值 == 入参");
        QCOMPARE(w.axisX3D()->min(), -4.0);
        QCOMPARE(w.axisY3D()->max(), 5.0);
        QCOMPARE(w.axisZ3D()->min(), -6.0);

        w.setDomainBox(QVector3D(-1, -1, -1), QVector3D(1, 1, 1));        // 便捷重载
        QVERIFY2(w.domainBox() == QCube(QVector3D(-1, -1, -1), QVector3D(1, 1, 1)),
                 "两点重载等价于 QCube 主签名");

        w.clearDomainBox();
        QVERIFY2(w.domainBox() == QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)), "clearDomainBox 回退默认盒");
        QVERIFY2(!w.hasDomainBox(), "clearDomainBox 后不标记");
        qInfo().noquote() << QString("4c: QCube 工具 + 盒子 API 契约通过；默认盒 size=(%1,%2,%3)")
                                 .arg(w.domainBox().size().x()).arg(w.domainBox().size().y())
                                 .arg(w.domainBox().size().z());
    }

    // ④ 4c 相机拟合判别力（t42 M1 回归断言：halfDiag 必须取「外扩 6% 后」的 viewCube 尺寸）
    //    盒 (-3,-3,-3)-(3,3,3) → pad=0.36 → viewCube ±3.36 → halfDiag=|6.72√3|·0.5 →
    //    d=halfDiag/sin(fov/2)=15.2075834…；若误用未外扩尺寸（fitBox.size()*0.5）则 d≈13.58 → 本段变红。
    {
        QChartWidget3D w;
        QCartesianProjection3D proj;
        w.setProjection3D(&proj);
        w.setDomainBox(QCube(QVector3D(-3, -3, -3), QVector3D(3, 3, 3)));
        QChartCamera3D* cam = w.camera3D();
        QVERIFY2(cam != nullptr, "camera3D 非空");

        const QCube vc = cam->viewCube();                       // 视角盒 = 域盒外扩 6%
        QVERIFY2(qAbs(vc.min.x() - (-3.36)) < 1e-5 && qAbs(vc.max.z() - 3.36) < 1e-5,
                 qPrintable(QString("viewCube 应外扩 6%%，实为 %1~%2").arg(vc.min.x()).arg(vc.max.z())));

        const qreal halfDiag = vc.size().length() * 0.5;        // 判别力核心：halfDiag 取自外扩后的 viewCube
        const qreal expected = halfDiag / qMax(qreal(0.05), qSin(qDegreesToRadians(cam->fov()) * 0.5));
        QVERIFY2(qAbs(cam->distance() - expected) < 1e-9,
                 qPrintable(QString("distance 应==halfDiag(外扩后)/sin(fov/2)=%1，实为 %2")
                                .arg(expected, 0, 'g', 12).arg(cam->distance(), 0, 'g', 12)));
        QVERIFY2(qAbs(cam->distance() - 15.2075834) < 1e-4,
                 qPrintable(QString("盒 ±3 期望 distance=15.2075834，实为 %1").arg(cam->distance(), 0, 'g', 12)));
        QVERIFY2(cam->distance() > 14.0,      // 未外扩旧写法 ≈13.58 必落此线下 → 回归判别
                 qPrintable(QString("distance 必须按外扩后半对角线（旧错误写法≈13.58），实为 %1")
                                .arg(cam->distance(), 0, 'g', 12)));

        w.clearDomainBox();                                     // 默认盒 0..10 → viewCube -0.6~10.6
        QVERIFY2(qAbs(cam->viewCube().min.x() - (-0.6)) < 1e-5 && qAbs(cam->viewCube().max.x() - 10.6) < 1e-5,
                 "clearDomainBox 后 viewCube 应为 -0.6~10.6");
        QVERIFY2(qAbs(cam->distance() - 25.3459753) < 1e-4,
                 qPrintable(QString("默认盒期望 distance=25.3459753，实为 %1").arg(cam->distance(), 0, 'g', 12)));
        qInfo().noquote() << QString("4c: 相机拟合判别力断言通过 d(±3 盒)=%1  d(clearDomainBox)=%2  viewCube(clear 后)=%3~%4")
                                 .arg(15.2075834, 0, 'g', 9).arg(w.camera3D()->distance(), 0, 'g', 9)
                                 .arg(w.camera3D()->viewCube().min.x(), 0, 'g', 9)
                                 .arg(w.camera3D()->viewCube().max.x(), 0, 'g', 9);
    }
}

// ===== 4d：相机自动适配契约（autoFit 三触发路径 / 掩码四约束 + 优先级 + 退化 / override 保护 /
//        resetNearFar 与 fit 收尾分工 / setPitch 钳制）=====
// 说明：相机专属测试类文件未在本阶段 CMake 清单内（改 Test/unit/CMakeLists.txt 越出 t45 inScope），
//       故 4d 相机契约落在已构建的 TestWidget3DSmoke 中；后续阶段可整批迁往独立相机测试类。
void TestWidget3DSmoke::cameraFitContract()
{
    // ===== ① autoFit 开关：三条触发路径（域盒变化 / 投影设置 / 显式 fitWorld）=====
    {
        QChartWidget3D w;
        QCartesianProjection3D proj;
        w.setProjection3D(&proj);                       // 路径②（autoFit 默认 on）
        QChartCamera3D* cam = w.camera3D();
        QVERIFY2(cam != nullptr, "camera3D 非空");
        QVERIFY2(cam->autoFit(), "默认 autoFit=true");

        w.setDomainBox(QCube(QVector3D(-3, -3, -3), QVector3D(3, 3, 3)));   // 建立基线状态
        cam->setYaw(12.0);                              // 用户手调姿态
        cam->setPitch(20.0);

        // ---- 关闭：三条路径均不触碰相机（含 viewCube 锚点）----
        cam->setAutoFit(false);
        const QCube vc0 = cam->viewCube();
        const qreal d0 = cam->distance(), fov0 = cam->fov();
        const qreal n0 = cam->nearPlane(), f0 = cam->farPlane();
        const qreal yaw0 = cam->yaw(), pitch0 = cam->pitch();

        w.setDomainBox(QCube(QVector3D(-5, -5, -5), QVector3D(5, 5, 5)));   // 路径①
        QCOMPARE(w.axisX3D()->min(), -5.0);             // 域盒数值仍落三轴
        QCOMPARE(w.axisX3D()->max(), 5.0);
        QCOMPARE(w.axisY3D()->min(), -5.0);
        w.setProjection3D(&proj);                       // 路径②
        w.fitWorld();                                   // 路径③
        QVERIFY2(cam->viewCube() == vc0, "autoFit=false：viewCube 不得被触碰");
        QCOMPARE(cam->distance(), d0);
        QCOMPARE(cam->fov(), fov0);
        QCOMPARE(cam->nearPlane(), n0);
        QCOMPARE(cam->farPlane(), f0);
        QCOMPARE(cam->yaw(), yaw0);                     // 手调姿态保持
        QCOMPARE(cam->pitch(), pitch0);
        qInfo().noquote() << QString("4d: autoFit=false 三路径零触碰 OK（d=%1 yaw=%2 pitch=%3）")
                                 .arg(cam->distance()).arg(cam->yaw()).arg(cam->pitch());

        // ---- 打开：下一次触发即按 FixedFov 解算 + 锚点跟随 + 姿态复位 ----
        cam->setAutoFit(true);
        w.fitWorld();
        const QCube vc1 = cam->viewCube();
        // ±5 域盒 → pad = 10*0.06 = 0.6 → 锚点 ±5.6
        QVERIFY2(qAbs(vc1.min.x() - (-5.6)) < 1e-4 && qAbs(vc1.max.x() - 5.6) < 1e-4,
                 qPrintable(QString("开启后锚点应外扩 6%%（±5.6），实为 %1~%2").arg(vc1.min.x()).arg(vc1.max.x())));
        const qreal r1 = vc1.size().length() * 0.5;
        const qreal expD = r1 / qMax(qreal(0.05), qSin(qDegreesToRadians(cam->fov()) * 0.5));
        QVERIFY2(qAbs(cam->distance() - expD) < 1e-9, "开启后 distance == r/max(0.05,sin(fov/2))");
        QCOMPARE(cam->yaw(), 45.0);                     // 姿态复位
        QCOMPARE(cam->pitch(), 30.0);
        QVERIFY2(qAbs(cam->nearPlane() - qMax<qreal>(0.01, cam->distance() - r1)) < 1e-9, "near 内切");
        QVERIFY2(qAbs(cam->farPlane() - (cam->distance() + r1)) < 1e-9, "far 内切");
        QVERIFY2(!cam->isNearFarOverridden(), "fit 接管后 override 清除");
    }

    // ===== ③ 掩码四约束 + 优先级 + 退化输入 =====
    {
        QChartCamera3D cam;
        cam.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));
        cam.setFov(45.0);
        const QRectF pa(0, 0, 400, 300);                        // aspect = 4/3 ≥ 1 → 垂直基准
        const qreal r = cam.viewCubeSize().length() * 0.5;      // == radius()
        const qreal halfY = qDegreesToRadians(cam.fov()) * 0.5;

        // FixedFov → 解 distance（fov 保持）
        cam.setDistance(100.0);
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedFov));
        QCOMPARE(cam.fov(), 45.0);
        QVERIFY2(qAbs(cam.distance() - r / qSin(halfY)) < 1e-9, "FixedFov：d = r/sin(fov/2)");

        // FixedDist → 解 fov（distance 保持）
        cam.setDistance(30.0);
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedDist));
        QCOMPARE(cam.distance(), 30.0);
        QVERIFY2(qAbs(cam.fov() - qRadiansToDegrees(2.0 * qAsin(r / 30.0))) < 1e-9,
                 "FixedDist：fov = 2·asin(r/d)");

        // FixedNear → distance = near + r，再解 fov
        cam.setNearPlane(5.0);
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedNear));
        QVERIFY2(qAbs(cam.nearPlane() - 5.0) < 1e-9, "FixedNear：near 保持");
        QVERIFY2(qAbs(cam.distance() - (5.0 + r)) < 1e-9, "FixedNear：d = near + r");
        QVERIFY2(qAbs(cam.fov() - qRadiansToDegrees(2.0 * qAsin(r / cam.distance()))) < 1e-9,
                 "FixedNear 后按新 d 解 fov");

        // FixedFar → distance = far − r，再解 fov
        cam.setFarPlane(100.0);
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedFar));
        QVERIFY2(qAbs(cam.distance() - (100.0 - r)) < 1e-9, "FixedFar：d = far − r");
        QVERIFY2(qAbs(cam.fov() - qRadiansToDegrees(2.0 * qAsin(r / cam.distance()))) < 1e-9,
                 "FixedFar 后按新 d 解 fov");

        // 多约束优先级：Fov > Dist > Near > Far
        cam.setFov(45.0);
        cam.setDistance(77.0);
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedFov | FitConstraint::FixedDist
                                        | FitConstraint::FixedNear | FitConstraint::FixedFar));
        QCOMPARE(cam.fov(), 45.0);                              // fov 未被反解
        QVERIFY2(qAbs(cam.distance() - r / qSin(halfY)) < 1e-9, "多约束取 FixedFov（解 distance）");

        cam.setFov(45.0);
        cam.setDistance(30.0);
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedDist | FitConstraint::FixedNear
                                        | FitConstraint::FixedFar));
        QCOMPARE(cam.distance(), 30.0);                          // distance 未被改写
        QVERIFY2(qAbs(cam.fov() - qRadiansToDegrees(2.0 * qAsin(r / 30.0))) < 1e-9,
                 "多约束取 FixedDist（解 fov）");

        // 退化①：零尺寸盒（r = 0）→ 不改动任何参数并返回 false
        {
            QChartCamera3D c2;
            c2.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(0, 0, 0)));
            const qreal d = c2.distance(), fv = c2.fov(), nr = c2.nearPlane(), fr = c2.farPlane();
            QVERIFY2(!c2.fitCameraConfig(pa, FitConstraint::FixedFov), "r=0 → false");
            QCOMPARE(c2.distance(), d);
            QCOMPARE(c2.fov(), fv);
            QCOMPARE(c2.nearPlane(), nr);
            QCOMPARE(c2.farPlane(), fr);
        }

        // 退化②：零/负尺寸 plotArea → aspect 按 1.0 继续求解（不 no-op）
        {
            QChartCamera3D c3;
            c3.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));
            c3.setDistance(100.0);                          // 先扰动：使 fit 必有变更（返回 true）
            QVERIFY2(c3.fitCameraConfig(QRectF(), FitConstraint::FixedFov), "退化 plotArea 仍求解");
            QVERIFY2(qAbs(c3.distance() - r / qSin(halfY)) < 1e-9,
                     "退化 plotArea：按垂直基准 d = r/sin(fov/2)");
            QChartCamera3D c3b;
            c3b.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));
            c3b.setDistance(100.0);
            QVERIFY2(c3b.fitCameraConfig(QRectF(0, 0, -5, 0), FitConstraint::FixedFov), "负尺寸同样继续");
            QVERIFY2(qAbs(c3b.distance() - r / qSin(halfY)) < 1e-9, "负尺寸 plotArea：同垂直基准");
        }

        // 退化③：r/distance > 1（距离过近）→ fov 取 179°（尽量包容），distance 保持
        {
            QChartCamera3D c4;
            c4.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));
            c4.setDistance(2.0);
            QVERIFY(c4.fitCameraConfig(pa, FitConstraint::FixedDist));
            QCOMPARE(c4.fov(), 179.0);
            QCOMPARE(c4.distance(), 2.0);
        }

        // 退化④：极端 aspect（超宽扁）→ fov 有限且钳在 (1,179]
        {
            QChartCamera3D c5;
            c5.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));
            c5.setDistance(30.0);
            QVERIFY(c5.fitCameraConfig(QRectF(0, 0, 10000, 1), FitConstraint::FixedDist));
            QVERIFY2(std::isfinite(c5.fov()) && c5.fov() > 1.0 && c5.fov() <= 179.0,
                     qPrintable(QString("极端 aspect：fov 有限且钳制，实为 %1").arg(c5.fov())));
        }

        // 4d 表现变更锁定（t46 裁定①，aspect < 1 竖高视口）：
        //   fitWorld 并入的 FixedFov 在竖高视口下按**横向**临界半视角解算
        //   （d = r / max(0.05, sin(atan(tan(fov/2)·aspect)))，相机后退以完整包容数据）；
        //   4c 内联式忽略 aspect，故此处数值不同于 4c（有意改进）；aspect ≥ 1 与退化 plotArea 仍逐位一致。
        {
            QChartCamera3D c7;
            c7.setViewCube(QCube(QVector3D(-3.36f, -3.36f, -3.36f), QVector3D(3.36f, 3.36f, 3.36f)));
            c7.setFov(45.0);
            c7.setDistance(100.0);
            const QRectF tall(0, 0, 332, 638);                       // aspect = 332/638 ≈ 0.5204 < 1（竖高）
            QVERIFY2(c7.fitCameraConfig(tall, FitConstraint::FixedFov), "竖高视口 FixedFov 解算");
            const qreal r7 = c7.viewCubeSize().length() * 0.5;
            const qreal halfY7 = qDegreesToRadians(c7.fov()) * 0.5;
            const qreal halfX7 = qAtan(qTan(halfY7) * (tall.width() / tall.height()));
            const qreal exp7 = r7 / qMax(qreal(0.05), qSin(halfX7));
            QVERIFY2(qAbs(c7.distance() - exp7) < 1e-9,
                     "aspect<1：d == r/max(0.05, sin(atan(tan(fov/2)·aspect)))（横向临界半视角）");
            QVERIFY2(qAbs(c7.distance() - 27.619738287) < 1e-5,
                     qPrintable(QString("aspect<1（332×638）期望 ≈27.619738287，实为 %1")
                                    .arg(c7.distance(), 0, 'g', 12)));
            QVERIFY2(c7.distance() > 20.0,
                     qPrintable(QString("不得回落到忽略 aspect 的 4c 值 15.2076（判别力），实为 %1")
                                    .arg(c7.distance(), 0, 'g', 12)));
            qInfo().noquote() << QString("4d: aspect<1（332x638）d=%1（同盒 aspect≥1/退化时 d=15.2075834）")
                                     .arg(c7.distance(), 0, 'g', 12);
        }
    }

    // ===== ④ m_nearFarOverride 保护策略 =====
    {
        QChartCamera3D cam;
        cam.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));
        const QRectF pa(0, 0, 400, 300);
        const qreal r = cam.viewCubeSize().length() * 0.5;
        const qreal halfY = qDegreesToRadians(cam.fov()) * 0.5;

        QVERIFY2(!cam.isNearFarOverridden(), "初始无 override");
        cam.setNearPlane(3.0);                                  // 用户显式设置 near/far
        cam.setFarPlane(90.0);
        QVERIFY(cam.isNearFarOverridden());
        cam.setDistance(100.0);
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedFov));
        QVERIFY2(qAbs(cam.nearPlane() - 3.0) < 1e-9 && qAbs(cam.farPlane() - 90.0) < 1e-9,
                 "override 保护：fit 不得覆盖用户 near/far");
        QVERIFY2(cam.isNearFarOverridden(), "保护生效时 override 标志保持");
        QVERIFY2(qAbs(cam.distance() - r / qSin(halfY)) < 1e-9, "只保护 near/far，镜头仍解算");

        // 显式 FixedFar ⇒ fit 接管：覆盖 near/far（按内切值）并清除 override
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedFar));
        QVERIFY2(!cam.isNearFarOverridden(), "显式 FixedFar → clear override");
        QVERIFY2(qAbs(cam.distance() - (90.0 - r)) < 1e-9, "FixedFar：d = far − r");
        QVERIFY2(qAbs(cam.farPlane() - (cam.distance() + r)) < 1e-9, "far 回内切值");
        QVERIFY2(qAbs(cam.nearPlane() - qMax<qreal>(0.01, cam.distance() - r)) < 1e-9, "near 回内切值");

        // 无 override：fit 直接接管 near/far
        QChartCamera3D c2;
        c2.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));
        c2.setDistance(100.0);
        QVERIFY(c2.fitCameraConfig(pa, FitConstraint::FixedFov));
        QVERIFY2(!c2.isNearFarOverridden(), "无 override 时 fit 接管并保持标志清除");
        QVERIFY2(qAbs(c2.nearPlane() - qMax<qreal>(0.01, c2.distance() - r)) < 1e-9, "自动 near");
        QVERIFY2(qAbs(c2.farPlane() - (c2.distance() + r)) < 1e-9, "自动 far");
    }

    // ===== ⑤ resetNearFar 与 fit 收尾分工（同一内切实现，两条路径数值一致）=====
    {
        QChartCamera3D cam;
        cam.setViewCube(QCube(QVector3D(0, 0, 0), QVector3D(10, 10, 10)));
        const QRectF pa(0, 0, 400, 300);
        const qreal r = cam.viewCubeSize().length() * 0.5;

        cam.setDistance(50.0);
        cam.setNearPlane(7.0);
        cam.setFarPlane(80.0);
        QVERIFY(cam.isNearFarOverridden());
        cam.resetNearFar();                                     // 用户复位路径
        QVERIFY2(!cam.isNearFarOverridden(), "resetNearFar 清除 override");
        QVERIFY2(qAbs(cam.nearPlane() - qMax<qreal>(0.01, 50.0 - r)) < 1e-9, "resetNearFar：near 内切");
        QVERIFY2(qAbs(cam.farPlane() - (50.0 + r)) < 1e-9, "resetNearFar：far 内切");
        QCOMPARE(cam.distance(), 50.0);                         // 复位不改 distance/fov

        // fit 收尾路径（无 override → fit 直接接管），与复位路径同一实现 ⇒ 同 distance 下数值逐位一致
        cam.setDistance(30.0);
        cam.setFov(45.0);
        QVERIFY(cam.fitCameraConfig(pa, FitConstraint::FixedDist));
        const qreal nearFit = cam.nearPlane(), farFit = cam.farPlane(), distFit = cam.distance();
        cam.resetNearFar();                                          // 同 distance/radius 的复位路径
        QVERIFY2(cam.nearPlane() == nearFit && cam.farPlane() == farFit,
                 "fit 收尾与 resetNearFar 为同一实现（同 distance/radius ⇒ 逐位相同）");
        QVERIFY2(qAbs(nearFit - qMax<qreal>(0.01, distFit - r)) < 1e-9, "fit 收尾 near 内切");
        QVERIFY2(qAbs(farFit - (distFit + r)) < 1e-9, "fit 收尾 far 内切");
        QVERIFY2(!cam.isNearFarOverridden(), "fit 收尾清除 override");
    }

    // ===== ⑥ setPitch 直接超范围调用也钳制 ±89°（与 orbit 一致）=====
    {
        QChartCamera3D cam;
        cam.setPitch(120.0);
        QCOMPARE(cam.pitch(), 89.0);
        cam.setPitch(-1000.0);
        QCOMPARE(cam.pitch(), -89.0);
        cam.setPitch(30.0);
        QCOMPARE(cam.pitch(), 30.0);
        cam.orbit(0.0, 500.0);
        QCOMPARE(cam.pitch(), 89.0);
        cam.orbit(0.0, -500.0);
        QCOMPARE(cam.pitch(), -89.0);
        qInfo().noquote() << "4d: autoFit/掩码四约束/override 保护/resetNearFar 分工/pitch 钳制 契约全部通过";
    }
}

// ===== 4e：像素侧驱动（plotArea 变化 → 仅重解算镜头）+ 竖高视口 8 角点硬证据断言 =====
void TestWidget3DSmoke::plotAreaDriveContract()
{
    QChartWidget3D w;
    QCartesianProjection3D proj;
    w.setProjection3D(&proj);
    w.setDomainBox(QCube(QVector3D(-3, -3, -3), QVector3D(3, 3, 3)));   // 锚点 ±3.36（pad = 6*0.06）
    QChartCamera3D* cam = w.camera3D();
    QVERIFY2(cam != nullptr, "camera3D 非空");
    const qreal r = cam->viewCubeSize().length() * 0.5;
    const qreal halfY = qDegreesToRadians(cam->fov()) * 0.5;
    w.resetDriveCounters();

    // ---- 宽视口（aspect ≥ 1）：距离按垂直基准 == 15.2075834（4c/4d 基线值）----
    w.resize(720, 540);
    w.relayout();
    const QRectF paWide = w.plotArea();
    QVERIFY2(paWide.width() > 0.0 && paWide.height() > 0.0, "宽视口 plotArea 有效");
    QVERIFY2(paWide.width() / paWide.height() >= 1.0, "宽视口 aspect ≥ 1");
    QVERIFY2(qAbs(cam->distance() - r / qMax(qreal(0.05), qSin(halfY))) < 1e-9, "宽视口：垂直基准");
    QVERIFY2(qAbs(cam->distance() - 15.2075834) < 1e-4,
             qPrintable(QString("宽视口距离应 == 15.2075834，实为 %1").arg(cam->distance(), 0, 'g', 12)));
    QVERIFY2(w.plotAreaFitCount() >= 1, "宽视口布局应触发像素侧驱动（解算实跑）");
    QCOMPARE(w.plotAreaFitChangeCount(), 0);   // 宽视口与既有窗口等价 → 幂等无变化

    // ---- 竖高视口（aspect < 1）：横向临界半视角 → 相机后退；8 角点必须全部落在 plotArea 内 ----
    w.resize(332, 638);
    w.relayout();
    const QRectF paTall = w.plotArea();
    QVERIFY2(paTall.height() > paTall.width(), "竖高视口 plotArea 应高大于宽");
    const qreal aspect = paTall.width() / paTall.height();
    QVERIFY2(aspect < 1.0, "aspect < 1");
    // ★ 4e 加固断言（硬证据，先于数值断言——4c 旧语义下距离偏小 → 角点出界，此断言首当其冲变红）：
    //    viewCube 8 角点经 cam.project() 投影后必须全部落在 plotArea 内
    {
        const QCube vc = cam->viewCube();
        const QRectF paEps = paTall.adjusted(-0.5, -0.5, 0.5, 0.5);
        int inside = 0;
        for (int i = 0; i < 8; ++i) {
            const QVector3D c((i & 1) ? vc.max.x() : vc.min.x(),
                              (i & 2) ? vc.max.y() : vc.min.y(),
                              (i & 4) ? vc.max.z() : vc.min.z());
            const QPointF s = cam->project(c, paTall).screen;
            if (paEps.contains(s)) ++inside;
        }
        QVERIFY2(inside == 8,
                 qPrintable(QString("竖高视口：viewCube 8 角点应全部落在 plotArea 内，实际 %1/8（4c 旧语义下距离偏小 → 角点出界）")
                                .arg(inside)));
    }

    const qreal halfX = qAtan(qTan(halfY) * aspect);
    QVERIFY2(qAbs(cam->distance() - r / qMax(qreal(0.05), qSin(halfX))) < 1e-9,
             "竖高视口：按横向临界半视角解算（4d 修正语义）");
    const qreal tallDistance = cam->distance();
    QVERIFY2(tallDistance > 15.3,
             qPrintable(QString("竖高视口距离应大于宽视口基线（相机后退），实为 %1").arg(tallDistance, 0, 'g', 12)));
    QVERIFY2(w.plotAreaFitChangeCount() >= 1, "竖高视口应真正改变镜头（相机后退）");

    // ---- 幂等：同尺寸重复 relayout → 无额外重解算 ----
    w.resetDriveCounters();
    w.relayout();
    QCOMPARE(w.plotAreaFitCount(), 0);

    // ---- 回到宽视口 → 距离恢复基线（往返一致）----
    w.resize(720, 540);
    w.relayout();
    QVERIFY2(qAbs(cam->distance() - 15.2075834) < 1e-4,
             qPrintable(QString("回宽视口距离应恢复 15.2075834，实为 %1").arg(cam->distance(), 0, 'g', 12)));

    qInfo().noquote() << QString("4e 3D: 像素侧驱动 %1 次（改变镜头 %2 次）；竖高 aspect=%3 → d=%4；"
                                 "回宽视口 d=%5（基线 15.2075834），8/8 角点在视口内")
                             .arg(w.plotAreaFitCount()).arg(w.plotAreaFitChangeCount())
                             .arg(aspect, 0, 'g', 6).arg(tallDistance, 0, 'g', 12)
                             .arg(cam->distance(), 0, 'g', 9);
}

// ===== 4f：三维鼠标交互契约（orbit 增量与钳制 / dolly + autoFit 两侧 / panViewCube / 开关 / 计数）=====
void TestWidget3DSmoke::mouseInteraction3DContract()
{
    QChartWidget3D w;
    QCartesianProjection3D proj;
    w.setProjection3D(&proj);
    w.setDomainBox(QCube(QVector3D(-3, -3, -3), QVector3D(3, 3, 3)));
    w.resize(720, 540);
    w.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&w), "offscreen 下窗口应暴露");
    w.grab();
    QChartCamera3D* cam = w.camera3D();
    QVERIFY2(cam != nullptr, "camera3D 非空");
    const QRectF pa = w.plotArea();
    QVERIFY2(pa.width() > 0.0 && pa.height() > 0.0, "plotArea 应有效");
    QVERIFY2(w.isInteractionEnabled(), "交互开关默认开");
    const QPointF p0 = pa.center();

    // ---- ① 左键拖动 = orbit：增量 0.5°/px 与俯仰钳制 ----
    const qreal yaw0 = cam->yaw(), pitch0 = cam->pitch();
    const QPointF d(40.0, 30.0);
    sendMouse3D(&w, QEvent::MouseButtonPress, p0, Qt::LeftButton);
    sendMouse3D(&w, QEvent::MouseMove, p0 + d, Qt::NoButton);
    sendMouse3D(&w, QEvent::MouseButtonRelease, p0 + d, Qt::LeftButton);
    // 期望值用字面常量（t54 F2：不得用生产 helper 自证；helper 仅用于下方接线断言）
    QVERIFY2(qAbs(cam->yaw() - (yaw0 + 20.0)) < 1e-9, "yaw 增量 = 40px × 0.5°/px = +20°");
    QVERIFY2(qAbs(cam->pitch() - (pitch0 - 15.0)) < 1e-9, "pitch 增量 = 30px × (-0.5)°/px = -15°");
    QVERIFY2(qAbs(QChartWidget3D::orbitYawDelta(1.0) - 0.5) < 1e-12
                 && qAbs(QChartWidget3D::orbitPitchDelta(1.0) + 0.5) < 1e-12,
             "接线断言：orbit 单位增量恰为 0.5 / -0.5 °/px（helper 对字面常量）");

    sendMouse3D(&w, QEvent::MouseButtonPress, p0, Qt::LeftButton);
    sendMouse3D(&w, QEvent::MouseMove, p0 + QPointF(0.0, 5000.0), Qt::NoButton);
    sendMouse3D(&w, QEvent::MouseButtonRelease, p0 + QPointF(0.0, 5000.0), Qt::LeftButton);
    QCOMPARE(cam->pitch(), -89.0);                       // 向下大幅拖动 → 钳到 -89°
    sendMouse3D(&w, QEvent::MouseButtonPress, p0, Qt::LeftButton);
    sendMouse3D(&w, QEvent::MouseMove, p0 + QPointF(0.0, -5000.0), Qt::NoButton);
    sendMouse3D(&w, QEvent::MouseButtonRelease, p0 + QPointF(0.0, -5000.0), Qt::LeftButton);
    QCOMPARE(cam->pitch(), 89.0);                        // 向上大幅拖动 → 钳到 +89°
    cam->setPitch(120.0);
    QCOMPARE(cam->pitch(), 89.0);                        // 直接超范围同样钳制

    // ---- ② 滚轮 = dolly + autoFit 联动 ----
    cam->setYaw(45.0); cam->setPitch(30.0);
    w.resetDriveCounters();
    const QCube vc0 = cam->viewCube();
    const qreal dBefore = cam->distance();
    const QPointF pz = pa.center();
    sendWheel3D(&w, pz, 120);
    const qreal fWheel = 1.0 / 1.10;                     // 字面常量（t54 F2：不用生产 helper 自证）
    QVERIFY2(fWheel < 1.0, "上滚 = 放大（视野盒收缩）");
    QVERIFY2(qAbs(cam->viewCube().size().x() - vc0.size().x() * fWheel) < 1e-4, "dolly 因子生效（盒尺寸 × 1.10⁻¹）");
    QVERIFY2(qAbs(QChartWidget3D::wheelDollyFactor(120) - (1.0 / 1.10)) < 1e-12,
             "接线断言：滚轮 120 的 dolly 因子恰为 1.10⁻¹（helper 对字面常量）");
    QCOMPARE(w.interactionFitCount(), 1);                // autoFit 开 → fit 恰 1 次
    QVERIFY2(!qFuzzyCompare(cam->distance(), dBefore), "autoFit 开：distance 随之重算");
    const qreal r1 = cam->viewCubeSize().length() * 0.5;
    QVERIFY2(qAbs(cam->distance() - r1 / qMax(qreal(0.05), qSin(qDegreesToRadians(cam->fov()) * 0.5))) < 1e-9,
             "重算值 == r/sin(fov/2)（新视野盒）");
    QVERIFY2(qAbs(cam->nearPlane() - qMax<qreal>(0.01, cam->distance() - r1)) < 1e-9
                 && qAbs(cam->farPlane() - (cam->distance() + r1)) < 1e-9, "近/远面同步重算");

    cam->setAutoFit(false);                              // autoFit 关：dolly 生效但保留手调参数
    w.resetDriveCounters();
    const QCube vc1 = cam->viewCube();
    const qreal d1 = cam->distance(), n1 = cam->nearPlane(), f1 = cam->farPlane();
    sendWheel3D(&w, pz, 120);
    QVERIFY2(qAbs(cam->viewCube().size().x() - vc1.size().x() * fWheel) < 1e-4, "autoFit 关：dolly 仍改视野盒");
    QCOMPARE(cam->distance(), d1); QCOMPARE(cam->nearPlane(), n1); QCOMPARE(cam->farPlane(), f1);
    QCOMPARE(w.interactionFitCount(), 0);                // 关 → 不触发 fit
    cam->setAutoFit(true);

    // ---- ③ 中键/右键拖动 = panViewCube（平移中心；不改盒尺寸、不 fit）----
    w.resetDriveCounters();
    const QVector3D c0 = cam->viewCubeCenter();
    const QVector3D s0 = cam->viewCubeSize();
    const QPointF dp(24.0, 18.0);
    sendMouse3D(&w, QEvent::MouseButtonPress, p0, Qt::MiddleButton);
    sendMouse3D(&w, QEvent::MouseMove, p0 + dp, Qt::NoButton);
    sendMouse3D(&w, QEvent::MouseButtonRelease, p0 + dp, Qt::MiddleButton);
    const qreal kx = s0.x() / pa.width(), ky = s0.y() / pa.height();
    QVERIFY2(qAbs(cam->viewCubeCenter().x() - (c0.x() - dp.x() * kx)) < 1e-3, "pan：中心 Δx = -Δpx·kx");
    QVERIFY2(qAbs(cam->viewCubeCenter().y() - (c0.y() + dp.y() * ky)) < 1e-3, "pan：中心 Δy = +Δpy·ky");
    QVERIFY2(cam->viewCubeSize() == s0, "pan 不改视野盒尺寸");
    QCOMPARE(w.interactionFitCount(), 0);                // pan 不触发 fit

    // ---- ④ 开关关闭：鼠标事件零效果 ----
    w.setInteractionEnabled(false);
    const qreal yawOff = cam->yaw(), pitchOff = cam->pitch();
    const QCube vcOff = cam->viewCube();
    const qreal dOff = cam->distance(), nOff = cam->nearPlane(), fOff = cam->farPlane();
    sendMouse3D(&w, QEvent::MouseButtonPress, p0, Qt::LeftButton);
    sendMouse3D(&w, QEvent::MouseMove, p0 + QPointF(120.0, 90.0), Qt::NoButton);
    sendMouse3D(&w, QEvent::MouseButtonRelease, p0 + QPointF(120.0, 90.0), Qt::LeftButton);
    sendMouse3D(&w, QEvent::MouseButtonPress, p0, Qt::RightButton);
    sendMouse3D(&w, QEvent::MouseMove, p0 + QPointF(60.0, 40.0), Qt::NoButton);
    sendMouse3D(&w, QEvent::MouseButtonRelease, p0 + QPointF(60.0, 40.0), Qt::RightButton);
    sendWheel3D(&w, pz, 120);
    QCOMPARE(cam->yaw(), yawOff); QCOMPARE(cam->pitch(), pitchOff);
    QVERIFY2(cam->viewCube() == vcOff, "开关关闭：视野盒不变");
    QCOMPARE(cam->distance(), dOff); QCOMPARE(cam->nearPlane(), nOff); QCOMPARE(cam->farPlane(), fOff);
    w.setInteractionEnabled(true);

    qInfo().noquote() << QString("4f 3D: orbit Δ=(%1°,%2°)/（%3,%4)px；滚轮 f(120)=%5（autoFit 开 fit=%6 / 关 fit=%7）；"
                                 "中键 pan Δ中心=(%8,%9)")
                             .arg(QChartWidget3D::orbitYawDelta(40.0)).arg(QChartWidget3D::orbitPitchDelta(30.0))
                             .arg(40.0).arg(30.0).arg(fWheel, 0, 'g', 6)
                             .arg(1).arg(0)
                             .arg(-dp.x() * kx, 0, 'g', 4).arg(dp.y() * ky, 0, 'g', 4);
}
