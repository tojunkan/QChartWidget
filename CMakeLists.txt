# QChartWidget —— 基于 QPainter 的图表库（五空间模型）
# 构建入口：CMake + Qt6。同一套源码可用 MSVC / MinGW / GCC 编译，跨 Windows / Linux / macOS。
#
# 目录职责：
#   根目录        库源码（QChartWidget / QChartAxis / QChartLayer / QChartSeries / QChartProjection 等）
#   Test/         演示程序（demos/ 下每个 buildDemoXxx 一个演示，test.cpp 统一入口）
#   TestUnit/     单元测试（QTest，tests/ 下每类轴一个测试）
#
# 用法（Windows，示例见 scripts/ 下的 .bat）：
#   MSVC:  vcvars64.bat && cmake -G Ninja -S . -B build -DCMAKE_PREFIX_PATH=E:/Qt/6.11.1/msvc2022_64
#   MinGW: cmake -G Ninja -S . -B build -DCMAKE_PREFIX_PATH=E:/Qt/6.11.0/mingw_64 -DCMAKE_CXX_COMPILER=...\g++.exe
cmake_minimum_required(VERSION 3.21)
project(QChartWidget VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- 构建选项 ----
option(BUILD_DEMOS "构建演示程序" ON)
option(BUILD_TESTS "构建单元测试" ON)
option(BUILD_BENCH "构建性能基准" ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg OpenGL OpenGLWidgets)

# ---- 库源码 ----
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

# ---- 库头文件（显式列出）----
# AUTOMOC 兜底：CMake 3.28 automoc 对"被源文件引用的头文件"仅解析相对源文件目录，
# 无法经 include 目录命中 include/<module>/ 子目录头（R1 实测），
# 故把全部头文件列入目标源，automoc 对列入源的头必扫（Q_OBJECT → moc，其余跳过）。
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
    include/projection/QChartProjection.h
    include/projection/QChartProjectionFactory.h
    include/projection/QCartesianProjection.h
    include/projection/QPolarProjection.h
    include/projection/QFunctionalProjection.h
    include/projection/QInterpolatedProjection.h
    include/projection/QChartProjection3D.h
    include/projection/QChartCartesianProjection3D.h
    include/projection/QChartCylindricalProjection3D.h
    include/projection/QChartSphericalProjection3D.h
    include/projection/QChartFunctionalProjection3D.h
)

# ---- 库目标 ----
# 静态库：当前公共类没有 QCHARTWIDGET_EXPORT 导出宏，暂不支持 shared；
# 后续若要共享库，需给公共类加导出宏。
add_library(QChartWidget STATIC ${QCHART_SOURCES} ${QCHART_HEADERS})
add_library(QChartWidget::QChartWidget ALIAS QChartWidget)

# include/ 根 + 各模块目录：扁平 #include "QChartXxx.h" 写法不变（头文件按模块放子目录，
# 编译器不递归 -I，故需把每个模块目录显式加入 include 路径；basename 全库唯一，无遮蔽风险）
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
)
target_link_libraries(QChartWidget PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::OpenGL Qt6::OpenGLWidgets)

# moc 的唯一持有者：库内所有 Q_OBJECT 类的 moc 在此生成并编入静态库，
# 消费目标（demo/test）一律关掉 AUTOMOC，避免重复 moc 导致链接符号冲突。
set_target_properties(QChartWidget PROPERTIES AUTOMOC ON)

# ---- 演示程序 ----
if(BUILD_DEMOS)
    add_executable(QChartDemo
        Test/test.cpp
        Test/demos/demo_polar.cpp
        Test/demos/demo_bar.cpp
        Test/demos/demo_pendulum.cpp
        Test/demos/demo_sort.cpp
        Test/demos/demo_camera.cpp
        Test/demos/demo_swirl.cpp
        Test/demos/demo_stress.cpp
        Test/demos/demo_theme.cpp
        Test/demos/demo_scatter3d.cpp
        Test/demos/demo_line3d.cpp
        Test/demos/demo_surface3d.cpp
    )
    # 演示源码不含 Q_OBJECT，moc 全在库内，故关闭 AUTOMOC
    set_target_properties(QChartDemo PROPERTIES AUTOMOC OFF)
    target_link_libraries(QChartDemo PRIVATE QChartWidget::QChartWidget)

    # 寻找 windeployqt（位于 Qt 的 bin 目录）
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${CMAKE_PREFIX_PATH}/bin)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET QChartDemo POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:QChartDemo>
            COMMENT "Deploying Qt dependencies for QChartDemo"
        )
    else()
        message(WARNING "windeployqt not found, skipping deployment")
    endif()
