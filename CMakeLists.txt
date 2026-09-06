# QChartWidget —— 阶段构建 CMake（S0：轴渲染管线，CPU + GL 双后端）
# 说明：原单文件 CMake 已改名 CMakeLists_OLD.txt 保留作参考。
# 阶梯策略（用户定案）：每阶段 QCHART_SOURCES 只含该阶段已迁移/验证的模块；
#   每阶段把已迁移模块加回；排除/加回清单记录在 docs/stages/S<阶段>_*.md。
# S0 子集 = 轴管线所需：axes（QChartAxis + 2D 轴）+ core 相机/渲染器（CPU+GL）+ QChartGL
#          + layers（QChartLayer，含少量迁移修复）+ projection 2D 族 + utils 头。
# 尚未纳入（后续阶段加回）：3D 系列/图层/Widget3D、QChartWidget、QChartHitTester、
#   动画、theme/legend/导出消费方。
cmake_minimum_required(VERSION 3.21)
project(QChartWidget VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- 构建选项（保持与旧版同名，默认开）----
option(BUILD_DEMOS "构建演示程序" OFF)          # S0 阶段 demo 未迁移，默认关
option(BUILD_BENCH "构建性能基准" OFF)          # S0 阶段 bench 未迁移，默认关
option(BUILD_UNIT_TESTS "构建单元测试" ON)
option(BUILD_INTEGRATION_TESTS "构建集成测试" ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg OpenGL OpenGLWidgets)

# ---- 测试注册 ----
# enable_testing() 必须位于顶层；Test/unit 与 Test/integration 内 add_test 由此生效。
enable_testing()

# ---- 库源码（S0 子集）----
set(QCHART_SOURCES
    # axes：轴基类 + 2D 轴（3D 轴 QChartAxes3D 待后续阶段）
    src/axes/QChartAxis.cpp
    src/axes/2d/QValueAxis.cpp
    src/axes/2d/QLogAxis.cpp
    src/axes/2d/QDateTimeAxis.cpp
    src/axes/2d/QBarCategoryAxis.cpp
    # core：2D 相机 + 双后端渲染器 + GL 资源池 + widget 容器层（批次 A 加入）
    src/core/QChartCamera.cpp
    src/core/QChartRenderer.cpp
    src/core/QPainterChartRenderer.cpp
    src/core/QOpenGLChartRenderer.cpp
    src/core/QChartGL.cpp
    src/core/QChartAbstractWidget.cpp
    src/core/QChartWidget.cpp
    # layers：2D 图层（3D 图层待后续阶段）
    src/layers/QChartLayer.cpp
    # projection：2D 投影族（3D 投影族为 header-only，随头文件引入即可；插值投影有 .cpp）
    src/projection/QInterpolatedProjection.cpp
)

# 库头文件（AUTOMOC 需要：含 Q_OBJECT 的类经此入 moc 扫描）
set(QCHART_HEADERS
    include/axes/QChartAxis.h
    include/axes/2d/QValueAxis.h
    include/axes/2d/QLogAxis.h
    include/axes/2d/QDateTimeAxis.h
    include/axes/2d/QBarCategoryAxis.h
    include/core/QChartAbstractCamera.h
    include/core/QChartCamera.h
    include/core/QChartPrimitive.h
    include/core/QChartScene.h
    include/core/QChartTextLabel.h
    include/core/QChartRenderer.h
    include/core/QPainterChartRenderer.h
    include/core/QOpenGLChartRenderer.h
    include/core/QChartGL.h
    include/core/QChartAbstractWidget.h
    include/core/QChartWidget.h
    include/layers/QChartLayer.h
    include/projection/QChartAbstractProjection.h
    include/projection/2d/QChartProjection.h
    include/projection/2d/QCartesianProjection.h
    include/projection/2d/QPolarProjection.h
    include/projection/2d/QFunctionalProjection.h
    include/projection/QInterpolatedProjection.h
    include/utils/QChartMath.h
    include/utils/QCube.h
    include/utils/QChartDebug.h
    include/utils/QDataPoint.h
    include/utils/QDataPoint3D.h
    include/utils/QDataRect.h
    include/utils/ProjectionToolKit.h
)

# ---- 库目标（moc 唯一持有者，红线不变）----
add_library(QChartWidget STATIC ${QCHART_SOURCES} ${QCHART_HEADERS})
add_library(QChartWidget::QChartWidget ALIAS QChartWidget)

target_include_directories(QChartWidget PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axes
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axes/2d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/layers
    ${CMAKE_CURRENT_SOURCE_DIR}/include/projection
    ${CMAKE_CURRENT_SOURCE_DIR}/include/projection/2d
    ${CMAKE_CURRENT_SOURCE_DIR}/include/series
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
)
target_link_libraries(QChartWidget PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::OpenGL Qt6::OpenGLWidgets)

set_target_properties(QChartWidget PROPERTIES AUTOMOC ON)

# ---- 测试（S0 阶段只构建新写的分期测试入口）----
if(BUILD_UNIT_TESTS OR BUILD_INTEGRATION_TESTS)
    add_subdirectory(Test)
endif()
