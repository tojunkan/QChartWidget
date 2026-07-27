#pragma once
// QChartProjectionFactory.h
#pragma once
#include "QChartProjection.h"
#include "QCartesianProjection.h"
#include "QPolarProjection.h"
#include <memory>

class QChartProjectionFactory {
public:
    // 根据坐标系类型创建对应的投影
    static std::unique_ptr<QChartProjection> create(CoordinateSystem type) {
        switch (type) {
        case CoordinateSystem::Cartesian:
            return std::make_unique<QCartesianProjection>();
        case CoordinateSystem::Polar:
            return std::make_unique<QPolarProjection>();
        default:
            qWarning() << "Unsupported CoordinateSystem: " << static_cast<int>(type);
            return nullptr;
        }
    }
};