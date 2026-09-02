cmake_minimum_required(VERSION 3.21)
project(QChartWidget VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- 构建选项 ----
option(BUILD_DEMOS "构建演示程序" ON)
option(BUILD_BENCH "构建性能基准" ON)
option(BUILD_UNIT_TESTS "构建单元测试" ON)
option(BUILD_INTEGRATION_TESTS "构建集成测试" ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg OpenGL OpenGLWidgets)

# ---- 库源码（保持不变） ----
set(QCHART_SOURCES
    src/core/QChartCamera.cpp
    src/core/QChartCamera3D.cpp
    src/series/3d/QChartSeries3D.cpp
    src/series/3d/QChartScatterSeries3D.cpp
    src/series/3d/QChartLineSeries3D.cpp
    src/series/3d/QChartSurfaceSeries.cpp
    src/layers/3d/QChartLayer3D.cpp
    src/core/QChartWidget3D.cpp
    src/axes/3d/QChartAxes3D.cpp
    src/core/QChartHitTester.cpp
    src/core/QChartGL.cpp
    src/core/QOpenGLChartRenderer.cpp
    src/core/QChartRenderer.cpp
    src/core/QPainterChartRenderer.cpp
    src/axes/QChartAxis.cpp
    src/layers/QChartLayer.cpp
    src/series/QChartSeries.cpp
    src/core/QChartWidget.cpp
    src/axes/2d/QValueAxis.cpp
    src/axes/2d/QLogAxis.cpp
    src/axes/2d/QDateTimeAxis.cpp
    src/axes/2d/QBarCategoryAxis.cpp
    src/series/2d/QXYSeries.cpp
    src/series/2d/QLineSeries.cpp
    src/series/2d/QScatterSeries.cpp
    src/series/2d/QPolygonSeries.cpp
    src/series/2d/QBarSeries.cpp
    src/series/2d/QRegionSeries.cpp
    src/animation/QChartAnimation.cpp
    src/animation/QNumericSeriesAnimation.cpp
    src/animation/QBarAnimation.cpp
    src/animation/QViewRectAnimation.cpp
    src/animation/QProjectionSwitchAnimation.cpp
    src/projection/QInterpolatedProjection.cpp
    src/core/QChartTheme.cpp
    src/core/QChartLegend.cpp
)

set(QCHART_HEADERS
    include/animation/QBarAnimation.h
    include/animation/QChartAnimation.h
    include/animation/QNumericSeriesAnimation.h
    include/animation/QViewRectAnimation.h
    include/animation/QProjectionSwitchAnimation.h
    include/axes/QChartAxis.h
    include/axes/2d/QBarCategoryAxis.h
    include/axes/2d/QValueAxis.h
    include/axes/2d/QLogAxis.h
    include/axes/2d/QDateTimeAxis.h
    include/axes/3d/QChartAxes3D.h
    include/core/QChartWidget.h
    include/core/QChartWidget3D.h
    include/core/QChartCamera.h
    include/core/QChartCamera3D.h
    include/core/QChartTheme.h
    include/core/QChartLegend.h
    include/core/QChartScene.h
    include/core/QChartTextLabel.h
    include/core/QChartPrimitive.h
    include/core/QChartRenderer.h
    include/core/QPainterChartRenderer.h
    include/core/QOpenGLChartRenderer.h
    include/core/QChartGL.h
    include/core/QChartHitTester.h
    include/layers/QChartLayer.h
    include/layers/3d/QChartLayer3D.h
    include/series/QChartSeries.h
    include/series/2d/QXYSeries.h
    include/series/2d/QLineSeries.h
    include/series/2d/QScatterSeries.h
    include/series/2d/QPolygonSeries.h
    include/series/2d/QBarSeries.h
    include/series/2d/QRegionSeries.h
    include/series/3d/QChartSeries3D.h
    include/series/3d/QChartLineSeries3D.h
    include/series/3d/QChartScatterSeries3D.h
    include/series/3d/QChartSurfaceSeries.h
    include/utils/ProjectionToolKit.h
    include/utils/QChartMath.h
    include/utils/QChartDebug.h
    include/utils/QDataPoint.h
    include/utils/QDataPoint3D.h
    include/utils/QDataRect.h
    include/projection/QInterpolatedProjection.h
    include/projection/QChartProjectionFactory.h
    include/projection/QChartAbstractProjection.h
    include/projection/2d/QChartProjection.h
    include/projection/2d/QCartesianProjection.h
    include/projection/2d/QPolarProjection.h
    include/projection/2d/QFunctionalProjection.h
    include/projection/3d/QChartProjection3D.h
    include/projection/3d/QCartesianProjection3D.h
    include/projection/3d/QCylindricalProjection3D.h
    include/projection/3d/QSphericalProjection3D.h
    include/projection/3d/QFunctionalProjection3D.h
)

add_library(QChartWidget STATIC ${QCHART_SOURCES} ${QCHART_HEADERS}
    include/core/QChartAbstractCamera.h)
add_library(QChartWidget::QChartWidget ALIAS QChartWidget)

target_include_directories(QChartWidget PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/animation
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axes
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axes/2d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axes/3d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/layers
    ${CMAKE_CURRENT_SOURCE_DIR}/include/layers/2d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/layers/3d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/series
    ${CMAKE_CURRENT_SOURCE_DIR}/include/series/2d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/series/3d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/include/projection
    ${CMAKE_CURRENT_SOURCE_DIR}/include/projection/2d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/projection/3d
)
target_link_libraries(QChartWidget PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::OpenGL Qt6::OpenGLWidgets)
set_target_properties(QChartWidget PROPERTIES AUTOMOC ON)

# ---- 全部测试/演示程序统一下放到 Test/ ----
add_subdirectory(Test)