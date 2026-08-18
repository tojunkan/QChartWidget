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
    QChartCamera.cpp
    QChartCamera3D.cpp
    QChartSeries3D.cpp
    QChartScatterSeries3D.cpp
    QChartLineSeries3D.cpp
    QChartSurfaceSeries.cpp
    QChartLayer3D.cpp
    QChartWidget3D.cpp
    QChartAxes3D.cpp
    QChartHitTester.cpp
    QChartGL.cpp
    QOpenGLChartRenderer.cpp
    QChartRenderer.cpp
    QPainterChartRenderer.cpp
    QChartAxis.cpp
    QChartLayer.cpp
    QChartSeries.cpp
    QChartWidget.cpp
    QValueAxis.cpp
    QLogAxis.cpp
    QDateTimeAxis.cpp
    QBarCategoryAxis.cpp
    QXYSeries.cpp
    QLineSeries.cpp
    QScatterSeries.cpp
    QPolygonSeries.cpp
    QBarSeries.cpp
    QRegionSeries.cpp
    QChartAnimation.cpp
    QNumericSeriesAnimation.cpp
    QBarAnimation.cpp
    QViewRectAnimation.cpp
    QProjectionSwitchAnimation.cpp
    QInterpolatedProjection.cpp
    QChartTheme.cpp
    QChartLegend.cpp
)

# ---- 库目标 ----
# 静态库：当前公共类没有 QCHARTWIDGET_EXPORT 导出宏，暂不支持 shared；
# 后续若要共享库，需给公共类加导出宏。
add_library(QChartWidget STATIC ${QCHART_SOURCES})
add_library(QChartWidget::QChartWidget ALIAS QChartWidget)

target_include_directories(QChartWidget PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
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
