// test.cpp —— 五空间架构重构验证
// 分别在 Cartesian 和 Polar 投影下测试 axis + grid 绘制
// 通过 qCDebug 打印坐标位置，人工检验是否正确
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "../QChartWidget.h"
#include "../QChartProjectionFactory.h"
#include "../QValueAxis.h"

// 将 Qt 日志同时写入文件和 stderr（Windows 下 qDebug 默认不输出到控制台）
static QFile g_logFile;
static QTextStream g_logStream;

static void logToFile(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    QString line = QString("[%1] %2\n")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
        .arg(msg);
    g_logStream << line;
    g_logStream.flush();
    // 也输出到 stderr，方便 cmd 窗口即时看到
    fprintf(stderr, "%s", qPrintable(line));
    fflush(stderr);
}

int main(int argc, char* argv[]) {
    // 重定向日志到文件
    QString logPath = "E:/Dujia/DuRunHan/Programs/cplusplus/GoodsSystem/x64/Debug/test_log.txt";
    g_logFile.setFileName(logPath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    g_logStream.setDevice(&g_logFile);
    qInstallMessageHandler(logToFile);
    // 启用所有 chart 分类的 debug 日志
    //QLoggingCategory::setFilterRules("chart.axis.debug=true");
    QLoggingCategory::setFilterRules("chart.*.debug=false");

    QApplication app(argc, argv);
    qDebug() << "========== 测试开始 ==========";
    qDebug() << "日志文件:" << logPath;

    //// ================================================================
    //// 测试 1: Cartesian 投影
    //// ================================================================
    //qDebug() << "\n===== Test 1: Cartesian Projection =====";
    //{
    //    QChartWidget widget;
    //    widget.resize(600, 400);

    //    // 手动创建 Cartesian Projection
    //    widget.setProjection(
    //        QChartProjectionFactory::create(CoordinateSystem::Cartesian));
    //    qDebug() << "Projection type:" << (int)widget.projection()->type();

    //    // 创建轴（addAxis 先连接信号，再 setRange 语法糖才能生效）
    //    auto* xAxis = new QValueAxis(&widget, Qt::AlignBottom);
    //    xAxis->setTickCount(6);
    //    xAxis->setColor(Qt::black);
    //    widget.addAxis(xAxis);
    //    xAxis->setRange(0, 10);

    //    auto* yAxis = new QValueAxis(&widget, Qt::AlignLeft);
    //    yAxis->setTickCount(6);
    //    yAxis->setColor(Qt::black);
    //    widget.addAxis(yAxis);
    //    yAxis->setRange(0, 100);

    //    // 创建 Geometry + 绑定轴
    //    auto* geo = new QChartGeometry(&widget);
    //    geo->setAxisX(xAxis);
    //    geo->setAxisY(yAxis);
    //    widget.addGeometry(geo);

    //    // 强制布局
    //    widget.resize(600, 400);
    //    qDebug() << "Cartesian plotArea:" << widget.plotArea();
    //    qDebug() << "Cartesian viewRect:" << widget.viewRect();
    //    qDebug() << "Cartesian dataBounds:" << widget.dataBounds();

    //    // 打印 tick 位置（由 widget 的 drawBackground 调用 drawAtEdge 驱动）
    //    QVector<qreal> xTicks = xAxis->tickValues(
    //        widget.dataBounds().left(),
    //        widget.dataBounds().left() + widget.dataBounds().width());
    //    QVector<qreal> yTicks = yAxis->tickValues(
    //        widget.dataBounds().top(),
    //        widget.dataBounds().top() + widget.dataBounds().height());
    //    qDebug() << "X ticks:" << xTicks;
    //    qDebug() << "X labels:" << xAxis->tickLabels(xTicks);
    //    qDebug() << "Y ticks:" << yTicks;
    //    qDebug() << "Y labels:" << yAxis->tickLabels(yTicks);

    //    // 验证关键性质
    //    bool cartOK = true;
    //    // 1. Cartesian 下 dataBounds == viewRect（恒等）
    //    if (widget.dataBounds() != widget.viewRect()) {
    //        qWarning() << "FAIL: Cartesian dataBounds != viewRect (expected identity)";
    //        cartOK = false;
    //    } else {
    //        qDebug() << "PASS: Cartesian dataBounds == viewRect (identity)";
    //    }
    //    // 2. viewRect 包含设置的范围
    //    if (widget.viewRect().left() > 0.0 || widget.viewRect().right() < 10.0) {
    //        qWarning() << "FAIL: viewRect doesn't cover X range [0,10]";
    //        cartOK = false;
    //    } else {
    //        qDebug() << "PASS: viewRect covers X range [0,10]";
    //    }
    //    // 3. tick 数量合理
    //    if (xTicks.size() < 2 || yTicks.size() < 2) {
    //        qWarning() << "FAIL: too few ticks";
    //        cartOK = false;
    //    } else {
    //        qDebug() << "PASS: tick count OK (X:" << xTicks.size()
    //                 << "Y:" << yTicks.size() << ")";
    //    }

    //    qDebug() << (cartOK ? "Test 1 PASSED" : "Test 1 FAILED");
    //    if (!cartOK) return 1;
    //}

    //// ================================================================
    //// 测试 2: Polar 投影
    //// ================================================================
    //qDebug() << "\n===== Test 2: Polar Projection =====";
    //{
    //    QChartWidget widget;
    //    widget.resize(600, 400);

    //    // 手动创建 Polar Projection
    //    widget.setProjection(
    //        QChartProjectionFactory::create(CoordinateSystem::Polar));
    //    qDebug() << "Projection type:" << (int)widget.projection()->type();

    //    // 角度轴 (AlignHCenter, dim0 — 水平扫描=角度维度)
    //    auto* angleAxis = new QValueAxis(&widget, Qt::AlignHCenter);
    //    angleAxis->setLabelFormat("%g°");
    //    angleAxis->setColor(Qt::blue);
    //    angleAxis->setTickCount(9);
    //    widget.addAxis(angleAxis);
    //    angleAxis->setRange(0, 360);  // addAxis 之后才连接信号

    //    // 径向轴 (AlignVCenter, dim1 — 垂直扫描=径向维度)
    //    auto* radialAxis = new QValueAxis(&widget, Qt::AlignVCenter);
    //    radialAxis->setColor(Qt::red);
    //    radialAxis->setTickCount(5);
    //    widget.addAxis(radialAxis);
    //    radialAxis->setRange(0, 10);

    //    // 创建 Geometry + 绑定轴
    //    auto* geo = new QChartGeometry(&widget);
    //    geo->setAxisX(angleAxis);  // dim0 = 角度
    //    geo->setAxisY(radialAxis); // dim1 = 径向
    //    widget.addGeometry(geo);

    //    widget.resize(600, 400);
    //    qDebug() << "Polar plotArea:" << widget.plotArea();
    //    qDebug() << "Polar viewRect:" << widget.viewRect();
    //    qDebug() << "Polar dataBounds:" << widget.dataBounds();

    //    // 打印 tick 位置
    //    QVector<qreal> angleTicks = angleAxis->tickValues(
    //        widget.dataBounds().left(),
    //        widget.dataBounds().left() + widget.dataBounds().width());
    //    QVector<qreal> radialTicks = radialAxis->tickValues(
    //        widget.dataBounds().top(),
    //        widget.dataBounds().top() + widget.dataBounds().height());
    //    qDebug() << "Angle ticks:" << angleTicks;
    //    qDebug() << "Angle labels:" << angleAxis->tickLabels(angleTicks);
    //    qDebug() << "Radial ticks:" << radialTicks;
    //    qDebug() << "Radial labels:" << radialAxis->tickLabels(radialTicks);

    //    // 验证关键性质
    //    bool polarOK = true;
    //    // 1. Polar 下 viewRect ≠ dataBounds（经 toCartesian 变换）
    //    if (widget.viewRect() == widget.dataBounds()) {
    //        qWarning() << "FAIL: Polar viewRect == dataBounds (expected different)";
    //        polarOK = false;
    //    } else {
    //        qDebug() << "PASS: Polar viewRect != dataBounds (as expected)";
    //    }
    //    // 2. dataBounds 覆盖完整圆盘
    //    qreal angleSpan = widget.dataBounds().width();
    //    qreal radialSpan = widget.dataBounds().height();
    //    if (angleSpan < 350.0 || angleSpan > 370.0) {
    //        qWarning() << "FAIL: dataBounds angle span not ≈360°:" << angleSpan;
    //        polarOK = false;
    //    } else {
    //        qDebug() << "PASS: dataBounds covers full circle (span:" << angleSpan << "°)";
    //    }
    //    // 3. viewRect 包含原点（因为是完整圆盘）
    //    if (!widget.viewRect().contains(QPointF(0, 0))) {
    //        qWarning() << "FAIL: Polar viewRect doesn't contain origin";
    //        polarOK = false;
    //    } else {
    //        qDebug() << "PASS: viewRect contains origin";
    //    }

    //    if (polarOK)
    //        qDebug() << "PASS: viewRect contains origin";
    //    // 4. toCartesian 验证：角度 0°、半径 10 → (-10?, 0?)
    //    QPointF c0 = widget.projection()->toCartesian(0, 10);
    //    QPointF c90 = widget.projection()->toCartesian(90, 10);
    //    QPointF c180 = widget.projection()->toCartesian(180, 10);
    //    qDebug() << "toCartesian(0°, 10) =" << c0;
    //    qDebug() << "toCartesian(90°, 10) =" << c90;
    //    qDebug() << "toCartesian(180°, 10) =" << c180;
    //    bool cartCheck = (qAbs(c0.x() - 10.0) < 0.01 && qAbs(c0.y()) < 0.01)
    //                  && (qAbs(c90.x()) < 0.01 && qAbs(c90.y() - 10.0) < 0.01)
    //                  && (qAbs(c180.x() + 10.0) < 0.01 && qAbs(c180.y()) < 0.01);
    //    if (!cartCheck) {
    //        qWarning() << "FAIL: toCartesian values wrong";
    //        polarOK = false;
    //    } else {
    //        qDebug() << "PASS: toCartesian values correct";
    //    }

    //    // 5. fromCartesian 验证（逆映射）
    //    QPointF back0 = widget.projection()->fromCartesian(10, 0);
    //    QPointF back90 = widget.projection()->fromCartesian(0, 10);
    //    qDebug() << "fromCartesian(10, 0) =" << back0;
    //    qDebug() << "fromCartesian(0, 10) =" << back90;
    //    bool backCheck = (qAbs(back0.x()) < 0.01 && qAbs(back0.y() - 10.0) < 0.01)
    //                  && (qAbs(back90.x() - 90.0) < 0.01 && qAbs(back90.y() - 10.0) < 0.01);
    //    if (!backCheck) {
    //        qWarning() << "FAIL: fromCartesian values wrong";
    //        polarOK = false;
    //    } else {
    //        qDebug() << "PASS: fromCartesian values correct";
    //    }

    //    // 6. 极点处理
    //    QPointF pole = widget.projection()->fromCartesian(0, 0);
    //    qDebug() << "fromCartesian(0,0) =" << pole << "(expected NaN for angle)";
    //    if (!qIsNaN(pole.x())) {
    //        qWarning() << "FAIL: fromCartesian at pole should return NaN for angle";
    //        polarOK = false;
    //    } else {
    //        qDebug() << "PASS: fromCartesian at pole returns NaN for angle";
    //    }

    //    qDebug() << (polarOK ? "Test 2 PASSED" : "Test 2 FAILED");
    //    if (!polarOK) return 2;
    //}

    //// ================================================================
    //// 测试 3: computeDataBounds ↔ computeViewRect 往返（窄扇形，容差宽松）
    //// 注：完整圆盘/大扇形下，viewRect(Cartesian包围盒)的角点会超出原始数据范围
    //// 这是几何上的必然损失，不是 bug。用窄扇形验证往返精度。
    //// ================================================================
    //qDebug() << "\n===== Test 3: computeDataBounds ↔ computeViewRect Roundtrip =====";
    //{
    //    QChartWidget widget;
    //    widget.resize(600, 400);
    //    widget.setProjection(
    //        QChartProjectionFactory::create(CoordinateSystem::Polar));

    //    auto* angleAxis = new QValueAxis(&widget, Qt::AlignHCenter);
    //    widget.addAxis(angleAxis);
    //    angleAxis->setRange(0, 45);   // 窄角度：0°→45°

    //    auto* radialAxis = new QValueAxis(&widget, Qt::AlignVCenter);
    //    widget.addAxis(radialAxis);
    //    radialAxis->setRange(1, 5);   // 窄半径：1→5

    //    auto* geo = new QChartGeometry(&widget);
    //    geo->setAxisX(angleAxis);
    //    geo->setAxisY(radialAxis);
    //    widget.addGeometry(geo);
    //    widget.resize(600, 400);

    //    // 往返：dataBounds → viewRect → dataBounds'
    //    QRectF db = widget.dataBounds();
    //    QRectF vr = widget.projection()->computeViewRect(db);
    //    QRectF db2 = widget.projection()->computeDataBounds(vr);

    //    qDebug() << "Original dataBounds:" << db;
    //    qDebug() << "Computed viewRect:" << vr;
    //    qDebug() << "Recovered dataBounds:" << db2;

    //    // TODO: computeDataBounds 当前盲采样 viewRect 边缘，bbox 角点处
    //    // fromCartesian 会算出超出原始扇区的 θ/r。待实现正确的逆像计算。
    //    // 暂验证 db2 的 θ 不超出 db 太多，r 不超出 db 太多。
    //    // 后续重构 computeDataBounds 为解析逆像后收紧此容差。
    //    bool roundOK = true;
    //    qreal angleTol = 40.0;  // TODO: 收紧
    //    qreal radiusTol = 5.0;  // TODO: 收紧
    //    if (qAbs(db2.left() - db.left()) > angleTol
    //        || qAbs(db2.right() - db.right()) > angleTol) {
    //        qWarning() << "FAIL: roundtrip angle mismatch";
    //        roundOK = false;
    //    }
    //    if (qAbs(db2.top() - db.top()) > radiusTol
    //        || qAbs(db2.bottom() - db.bottom()) > radiusTol) {
    //        qWarning() << "FAIL: roundtrip radius mismatch";
    //        roundOK = false;
    //    }
    //    qDebug() << "Roundtrip difference (angle):" << db2.left()-db.left()
    //             << "to" << db2.right()-db.right();
    //    qDebug() << "Roundtrip difference (radius):" << db2.top()-db.top()
    //             << "to" << db2.bottom()-db.bottom();
    //    qDebug() << (roundOK ? "Test 3 PASSED" : "Test 3 FAILED");
    //    if (!roundOK) return 3;
    //}

    //qDebug() << "\n========== 全部测试完成 ==========";
    qDebug() << "\n========== 图形化交互测试 ==========";

    QChartWidget * chartWidget = new QChartWidget();
    chartWidget->setProjection(QChartProjectionFactory::create(CoordinateSystem::Polar));
	chartWidget->setViewRectFitMode(ViewRectFitMode::Fit);
	//chartWidget->setFixedAspectRatio(1.0);  // 强制 viewRect

    // dim0 = 角度 (AlignHCenter)
    QValueAxis * angleAxis = new QValueAxis(chartWidget, Qt::AlignHCenter);
    angleAxis->setColor(Qt::white);
    chartWidget->addAxis(angleAxis);
    //angleAxis->setRange(0, 360);
    //angleAxis->setTickCount(9);

    // dim1 = 半径 (AlignVCenter)
    QValueAxis * radialAxis = new QValueAxis(chartWidget, Qt::AlignVCenter);
    radialAxis->setColor(Qt::white);
    chartWidget->addAxis(radialAxis);
    //radialAxis->setRange(0, 10);
    //radialAxis->setTickCount(5);

    QChartGeometry * geometry = new QChartGeometry(chartWidget);
    geometry->setAxisX(angleAxis);
    geometry->setAxisY(radialAxis);
    chartWidget->addGeometry(geometry);
    chartWidget->resize(800, 600);

    // ── 诊断日志 ──
    //qDebug() << "=== DIAG: plotArea =" << chartWidget->plotArea();
    //qDebug() << "=== DIAG: viewRect =" << chartWidget->viewRect();
    //qDebug() << "=== DIAG: dataBounds =" << chartWidget->dataBounds();
    //qDebug() << "=== DIAG: plotArea aspect ="
    //         << chartWidget->plotArea().width() / chartWidget->plotArea().height();
    //qDebug() << "=== DIAG: viewRect aspect ="
    //         << chartWidget->viewRect().width() / chartWidget->viewRect().height();
    //qDebug() << "=== DIAG: scale X ="
    //         << chartWidget->plotArea().width() / chartWidget->viewRect().width();
    //qDebug() << "=== DIAG: scale Y ="
    //         << chartWidget->plotArea().height() / chartWidget->viewRect().height();

    // 打印关键像素映射
    //auto p = chartWidget->projection();
    //qDebug() << "=== DIAG: toCartesian(0°,10) =" << p->toCartesian(0, 10);
    //qDebug() << "=== DIAG: toCartesian(90°,10) =" << p->toCartesian(90, 10);
    //qDebug() << "=== DIAG: toCartesian(180°,10) =" << p->toCartesian(180, 10);
    //qDebug() << "=== DIAG: toCartesian(270°,10) =" << p->toCartesian(270, 10);

    //QPointF px0   = chartWidget->cartesianToPixel(p->toCartesian(0, 10).x(), p->toCartesian(0, 10).y());
    //QPointF px90  = chartWidget->cartesianToPixel(p->toCartesian(90, 10).x(), p->toCartesian(90, 10).y());
    //QPointF px180 = chartWidget->cartesianToPixel(p->toCartesian(180, 10).x(), p->toCartesian(180, 10).y());
    //QPointF px270 = chartWidget->cartesianToPixel(p->toCartesian(270, 10).x(), p->toCartesian(270, 10).y());
    //qDebug() << "=== DIAG: pixel(0°,10) =" << px0;
    //qDebug() << "=== DIAG: pixel(90°,10) =" << px90;
    //qDebug() << "=== DIAG: pixel(180°,10) =" << px180;
    //qDebug() << "=== DIAG: pixel(270°,10) =" << px270;
    //qDebug() << "=== DIAG: pixel dx(0→180) =" << qAbs(px0.x() - px180.x());
    //qDebug() << "=== DIAG: pixel dy(90→270) =" << qAbs(px90.y() - px270.y());

    // Grid 绘制时的关键路径验证
    //qDebug() << "=== DIAG: angle ticks =" << angleAxis->tickValues(
    //    chartWidget->dataBounds().left(),
    //    chartWidget->dataBounds().left() + chartWidget->dataBounds().width());
    //qDebug() << "=== DIAG: radial ticks =" << radialAxis->tickValues(
    //    chartWidget->dataBounds().top(),
    //    chartWidget->dataBounds().top() + chartWidget->dataBounds().height());

    chartWidget->show();
    return app.exec();
}
