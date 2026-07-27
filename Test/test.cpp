#include <QApplication>
#include "../QChartWidget.h"
#include "../QBarCategoryAxis.h"

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);
	QChartWidget w;

	QBarCategoryAxis* valueAxisx = new QBarCategoryAxis(&w, Qt::AlignRight);
	valueAxisx->setColor(Qt::white);
	valueAxisx->append("hello");
	valueAxisx->append("world");
	//valueAxisx->setSubTickCount(4);
	w.addAxis(valueAxisx);

	//QBarCategoryAxis* valueAxisy = new QBarCategoryAxis(&w, Qt::AlignLeft);
	//valueAxisy->setColor(Qt::white);
	//valueAxisy->setSubTickCount(4);
	//valueAxisy->setBase(2);
	//w.addAxis(valueAxisy);

	qDebug() << "Program Started.";

	w.show();
	return app.exec();
}