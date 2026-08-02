// QCartesianGeometry.h —— Cartesian 几何层（临桩，待新架构重写）
#pragma once
#include "QChartGeometry.h"

class QCartesianGeometry : public QChartGeometry {
    Q_OBJECT
public:
    explicit QCartesianGeometry(QObject* parent = nullptr);
    // coordinateSystem 改为非虚——由 Widget 通过 setCoordinateSystem() 同步

    // ── 以下为旧 API 残桩（待删除）──
    [[deprecated]] QPointF mapToPixel(qreal x, qreal y) const;
    [[deprecated]] QPointF mapFromPixel(const QPointF& p) const;
};
