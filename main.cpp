#include <QApplication>
#include <QTimer>
#include <QVariantAnimation>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMessageLogContext>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QRandomGenerator>
#include <cmath>
#include "QPieWidget.h"
#include "QBarWidget.h"
#include "QHistogramWidget.h"
#include "QScatterWidget.h"
#include "QLineWidget.h"
#include "QXYSeries.h"
#include "QChartAxis.h"

// 消息处理器：将所有 qDebug/qWarning 等写入 debug.log
void myMessageOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QFile file("debug.log");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << " ";
        switch (type) {
        case QtDebugMsg: out << "[DEBUG] "; break;
        case QtWarningMsg: out << "[WARN] "; break;
        case QtCriticalMsg: out << "[CRIT] "; break;
        case QtFatalMsg: out << "[FATAL] "; break;
        default: break;
        }
        out << msg << "\n";
        file.close();
    }
}

int main(int argc, char* argv[])
{
    qInstallMessageHandler(myMessageOutput);
    QApplication app(argc, argv);

    QTabWidget tabs;
    tabs.setWindowTitle("QPainter Charts Demo");
    tabs.resize(700, 500);

    // ---- Tab 1: Pie Chart ----
    QPieWidget* pie = new QPieWidget;
    pie->appendSlice("电子产品", 35.0);
    pie->appendSlice("服装", 25.0);
    pie->appendSlice("食品", 20.0);
    pie->appendSlice("图书", 12.0);
    pie->appendSlice("其他", 8.0);
    pie->setHoleSize(0.35);
    pie->setStartAngle(90);
    pie->setPieSize(0.8);
    pie->setAllLabelsVisible(true);
    pie->setAllLabelsPosition(1);  // Outside
    pie->setAllBorderWidth(2);
    pie->setAllBorderColor(Qt::white);
    tabs.addTab(pie, "PieChart");

    // ---- Tab 2: Bar Chart (Vertical Groups) ----
    QBarWidget* bar = new QBarWidget;
    bar->setCategories({"Q1", "Q2", "Q3", "Q4"});
    auto* income = new QBarSet("收入", bar);
    income->setValues({120, 150, 135, 170});
    bar->addBarSet(income);
    auto* expense = new QBarSet("支出", bar);
    expense->setValues({80, 95, 100, 110});
    bar->addBarSet(expense);
    bar->setBarsType(QBarWidget::Groups);
    bar->setBarWidth(0.6);
    bar->setBarLabelsVisible(true);
    bar->setBarLabelsPosition(QBarWidget::OutsideEnd);
    bar->setBarRadius(3);
    bar->setBarBorderWidth(1);
    bar->setBarBorderColor(Qt::white);
    bar->valueAxis()->setMin(0);
    bar->valueAxis()->setMax(200);
    bar->valueAxis()->setTickCount(5);
    bar->setValuesMultiplier(0); // 从 0 开始

    // 生长动画
    QVariantAnimation* barAnim = new QVariantAnimation(bar);
    barAnim->setDuration(800);
    barAnim->setStartValue(0.0);
    barAnim->setEndValue(1.0);
    barAnim->setEasingCurve(QEasingCurve::OutBack);
    QObject::connect(barAnim, &QVariantAnimation::valueChanged,
        bar, [bar](const QVariant& v) { bar->setValuesMultiplier(v.toReal()); });
    barAnim->start();
    tabs.addTab(bar, "BarChart");

    // ---- Tab 3: Histogram (动态 bin 动画) ----
    QHistogramWidget* hist = new QHistogramWidget;
    QVector<qreal> samples;
    samples.reserve(500);
    for (int i = 0; i < 500; ++i) {
        qreal u1 = QRandomGenerator::global()->generateDouble();
        qreal u2 = QRandomGenerator::global()->generateDouble();
        qreal z = std::sqrt(-2 * std::log(qMax(u1, 0.0001))) * std::cos(2 * M_PI * u2);
        samples.append(50 + z * 15);  // mean=50, sd=15
    }
    hist->setRawData(samples);
    hist->setDensityCurveVisible(true);
    hist->setNormalCurveVisible(true);
    hist->setValuesMultiplier(0);
    tabs.addTab(hist, "Histogram");

    // 动态调整 bin 数：从 5 变到 30 再回来，保持面积不变，自动扩轴
    QVariantAnimation* binAnim = new QVariantAnimation(hist);
    binAnim->setDuration(5000);
    binAnim->setStartValue(5.0);
    binAnim->setEndValue(30.0);
    binAnim->setEasingCurve(QEasingCurve::InOutQuad);
    binAnim->setLoopCount(-1); // 无限循环
    QObject::connect(binAnim, &QVariantAnimation::valueChanged,
        hist, [hist](const QVariant& v) {
            int bins = qRound(v.toReal());
            hist->setBinCount(bins);
            hist->computeBins();
            // 自动检查是否需要扩轴
            qreal maxVal = 0;
            for (auto* s : hist->barSets())
                for (int i = 0; i < s->count(); ++i)
                    maxVal = qMax(maxVal, s->valueAt(i));
            if (maxVal > hist->valueAxis()->max() * 0.95) {
                hist->valueAxis()->setMax(maxVal * 1.15);
                qDebug() << "[main] auto-expand Y axis to" << hist->valueAxis()->max();
            }
            if (maxVal < hist->valueAxis()->max() * 0.5 && hist->valueAxis()->max() > 10) {
                hist->valueAxis()->setMax(maxVal * 1.5);
                qDebug() << "[main] auto-shrink Y axis to" << hist->valueAxis()->max();
            }
        });
    binAnim->start();

    // 生长动画
    QTimer::singleShot(200, [hist]() {
        QVariantAnimation* anim = new QVariantAnimation(hist);
        anim->setDuration(600);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutQuad);
        QObject::connect(anim, &QVariantAnimation::valueChanged,
            hist, [hist](const QVariant& v) { hist->setValuesMultiplier(v.toReal()); });
        anim->start(QVariantAnimation::DeleteWhenStopped);
    });

    // ---- Tab 4: Scatter ----
    QScatterWidget* scatter = new QScatterWidget;
    scatter->axisX()->setMin(0); scatter->axisX()->setMax(100);
    scatter->axisY()->setMin(0); scatter->axisY()->setMax(100);
    auto* sc1 = QXYSeries::randomScatter("A 组", 60, 30, 10, 30, 10);
    sc1->setColor(QColor("#2196F3"));
    scatter->addSeries(sc1);
    auto* sc2 = QXYSeries::randomScatter("B 组", 40, 60, 8, 60, 8);
    sc2->setColor(QColor("#F44336"));
    scatter->addSeries(sc2);
    auto* sc3 = QXYSeries::randomScatter("C 组", 30, 70, 6, 70, 12);
    sc3->setColor(QColor("#4CAF50"));
    scatter->addSeries(sc3);
    scatter->setMarkerShape(QScatterWidget::Circle);
    scatter->setMarkerSize(9);
    scatter->setValuesMultiplier(0);
    tabs.addTab(scatter, "Scatter");

    QTimer::singleShot(500, [scatter]() {
        QVariantAnimation* anim = new QVariantAnimation(scatter);
        anim->setDuration(700);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutBack);
        QObject::connect(anim, &QVariantAnimation::valueChanged,
            scatter, [scatter](const QVariant& v) { scatter->setValuesMultiplier(v.toReal()); });
        anim->start(QVariantAnimation::DeleteWhenStopped);
    });

    // ---- Tab 5: Line ----
    QLineWidget* line = new QLineWidget;
    line->axisX()->setMin(0); line->axisX()->setMax(10);
    line->axisY()->setMin(-2); line->axisY()->setMax(2);
    line->addSeries(QXYSeries::sinusoidal("sin(x)", 80, 1.5, 1.5, 0));
    line->addSeries(QXYSeries::sinusoidal("cos(x)", 80, 1.0, 2.0, M_PI / 4));
    line->setSmooth(true);
    line->setPointsVisible(true);
    line->setPointMarkerSize(5);
    line->setValuesMultiplier(0);
    tabs.addTab(line, "Line");

    QTimer::singleShot(800, [line]() {
        QVariantAnimation* anim = new QVariantAnimation(line);
        anim->setDuration(800);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutQuad);
        QObject::connect(anim, &QVariantAnimation::valueChanged,
            line, [line](const QVariant& v) { line->setValuesMultiplier(v.toReal()); });
        anim->start(QVariantAnimation::DeleteWhenStopped);
    });

    // ---- Tab 6: Log Axis (Scatter) ----
    QScatterWidget* logScatter = new QScatterWidget;
    auto* logAx = new QLogAxis(logScatter);
    logAx->setMin(1);
    logAx->setMax(10000);
    logAx->setBase(10);
    logScatter->setAxisX(logAx);
    logScatter->axisY()->setMin(0);
    logScatter->axisY()->setMax(100);
    // 指数分布点
    auto* logSer = new QXYSeries("指数增长", logScatter);
    for (int i = 0; i < 40; ++i) {
        qreal x = std::pow(10, qreal(i) / 10);  // 1 ~ 10000
        qreal y = 10 + std::log10(x) * 20 + (QRandomGenerator::global()->generateDouble() - 0.5) * 10;
        logSer->append(x, y);
    }
    logSer->setColor(QColor("#FF9800"));
    logScatter->addSeries(logSer);
    logScatter->setMarkerShape(QScatterWidget::Triangle);
    logScatter->setMarkerSize(10);
    tabs.addTab(logScatter, "LogAxis");

    // ---- Tab 7: DateTime Axis (Line) ----
    QLineWidget* dtLine = new QLineWidget;
    auto* dtAx = new QDateTimeAxis(dtLine);
    QDateTime now = QDateTime::currentDateTime();
    dtAx->setRange(now.addDays(-30), now.addDays(1));
    dtAx->setFormat("MM-dd");
    dtLine->setAxisX(dtAx);
    dtLine->axisY()->setMin(-5);
    dtLine->axisY()->setMax(35);
    // 30天温度模拟
    auto* temp = new QXYSeries("温度(°C)", dtLine);
    for (int i = 0; i < 30; ++i) {
        QDateTime d = now.addDays(-29 + i);
        qreal baseTemp = 15 + 10 * std::sin(i * 2 * M_PI / 30);
        qreal noise = (QRandomGenerator::global()->generateDouble() - 0.5) * 6;
        temp->append(QDateTimeAxis::toEpoch(d), baseTemp + noise);
    }
    temp->setColor(QColor("#E91E63"));
    dtLine->addSeries(temp);
    dtLine->setSmooth(true);
    dtLine->setPointsVisible(true);
    dtLine->setPointMarkerSize(5);
    dtLine->setLineWidth(2.5);
    tabs.addTab(dtLine, "DateTime");

    tabs.show();
    return app.exec();
}
