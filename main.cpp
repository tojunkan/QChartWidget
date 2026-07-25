#include <QApplication>
#include <QTimer>
#include <QVariantAnimation>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMessageLogContext>
#include <QTabWidget>
#include <QRandomGenerator>
#include <cmath>
#include "QPieWidget.h"
#include "QBarWidget.h"
#include "QHistogramWidget.h"
#include "QLineWidget.h"
#include "QScatterWidget.h"
#include "QChartAxis.h"

void myMessageOutput(QtMsgType type, const QMessageLogContext&, const QString& msg) {
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
    }
}

int main(int argc, char* argv[]) {
    qInstallMessageHandler(myMessageOutput);
    QApplication app(argc, argv);
    QTabWidget tabs;
    tabs.setWindowTitle("QPainter Charts");
    tabs.resize(750, 520);

    // Tab 1: Pie
    auto* pie = new QPieWidget;
    pie->appendSlice("电子产品", 35); pie->appendSlice("服装", 25);
    pie->appendSlice("食品", 20); pie->appendSlice("图书", 12);
    pie->appendSlice("其他", 8);
    pie->setHoleSize(0.35);
    tabs.addTab(pie, "Pie");

    // Tab 2: Bar
    auto* bar = new QBarWidget;
    bar->setCategories({"Q1","Q2","Q3","Q4"});
    bar->addBarSet("收入", {120,150,135,170});
    bar->addBarSet("支出", {80,95,100,110});
    bar->setBarLabelsVisible(true);
    tabs.addTab(bar, "Bar");

    // Tab 3: Histogram
    auto* hist = new QHistogramWidget;
    QVector<qreal> samples; samples.reserve(300);
    auto* rng = QRandomGenerator::global();
    for (int i=0;i<300;++i){qreal u1=rng->generateDouble(),u2=rng->generateDouble();
        samples.append(50+15*std::sqrt(-2*std::log(qMax(u1,0.0001)))*std::cos(2*M_PI*u2));}
    hist->setRawData(samples);
    hist->setDensityCurveVisible(true); hist->setNormalCurveVisible(true);
    tabs.addTab(hist, "Histogram");

    // Tab 4: Scatter
    auto* scatter = new QScatterWidget;
    scatter->axisX()->setRange(0,100); scatter->axisY()->setRange(0,100);
    auto* sc1 = scatter->addSeries("A组"); sc1->setColor(QColor("#2196F3"));
    auto* sc2 = scatter->addSeries("B组"); sc2->setColor(QColor("#F44336"));
    auto* sc3 = scatter->addSeries("C组"); sc3->setColor(QColor("#4CAF50"));
    for (int i=0;i<50;++i){qreal u1=rng->generateDouble(),u2=rng->generateDouble();
        qreal z1=std::sqrt(-2*std::log(qMax(u1,0.0001)))*std::cos(2*M_PI*u2);
        qreal z2=std::sqrt(-2*std::log(qMax(u1,0.0001)))*std::sin(2*M_PI*u2);
        sc1->append(30+z1*10,30+z2*10); sc2->append(60+z1*8,60+z2*8); sc3->append(70+z1*6,70+z2*12);}
    tabs.addTab(scatter, "Scatter");

    // Tab 5: Line
    auto* line = new QLineWidget;
    line->axisX()->setRange(0,10); line->axisY()->setRange(-2,2);
    line->series()->setSmooth(true); line->series()->setPointsVisible(true);
    for (int i=0;i<80;++i){qreal x=qreal(i)/79*10;
        line->series()->append(x,1.5*std::sin(1.5*x));}
    tabs.addTab(line, "Line");

    // Tab 6: LogAxis
    {
        auto* chart = new QChartWidget;
        auto* g = new QCartesianGeometry; chart->addGeometry(g);
        auto* axX = new QLogAxis; axX->setRange(1,10000); axX->setBase(10);
        auto* axY = new QValueAxis; axY->setRange(0,100);
        chart->addAxis(axX); chart->addAxis(axY); g->setAxisX(axX); g->setAxisY(axY);
        auto* s = new QScatterSeries("指数"); s->setMarkerShape(QScatterSeries::Triangle);
        for (int i=0;i<40;++i){qreal x=std::pow(10,qreal(i)/10);
            s->append(x,10+std::log10(x)*20+(rng->generateDouble()-0.5)*10);}
        g->addSeries(s); tabs.addTab(chart, "LogAxis");
    }

    // Tab 7: DateTime
    {
        auto* chart = new QChartWidget;
        auto* g = new QCartesianGeometry; chart->addGeometry(g);
        auto* axX = new QDateTimeAxis; QDateTime now=QDateTime::currentDateTime();
        axX->setRange(now.addDays(-30),now.addDays(1)); axX->setFormat("MM-dd");
        auto* axY = new QValueAxis; axY->setRange(-5,35);
        chart->addAxis(axX); chart->addAxis(axY); g->setAxisX(axX); g->setAxisY(axY);
        auto* s = new QLineSeries("温度"); s->setSmooth(true); s->setPointsVisible(true);
        s->setColor(QColor("#E91E63"));
        for (int i=0;i<30;++i){QDateTime d=now.addDays(-29+i);
            qreal t=15+10*std::sin(i*2*M_PI/30)+(rng->generateDouble()-0.5)*6;
            s->append(QDateTimeAxis::toEpoch(d),t);}
        g->addSeries(s); tabs.addTab(chart, "DateTime");
    }

    // Tab 8: Horizontal histogram (freq on X, bin on Y)
    auto* hhist = new QHistogramWidget;
    hhist->setHorizontal(true);
    QVector<qreal> hsamps; hsamps.reserve(200);
    for (int i=0;i<200;++i){qreal u1=rng->generateDouble(),u2=rng->generateDouble();
        hsamps.append(50+15*std::sqrt(-2*std::log(qMax(u1,0.0001)))*std::cos(2*M_PI*u2));}
    hhist->setRawData(hsamps);
    tabs.addTab(hhist, "H-Hist");

    // Tab 9: Overlay
    {
        auto* chart = new QChartWidget;
        auto* g = new QCartesianGeometry; chart->addGeometry(g);
        auto* axX = new QValueAxis; axX->setRange(0,10); auto* axY = new QValueAxis; axY->setRange(-2,2);
        chart->addAxis(axX); chart->addAxis(axY); g->setAxisX(axX); g->setAxisY(axY);
        auto* ln = new QLineSeries("信号"); ln->setSmooth(true); ln->setLineWidth(1.5);
        for (int i=0;i<50;++i){qreal x=qreal(i)/49*10; ln->append(x,1.5*std::sin(1.5*x));}
        g->addSeries(ln);
        auto* sc = new QScatterSeries("采样"); sc->setMarkerShape(QScatterSeries::Diamond);
        sc->setMarkerSize(10); sc->setColor(QColor("#F44336"));
        for (int i=0;i<15;++i){qreal x=qreal(i)/14*10; sc->append(x,1.5*std::sin(1.5*x)+(rng->generateDouble()-0.5)*0.5);}
        g->addSeries(sc); tabs.addTab(chart, "Overlay");
    }

    // Tab 10: Pan&Zoom
    {
        auto* chart = new QChartWidget;
        auto* g = new QCartesianGeometry; chart->addGeometry(g);
        auto* axX = new QValueAxis; axX->setRange(0,10); auto* axY = new QValueAxis; axY->setRange(-2,2);
        chart->addAxis(axX); chart->addAxis(axY); g->setAxisX(axX); g->setAxisY(axY);
        auto* s = new QLineSeries("大数据"); s->setLineWidth(1); s->setColor(QColor("#4CAF50"));
        for (int i=0;i<1000;++i){qreal x=qreal(i)/100;
            s->append(x,std::sin(x)*std::exp(-x/10)+(rng->generateDouble()-0.5)*0.1);}
        g->addSeries(s); chart->setPanEnabled(true); chart->setZoomEnabled(true);
        tabs.addTab(chart, "Pan&Zoom");
    }

    // Tab 11: Polar pie (轴可见)
    {
        auto* chart = new QChartWidget;
        auto* g = new QPolarGeometry; chart->addGeometry(g);
        auto* s = new QPieSeries("分布");
        s->appendSlice("A",40); s->appendSlice("B",30);
        s->appendSlice("C",20); s->appendSlice("D",10);
        s->setHoleSize(0.2); g->addSeries(s);
        tabs.addTab(chart, "Polar");
    }

    tabs.show();
    return app.exec();
}