endif()

# ---- 单元测试 ----
if(BUILD_TESTS)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)

    set(QCHART_TEST_HEADERS
        TestUnit/tests/test_qvalueaxis.h
        TestUnit/tests/test_qbarcategoryaxis.h
        TestUnit/tests/test_qlogaxis.h
        TestUnit/tests/test_qdatetimeaxis.h
        TestUnit/tests/test_qchartcamera.h
        TestUnit/tests/test_qchartcamera3d.h
        TestUnit/tests/test_qchartaxes3d.h
        TestUnit/tests/test_qchartmath.h
        TestUnit/tests/test_qchartprojection.h
        TestUnit/tests/test_qchartsurface3d.h
        TestUnit/tests/test_qchartrenderer.h
        TestUnit/tests/test_qchartrenderer3d.h
        TestUnit/tests/test_hittest.h
        TestUnit/tests/test_qcharthittester.h
        TestUnit/tests/test_qcharttheme.h
        TestUnit/tests/test_qchartlegend.h
        TestUnit/tests/test_legend_interact.h
        TestUnit/tests/test_export.h
        TestUnit/tests/test_qchartgl.h
        TestUnit/tests/test_qopenglrenderer.h
    )

    # 测试类自身带 Q_OBJECT，但关闭 AUTOMOC 以免重复 moc 库头文件，
    # 改为仅对测试头做手动 moc。
    qt6_wrap_cpp(QCHART_TEST_MOCS ${QCHART_TEST_HEADERS})

    add_executable(QChartTests
        TestUnit/main.cpp
        TestUnit/tests/test_qvalueaxis.cpp
        TestUnit/tests/test_qbarcategoryaxis.cpp
        TestUnit/tests/test_qlogaxis.cpp
        TestUnit/tests/test_qdatetimeaxis.cpp
        TestUnit/tests/test_qchartcamera.cpp
        TestUnit/tests/test_qchartcamera3d.cpp
        TestUnit/tests/test_qchartaxes3d.cpp
        TestUnit/tests/test_qchartmath.cpp
        TestUnit/tests/test_qchartprojection.cpp
        TestUnit/tests/test_qchartsurface3d.cpp
        TestUnit/tests/test_qchartrenderer.cpp
        TestUnit/tests/test_qchartrenderer3d.cpp
        TestUnit/tests/test_hittest.cpp
        TestUnit/tests/test_qcharthittester.cpp
        TestUnit/tests/test_qcharttheme.cpp
        TestUnit/tests/test_qchartlegend.cpp
        TestUnit/tests/test_legend_interact.cpp
        TestUnit/tests/test_export.cpp
        TestUnit/tests/test_qchartgl.cpp
        TestUnit/tests/test_qopenglrenderer.cpp
        ${QCHART_TEST_MOCS}
    )
    set_target_properties(QChartTests PROPERTIES AUTOMOC OFF)
    target_include_directories(QChartTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/TestUnit/tests
    )
    target_link_libraries(QChartTests PRIVATE QChartWidget::QChartWidget Qt6::Test)

    enable_testing()
    add_test(NAME QChartTests COMMAND QChartTests)

    # 无头跑测试：QGuiApplication 需要平台插件。非 Windows（Linux CI/无 DISPLAY）
    # 用 offscreen 保证 ctest 可无头执行；Windows 有桌面，走默认平台（可能短暂弹窗）。
    if(NOT WIN32)
        set_tests_properties(QChartTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

# ---- 性能基准 ----
if(BUILD_BENCH)
    add_executable(QChartBench
        Test/bench/bench_main.cpp
    )
    # 基准源码不含 Q_OBJECT，moc 全在库内，故关闭 AUTOMOC
    set_target_properties(QChartBench PROPERTIES AUTOMOC OFF)
    target_link_libraries(QChartBench PRIVATE QChartWidget::QChartWidget)
endif()
