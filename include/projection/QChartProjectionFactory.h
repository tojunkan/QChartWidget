// QChartProjectionFactory.h —— 投影工厂
// 根据 CoordinateSystem 枚举创建对应 Projection 实例
// 也可通过 createFunctional 传入 lambda 创建自定义投影
#pragma once
#include "QChartAbstractProjection.h"
#include "QChartProjection.h"
#include "QCartesianProjection.h"
#include "QPolarProjection.h"
#include "QFunctionalProjection.h"

#include "QChartProjection3D.h"
#include "QCartesianProjection3D.h"
#include "QSphericalProjection3D.h"
#include "QCylindricalProjection3D.h"
#include "QFunctionalProjection3D.h"
#include <memory>
#include <functional>
#include <QDebug>
#include <QLoggingCategory>
#include "QChartDebug.h"

class QChartProjectionFactory {
public:
    /// <summary>
    /// 根据坐标系类型创建投影。Functional 类型不能通过此方法创建（缺少 lambda）
    /// </summary>
    static std::unique_ptr<QChartAbstractProjection> create(QChartAbstractProjection::CoordinateSystem type) {
        switch (type) {
        // ---- 2D ----
        case QChartAbstractProjection::CoordinateSystem::Cartesian:
            return std::make_unique<QCartesianProjection>();
        case QChartAbstractProjection::CoordinateSystem::Polar:
            return std::make_unique<QPolarProjection>();

        // ---- 3D ----
        case QChartAbstractProjection::CoordinateSystem::Cartesian3D:
            return std::make_unique<QCartesianProjection3D>();
        case QChartAbstractProjection::CoordinateSystem::Spherical:
            return std::make_unique<QSphericalProjection3D>();
        case QChartAbstractProjection::CoordinateSystem::Cylindrical:
            return std::make_unique<QCylindricalProjection3D>();

        case QChartAbstractProjection::CoordinateSystem::Functional:
            qWarning() << "QChartProjectionFactory: Cannot create QFunctionalProjection"
                          " without mapping functions — use createFunctional() instead";
            return nullptr;

        default:
            qWarning() << "QChartProjectionFactory: Unknown CoordinateSystem"
                       << static_cast<int>(type) << "— falling back to Cartesian";
            return std::make_unique<QCartesianProjection>();
        }
    }

    /// <summary>
    /// 创建用户自定义投影
    /// </summary>
    /// <param name="forward">Numeric (num0,num1) → View Cartesian (x,y)</param>
    /// <param name="backward">View Cartesian (x,y) → Numeric (num0,num1)，可选</param>
    /// <param name="defaultBounds">默认 Numeric 范围</param>
    /// <param name="dataToView">dataBounds → viewRect，可选（null=恒等）</param>
    /// <param name="viewToData">viewRect → dataBounds，可选（null=恒等）</param>
    static std::unique_ptr<QChartProjection> createFunctional(
        std::function<QPointF(qreal, qreal)> forward,
        std::function<QPointF(qreal, qreal)> backward = nullptr,
        QRectF defaultBounds = QRectF(0, 0, 10, 10),
        std::function<QRectF(const QRectF&)> dataToView = nullptr,
        std::function<QRectF(const QRectF&)> viewToData = nullptr,
        QString name0 = "x",
        QString name1 = "y")
    {
        qCDebug(logFactory) << "Creating QFunctionalProjection";
        return std::make_unique<QFunctionalProjection>(
            std::move(forward),
            std::move(backward),
            defaultBounds,
            std::move(dataToView),
            std::move(viewToData),
            name0,
            name1
        );
    }

};
