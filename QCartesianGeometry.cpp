// QCartesianGeometry.cpp —— 最小桩（临时，待新架构下完整重写）
// Test 项目编译需要此文件，但当前几何体绘制功能尚未迁移至新 API
// 桩实现委托基类 drawGrid / drawAllSeries 完成
#include "QCartesianGeometry.h"
#include <QDebug>

// 头文件声明了 Q_OBJECT，MOC 会自动生成元对象代码
// 构造函数委托基类
QCartesianGeometry::QCartesianGeometry(QObject* parent)
    : QChartGeometry(parent) {}

// coordinateSystem 在头文件 inline 实现（返回 Cartesian）
// 以下 stub 方法待删除——旧 API 残留
QPointF QCartesianGeometry::mapToPixel(qreal, qreal) const {
    qWarning() << "QCartesianGeometry::mapToPixel: deprecated, use DrawContext instead";
    return QPointF(0, 0);
}

QPointF QCartesianGeometry::mapFromPixel(const QPointF&) const {
    qWarning() << "QCartesianGeometry::mapFromPixel: deprecated";
    return QPointF(0, 0);
}
